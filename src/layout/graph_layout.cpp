#include "punkt/dot.hpp"
#include "punkt/utils/utils.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/layout/populate_glyph_quads_with_text.hpp"

#include <ranges>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <string_view>

using namespace punkt;

constexpr auto default_rank_sep = static_cast<size_t>(static_cast<float>(DEFAULT_DPI) * 0.5f);
constexpr auto default_node_sep = static_cast<size_t>(static_cast<float>(DEFAULT_DPI) * 0.25f);

static void populateGraphLabelText(const Attrs &attrs, const TextAlignment label_ta,
                                   render::glyph::GlyphLoader &glyph_loader, std::vector<GlyphQuad> &out_label_quads,
                                   size_t &out_graph_label_width, size_t &out_graph_label_height,
                                   const RankDirConfig rank_dir) {
    if (const std::string_view graph_label = getAttrOrDefault(attrs, "label", ""); !graph_label.empty()) {
        const size_t graph_label_font_size = getAttrTransformedCheckedOrDefault(
            attrs, "fontsize", default_font_size, stringViewToSizeT);
        populateGlyphQuadsWithText(graph_label, graph_label_font_size, label_ta, glyph_loader, out_label_quads,
                                   out_graph_label_width, out_graph_label_height, rank_dir);
    }
}

void Digraph::computeGraphLayout(render::glyph::GlyphLoader &glyph_loader) {
    m_render_attrs.m_graph_x = 0;
    m_render_attrs.m_graph_y = 0;
    m_render_attrs.m_rank_sep = getAttrTransformedCheckedOrDefault(m_attrs, "ranksep", default_rank_sep,
                                                                   stringViewToSizeT);
    m_render_attrs.m_node_sep = getAttrTransformedCheckedOrDefault(m_attrs, "nodesep", default_node_sep,
                                                                   stringViewToSizeT);
    const TextAlignment label_ta =
            getAttrTransformedOrDefault(m_attrs, "labeljust", default_label_just, textAlignmentFromStr);
    const std::string_view label_loc = getAttrOrDefault(m_attrs, "labelloc", "T");

    // graphs by default don't have padding and a margin
    const float graph_border_padding = getAttrTransformedCheckedOrDefault(m_attrs, "pad", 0.0f, stringViewToFloat);
    const float graph_border_pen_width = getAttrTransformedCheckedOrDefault(
        m_attrs, "penwidth", 0.0f, stringViewToFloat);

    constexpr size_t dpi = DEFAULT_DPI; // TODO handle custom DPI settings
    m_render_attrs.m_border_thickness = static_cast<size_t>(
        graph_border_pen_width * static_cast<float>(dpi) / static_cast<float>(DEFAULT_DPI));

    size_t graph_label_width = 0, graph_label_height = 0, graph_body_start = 0, graph_label_y = 0;
    populateGraphLabelText(m_attrs, label_ta, glyph_loader, m_render_attrs.m_label_quads, graph_label_width,
                           graph_label_height, m_render_attrs.m_rank_dir);
    if ((caseInsensitiveEquals(label_loc, "T") || caseInsensitiveEquals(label_loc, "L")) && graph_label_height > 0) {
        graph_body_start += graph_label_height + m_render_attrs.m_rank_sep;
    }

    m_render_attrs.m_rank_render_attrs.resize(m_per_rank_orderings.size());

    // compute the rank heights
    for (const Node &node: std::views::values(m_nodes)) {
        RankRenderAttrs &rra = m_render_attrs.m_rank_render_attrs.at(node.m_render_attrs.m_rank);
        rra.m_rank_height = std::max(node.m_render_attrs.m_height, rra.m_rank_height);
    }

    // compute the rank widths
    for (size_t rank = 0; rank < m_per_rank_orderings.size(); rank++) {
        RankRenderAttrs &rra = m_render_attrs.m_rank_render_attrs.at(rank);
        for (const auto &rank_ordering = m_per_rank_orderings.at(rank);
             const std::string_view &name: rank_ordering) {
            const Node &node = m_nodes.at(name);
            assert(node.m_render_attrs.m_rank == rank);
            rra.m_rank_width += node.m_render_attrs.m_width + m_render_attrs.m_node_sep;
        }
        rra.m_rank_width -= m_render_attrs.m_node_sep;
    }

    // compute the rank y positions
    size_t y = m_render_attrs.m_graph_y + graph_body_start;
    for (size_t rank = 0; rank < m_per_rank_orderings.size(); rank++) {
        RankRenderAttrs &rra = m_render_attrs.m_rank_render_attrs.at(rank);
        rra.m_rank_y = y;
        y += rra.m_rank_height + m_render_attrs.m_rank_sep;
    }

    // compute graph dimensions
    if ((caseInsensitiveEquals(label_loc, "B") || caseInsensitiveEquals(label_loc, "R")) && graph_label_height > 0) {
        graph_label_y = y;
        y += m_render_attrs.m_rank_sep + graph_label_height;
    }
    const size_t inner_height = y - m_render_attrs.m_rank_sep;
    const size_t y_padding = m_render_attrs.m_border_thickness + static_cast<size_t>(
                                 graph_border_padding * static_cast<float>(inner_height)); // h + pad + 2*border;
    m_render_attrs.m_graph_height = inner_height + 2 * y_padding;
    graph_label_y += y_padding;
    const size_t inner_width = std::ranges::max_element(
        m_render_attrs.m_rank_render_attrs,
        [](const RankRenderAttrs &rra1, const RankRenderAttrs &rra2) {
            return rra1.m_rank_width < rra2.m_rank_width;
        })->m_rank_width;
    m_render_attrs.m_graph_width = inner_width + 2 * m_render_attrs.m_border_thickness + static_cast<size_t>(
                                       graph_border_padding * static_cast<float>(inner_width)); // w + pad + 2*border

    // position graph label
    const size_t graph_label_right_just = m_render_attrs.m_graph_x +
                                          (m_render_attrs.m_graph_width - graph_label_width) / 2;
    for (GlyphQuad &gq: m_render_attrs.m_label_quads) {
        gq.m_left += graph_label_right_just;
        gq.m_top += graph_label_y;
        gq.m_right += graph_label_right_just;
        gq.m_bottom += graph_label_y;
    }

    // center each rank now that we know the graph dimensions
    for (RankRenderAttrs &rra: m_render_attrs.m_rank_render_attrs) {
        rra.m_rank_x = m_render_attrs.m_graph_x + (m_render_attrs.m_graph_width - rra.m_rank_width) / 2;
        rra.m_rank_y += y_padding;
    }

    // compute the x and y positions of each node in each rank (we need rank x/y because node x/y are absolute)
    for (size_t rank = 0; rank < m_per_rank_orderings.size(); rank++) {
        const RankRenderAttrs &rra = m_render_attrs.m_rank_render_attrs.at(rank);
        size_t x = rra.m_rank_x;
        for (const auto &rank_ordering = m_per_rank_orderings.at(rank);
             const std::string_view &name: rank_ordering) {
            Node &node = m_nodes.at(name);

            node.m_render_attrs.m_x = x;
            x += node.m_render_attrs.m_width + m_render_attrs.m_node_sep;

            const size_t node_vertical_centering_adjustment = (rra.m_rank_height - node.m_render_attrs.m_height) / 2;
            node.m_render_attrs.m_y = rra.m_rank_y + node_vertical_centering_adjustment;
        }
    }
}
