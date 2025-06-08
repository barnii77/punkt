#pragma once

#include <glad/glad.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <variant>
#include <span>

#include "punkt/utils/utils.hpp"

namespace punkt::render::glyph {
struct GlyphCharInfo {
    char32_t c{};
    size_t font_size{};

    bool operator==(const GlyphCharInfo &) const;
};

struct GlyphCharInfoHasher {
    size_t operator()(const GlyphCharInfo &glyph) const noexcept;
};

struct GlyphMeta {
    size_t m_width, m_height;

    GlyphMeta(size_t width, size_t height);
};

struct Glyph {
    GLuint m_texture;
    GlyphMeta m_meta;

    Glyph(GLuint texture, size_t width, size_t height);

    ~Glyph();
};

enum class FontType: uint32_t {
    none = 0,
    psf1,
    ttf,
};

struct PSF1GlyphsT {
    std::unordered_map<char32_t, std::vector<uint8_t> > data;
    size_t base_width{};
    size_t base_height{};
};

// TODO vector font data
using GlyphLoaderFontDataT = std::variant<PSF1GlyphsT>;

class GlyphLoader {
    bool m_is_real_loader{}, m_in_load_mode{};
    FontType m_font_type{FontType::none};
    GLuint m_pre_load_mode_enter_framebuffer{}, m_render_framebuffer{}, m_quad_vao{};
    GLuint m_ttf_stencil_shader{}, m_ttf_cover_shader{};
    size_t m_max_allowed_font_size{1024};
    std::string m_font_path;
    std::string m_raw_font_data;
    std::unordered_map<GlyphCharInfo, GlyphMeta, GlyphCharInfoHasher> m_glyph_metas;
    LRUCache<GlyphCharInfo, Glyph, GlyphCharInfoHasher> m_loaded_glyphs;
    GlyphLoaderFontDataT m_font_data;
    // used for zero-initializing textures
    std::vector<uint8_t> m_zeros_buffer;

    void loadTTFGlyph(char32_t c, size_t font_size);

    void loadRasterGlyph(char32_t c, size_t font_size, std::span<const uint8_t> glyph_data, size_t glyph_width,
                         size_t glyph_height);

    void loadGlyph(char32_t c, size_t font_size);

    void parseFontData();

    void loadAndCompileShaders();

public:
    // avoids actually loading the glyph if it's not already loaded
    [[nodiscard]] GlyphMeta getGlyphMeta(char32_t c, size_t font_size);

    const Glyph &getGlyph(char32_t c, size_t font_size);

    explicit GlyphLoader(std::string font_path);

    // creates a fake glyph loader that returns all black patches (useful for testing)
    explicit GlyphLoader();

    void startLoading();

    void endLoading();
};

class FontNotFoundException final : std::exception {
    const std::string m_path;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit FontNotFoundException(std::string path);
};

class IllegalFontSizeException final : std::exception {
    const size_t m_font_size;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit IllegalFontSizeException(size_t font_size);
};

class FontParsingException final : std::exception {
    const std::string m_msg;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit FontParsingException(std::string msg);
};
}
