#include "punkt/glyph_loader/glyph_loader.hpp"

#include <glad/glad.h>

#include <string>

using namespace punkt::render::glyph;

GlyphMeta::GlyphMeta(const size_t width, const size_t height)
    : m_width(width), m_height(height) {
}

Glyph::Glyph(const GLuint texture, const size_t width, const size_t height)
    : m_texture(texture), m_meta(width, height) {
}

FontNotFoundException::FontNotFoundException(std::string path)
    : m_path(std::move(path)) {
}

IllegalFontSizeException::IllegalFontSizeException(const size_t font_size)
    : m_font_size(font_size) {
}

FontParsingException::FontParsingException(std::string msg)
    : m_msg(std::move(msg)) {
}


const char *FontNotFoundException::what() const noexcept {
    const std::string msg = std::string("Font not found: \"") + std::string(m_path);
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
}

const char *IllegalFontSizeException::what() const noexcept {
    const std::string msg = std::string("Requested font size not allowed: \"") + std::to_string(m_font_size);
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
}

const char *FontParsingException::what() const noexcept {
    const std::string msg = std::string("FontParsingException: ") + m_msg;
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
}
