#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace punkt::render::glyph {
struct GlyphCharInfo {
    char32_t c{};
    size_t font_size{};

    bool operator==(const GlyphCharInfo &) const;
};

struct GlyphCharInfoHasher {
    size_t operator()(const GlyphCharInfo& glyph) const noexcept;
};

struct Glyph {
    std::vector<uint8_t> m_texture;
    size_t m_width;
    size_t m_height;

    Glyph(std::vector<uint8_t> texture, size_t width, size_t height);
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
