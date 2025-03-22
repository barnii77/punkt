#include "punkt/dot.hpp"
#include "punkt/gl_renderer.hpp"

#include <cassert>

using namespace punkt;

static void assertNonNull(void *p) {
    if (!p) {
        std::abort();
    }
}

GraphRenderer::GraphRenderer() = default;

GraphRenderer::~GraphRenderer() {
    if (m_graph_renderer) {
        delete static_cast<render::GLRenderer *>(m_graph_renderer);
    }
}

void GraphRenderer::initialize(const Digraph &dg, render::glyph::GlyphLoader &glyph_loader) {
    m_graph_renderer = new render::GLRenderer(dg, glyph_loader);
}

void GraphRenderer::notifyFramebufferSize(const int width, const int height) const {
    assertNonNull(m_graph_renderer);
    render::GLRenderer &glr = *static_cast<render::GLRenderer *>(m_graph_renderer);
    glr.notifyFramebufferSize(width, height);
}

void GraphRenderer::renderFrame() const {
    assertNonNull(m_graph_renderer);
    render::GLRenderer &glr = *static_cast<render::GLRenderer *>(m_graph_renderer);
    glr.renderFrame();
}

void GraphRenderer::updateZoom(const double factor, const double cursor_x, const double cursor_y, const double window_width, const double window_height) const {
    assertNonNull(m_graph_renderer);
    render::GLRenderer &glr = *static_cast<render::GLRenderer *>(m_graph_renderer);
    glr.updateZoom(factor, cursor_x, cursor_y, window_width, window_height);
}

void GraphRenderer::resetZoom() const {
    assertNonNull(m_graph_renderer);
    render::GLRenderer &glr = *static_cast<render::GLRenderer *>(m_graph_renderer);
    glr.resetZoom();
}

void GraphRenderer::notifyCursorMovement(const double dx, const double dy) const {
    assertNonNull(m_graph_renderer);
    render::GLRenderer &glr = *static_cast<render::GLRenderer *>(m_graph_renderer);
    glr.notifyCursorMovement(dx, dy);
}
