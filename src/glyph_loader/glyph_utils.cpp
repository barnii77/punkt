#include "punkt/glyph_loader/glyph_loader.hpp"

using namespace punkt::render::glyph;

Glyph::Glyph(std::vector<uint8_t> texture, const size_t width, const size_t height)
    : m_texture(std::move(texture)), m_width(width), m_height(height) {
}

FontNotFoundException::FontNotFoundException(std::string path)
    : m_path(std::move(path)) {
}

const char *FontNotFoundException::what() const noexcept {
    const std::string msg = std::string("Font not found: \"") + std::string(m_path);
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
}
