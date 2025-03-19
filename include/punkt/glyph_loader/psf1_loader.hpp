#pragma once

#include "punkt/gl_renderer.hpp"

#include <string_view>

namespace punkt::render::glyph {
void parsePSF1(GlyphLoaderFontDataT &font_data, std::string_view raw_font_data);
}
