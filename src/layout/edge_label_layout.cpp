#include "punkt/dot.hpp"
#include "punkt/populate_glyph_quads_with_text.hpp"
#include "punkt/utils.hpp"
#include "punkt/int_types.hpp"

#include <ranges>
#include <cassert>
#include <array>

using namespace punkt;

static void offsetQuads(std::vector<GlyphQuad> &quads, const Vector2<size_t> offset) {
    for (GlyphQuad &gq: quads) {
        gq.m_left += offset.x;
        gq.m_top += offset.y;
        gq.m_right += offset.x;
        gq.m_bottom += offset.y;
    }
}

static Vector2<size_t> getLineCenter(const std::span<const Vector2<size_t>> trajectory, const bool is_spline) {
    const auto [x1u, y1u] = trajectory[1];
    const auto [x2u, y2u] = trajectory[2];
    const auto x1 = static_cast<ssize_t>(x1u), y1 = static_cast<ssize_t>(y1u), x2 = static_cast<ssize_t>(x2u), y2 =
            static_cast<ssize_t>(y2u);
    if (is_spline) {
        // TODO evaluate the spline function at t = 0.5
        const auto [dx, dy] = Vector2(static_cast<ssize_t>(x2) - static_cast<ssize_t>(x1),
                                      static_cast<ssize_t>(y2) - static_cast<ssize_t>(y1));
        const size_t x = static_cast<size_t>(std::max(x1 + dx / 2, 0ll));
        const size_t y = static_cast<size_t>(std::max(y1 + dy / 2, 0ll));
        return Vector2(x, y);
    } else {
        const auto [dx, dy] = Vector2(static_cast<ssize_t>(x2) - static_cast<ssize_t>(x1),
                                      static_cast<ssize_t>(y2) - static_cast<ssize_t>(y1));
        const size_t x = static_cast<size_t>(std::max(x1 + dx / 2, 0ll));
        const size_t y = static_cast<size_t>(std::max(y1 + dy / 2, 0ll));
        return Vector2(x, y);
    }
}

static Vector2<size_t> getTrajectoryEndPoint(const std::span<const Vector2<size_t>> trajectory, const bool get_min) {
    assert(trajectory.size() == 4);
    if (trajectory.front().y < trajectory.back().y) {
        if (get_min) {
            return trajectory.front();
        } else {
            return trajectory.back();
        }
    } else {
        if (get_min) {
            return trajectory.back();
        } else {
            return trajectory.front();
        }
    }
}

void Digraph::computeEdgeLabelLayouts(const render::glyph::GlyphLoader &glyph_loader) {
    for (Node &node: std::views::values(m_nodes)) {
        for (Edge &edge: node.m_outgoing) {
            if (!edge.m_render_attrs.m_is_visible || edge.m_render_attrs.m_trajectory.empty()) {
                assert(!edge.m_render_attrs.m_is_visible && edge.m_render_attrs.m_trajectory.empty());
                continue;
            }

            const Node &destination = m_nodes.at(edge.m_dest);
            size_t max_line_width{}, height{};
            const size_t font_size =
                    getAttrTransformedCheckedOrDefault(edge.m_attrs, "fontsize", 14, stringViewToSizeT);

            if (const std::string_view &label = getAttrOrDefault(edge.m_attrs, "label", ""); !label.empty()) {
                populateGlyphQuadsWithText(label, font_size, TextAlignment::center, glyph_loader,
                                           edge.m_render_attrs.m_label_quads, max_line_width, height);
                // TODO make the default value of `splines` true once they are done
                const Vector2 center = getLineCenter(edge.m_render_attrs.m_trajectory,
                                                     getAttrOrDefault(m_attrs, "splines", "false") != "false");
                const ssize_t x_dir_dependent_offset = destination.m_render_attrs.m_x < node.m_render_attrs.m_x
                                                                 ? -static_cast<ssize_t>(font_size)
                                                                 : font_size;
                const auto top_left = Vector2(center.x + x_dir_dependent_offset,
                                              center.y - height / 2);
                offsetQuads(edge.m_render_attrs.m_label_quads, top_left);
            }

            // TODO should the head and tail labels be placed at the rank border, not at the node border,
            // to reduce the probability of colliding with the arrows?
            for (const std::string_view label_type: {"headlabel", "taillabel"}) {
                if (const std::string_view &label_value = getAttrOrDefault(edge.m_attrs, label_type, "");
                    !label_value.empty()) {
                    std::vector<GlyphQuad> &glyph_quads = label_type == "headlabel"
                                                              ? edge.m_render_attrs.m_head_label_quads
                                                              : edge.m_render_attrs.m_tail_label_quads;
                    populateGlyphQuadsWithText(label_value, font_size, TextAlignment::center, glyph_loader, glyph_quads,
                                               max_line_width, height);
                    const ssize_t rank_diff = static_cast<ssize_t>(destination.m_render_attrs.m_rank) -
                                              static_cast<ssize_t>(node.m_render_attrs.m_rank);

                    ssize_t top, left;
                    const bool get_min = label_type == "headlabel" && rank_diff == -1 ||
                                         label_type == "taillabel" && rank_diff == 1;
                    const auto [end_x, end_y] = getTrajectoryEndPoint(edge.m_render_attrs.m_trajectory, get_min);
                    left = end_x;
                    top = end_y;
                    if (!get_min) {
                        top -= static_cast<ssize_t>(height);
                    }
                    const ssize_t x_dir_dependent_offset = destination.m_render_attrs.m_x < node.m_render_attrs.m_x
                                                                     ? -static_cast<ssize_t>(font_size)
                                                                     : font_size;
                    left += x_dir_dependent_offset;

                    const Vector2 top_left{
                        static_cast<size_t>(std::max(left, 0ll)), static_cast<size_t>(std::max(top, 0ll))
                    };
                    offsetQuads(glyph_quads, top_left);
                }
            }
        }
    }
}
