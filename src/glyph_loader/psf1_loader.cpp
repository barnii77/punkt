#include "punkt/utils/int_types.hpp"
#include "punkt/glyph_loader/psf1_loader.hpp"

#include <vector>
#include <unordered_map>
#include <cassert>
#include <cstring>

using namespace punkt::render::glyph;

constexpr size_t glyph_width = 8; // glyph width is always 8 pixels in this format
constexpr size_t n_glyphs_per_mode_step = 256;

struct Header {
    // has to be an array because I don't want to assume host endianness (though it's almost certainly little)
    uint8_t m_magic[2];
    uint8_t m_mode;
    uint8_t m_height;
};

using GlyphData = std::vector<uint8_t>;

void punkt::render::glyph::parsePSF1(GlyphLoaderFontDataT &font_data, std::string_view raw_font_data) {
    assert(raw_font_data.size() >= sizeof(Header));
    Header h{};
    std::memcpy(&h, raw_font_data.data(), sizeof(h));
    assert(h.m_magic[0] == 0x36 && h.m_magic[1] == 0x04);

    auto &psf1_glyphs = std::get<PSF1GlyphsT>(font_data);

    psf1_glyphs.base_width = glyph_width;
    psf1_glyphs.base_height = h.m_height;

    // if the LSB of the mode byte is set, this means it's a font with 512 glyphs instead of 256
    const size_t n = n_glyphs_per_mode_step * (h.m_mode & 0x1 ? 2 : 1);

    raw_font_data = raw_font_data.substr(sizeof(Header), raw_font_data.size() - sizeof(Header));
    assert(raw_font_data.size() >= n * psf1_glyphs.base_height);

    for (char32_t c = 0; c < n; c++) {
        std::vector<uint8_t> glyph;
        glyph.reserve(psf1_glyphs.base_width * psf1_glyphs.base_height);

        for (size_t i = 0; i < psf1_glyphs.base_height; i++) {
            const uint8_t pix_row = raw_font_data.at(i);
            for (int j = 7; j >= 0; j--) {
                glyph.emplace_back(pix_row & (1 << j) ? 255 : 0);
            }
        }
        psf1_glyphs.data.insert_or_assign(c, std::move(glyph));

        // advance string_view by removing consumed data
        raw_font_data = raw_font_data.substr(psf1_glyphs.base_height,
                                             raw_font_data.size() - psf1_glyphs.base_height);
    }
}
