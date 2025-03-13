#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <stdexcept>

namespace render::glyph {
#pragma pack(push, 1)
struct Pixel {
    uint8_t r, g, b, a;

    explicit Pixel();

    Pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
};
#pragma pack(pop)

struct GlyphCharInfo {
    char32_t c;
    size_t font_size;

    bool operator==(const GlyphCharInfo &) const;
};

struct GlyphCharInfoHasher {
    size_t operator()(const GlyphCharInfo& glyph) const noexcept;
};

struct Glyph {
    std::vector<Pixel> m_texture;
    size_t m_width;
    size_t m_height;

    Glyph(std::vector<Pixel> texture, size_t width, size_t height);
};

class GlyphLoader {
    bool m_is_real_loader{};
    std::string m_font_path;
    std::string m_raw_font_data;
    std::unordered_map<GlyphCharInfo, Glyph, GlyphCharInfoHasher> m_loaded_glyphs;

    void loadGlyph(char32_t c, size_t font_size);

    void parseFontData();

public:
    const Glyph &getGlyph(char32_t c, size_t font_size);

    explicit GlyphLoader(std::string font_path);

    // creates a fake glyph loader that returns all black patches (useful for testing)
    explicit GlyphLoader();
};

class FontNotFoundException final : std::exception {
    const std::string m_path;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit FontNotFoundException(std::string path);
};
}
