#include "punkt/dot.hpp"
#include "punkt/utils.hpp"
#include "punkt/dot_constants.hpp"

#include <ranges>
#include <algorithm>
#include <cassert>

using namespace punkt;

constexpr size_t default_rank_sep = static_cast<size_t>(static_cast<float>(DEFAULT_DPI) * 0.5f);
constexpr size_t default_node_sep = static_cast<size_t>(static_cast<float>(DEFAULT_DPI) * 0.25f);

void Digraph::computeGraphLayout() {
    m_render_attrs.m_rank_sep = m_attrs.contains("ranksep")
                                    ? stringViewToSizeT(m_attrs.at("ranksep"), "ranksep")
                                    : default_rank_sep;
    m_render_attrs.m_node_sep = m_attrs.contains("nodesep")
                                    ? stringViewToSizeT(m_attrs.at("nodesep"), "nodesep")
                                    : default_node_sep;

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
    size_t y = 0;
    for (size_t rank = 0; rank < m_per_rank_orderings.size(); rank++) {
        RankRenderAttrs &rra = m_render_attrs.m_rank_render_attrs.at(rank);
        rra.m_rank_y = y;
        y += rra.m_rank_height + m_render_attrs.m_rank_sep;
    }

    // compute graph dimensions
    m_render_attrs.m_graph_height = y - m_render_attrs.m_rank_sep;
    m_render_attrs.m_graph_width = std::ranges::max_element(
        m_render_attrs.m_rank_render_attrs,
        [](const RankRenderAttrs &rra1, const RankRenderAttrs &rra2) {
            return rra1.m_rank_width < rra2.m_rank_width;
        })->m_rank_width;

    // center each rank now that we know the graph dimensions
    for (RankRenderAttrs &rra: m_render_attrs.m_rank_render_attrs) {
        rra.m_rank_x = (m_render_attrs.m_graph_width - rra.m_rank_width) / 2;
    }

    // finally, compute the x and y positions of each node in each rank (we need rank x/y because node x/y are absolute)
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
