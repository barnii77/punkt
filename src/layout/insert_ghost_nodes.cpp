#include "punkt/dot.hpp"
#include "punkt/int_types.hpp"

#include <ranges>
#include <string>
#include <cassert>
#include <optional>

#include "punkt/utils.hpp"

using namespace punkt;

static std::string_view newGhostNode(Digraph &dg, const size_t rank, std::optional<std::string_view> color) {
    std::string name = std::string("@") + std::to_string(dg.m_n_ghost_nodes++);

    Attrs attrs = {{"@type", "ghost"}, {"shape", "none"}};
    if (color.has_value()) {
        attrs.insert_or_assign("fillcolor", color.value());
    } else {
        attrs.insert_or_assign("fillcolor", "black");
    }

    dg.m_generated_sources.push_front(std::move(name));
    Node ghost(dg.m_generated_sources.front(), std::move(attrs));
    ghost.m_render_attrs.m_rank = rank;
    ghost.m_render_attrs.m_is_ghost = true;
    assert(!dg.m_nodes.contains(ghost.m_name));
    const std::string_view ghost_name = ghost.m_name;
    dg.m_nodes.emplace(ghost_name, std::move(ghost));
    dg.m_rank_counts.at(rank)++;

    return ghost_name;
}

// if necessary, decomposes an edge into multiple other edges connecting ghost nodes
static void decomposeEdgeIfRequired(Digraph &dg, const std::string_view source_name, const std::string_view dest_name,
                                    const size_t edge_idx) {
    if (const auto rank_diff = static_cast<ssize_t>(
            dg.m_nodes.at(dest_name).m_render_attrs.m_rank - dg.m_nodes.at(source_name).m_render_attrs.m_rank);
        rank_diff != -1 && rank_diff != 1) {
        // must decompose edge into ghost node edges (therefore, the original edge becomes invisible)
        Attrs edge_attrs;
        // scope `edge` because I have to be *really* careful with any references here because of relocation
        {
            Edge &edge = dg.m_nodes.at(source_name).m_outgoing.at(edge_idx);
            edge.m_render_attrs.m_is_visible = false;
            edge_attrs = edge.m_attrs;
        }

        if (rank_diff == 0) {
            // special case handling for when two nodes are on the same rank
            const size_t rank = dg.m_nodes.at(source_name).m_render_attrs.m_rank;
            size_t ghost_rank;
            if (rank == 0) {
                ghost_rank = 1;
                if (dg.m_rank_counts.size() == 1) {
                    dg.m_rank_counts.resize(2);
                }
            } else if (rank == dg.m_rank_counts.size() - 1) {
                ghost_rank = dg.m_rank_counts.size() - 2;
            } else {
                // this way of deciding where to insert the ghost node will lead to alternating between inserting above
                // and below and therefore balances the edges cleanly
                const size_t position_seed = dg.m_rank_counts.at(rank - 1) + dg.m_rank_counts.at(rank) +
                                             dg.m_rank_counts.at(rank + 1);
                ghost_rank = position_seed % 2 ? rank - 1 : rank + 1;
            }

            const std::optional<std::string_view> color = getAttrTransformedOrDefault(
                dg.m_nodes.at(source_name).m_outgoing.at(edge_idx).m_attrs, "color", std::nullopt, std::optional);
            const std::string_view ghost_name = newGhostNode(dg, ghost_rank, color);
            // source -> ghost
            {
                Node &source = dg.m_nodes.at(source_name);
                source.m_outgoing.emplace_back(source.m_name, ghost_name, edge_attrs);
            }
            // ghost -> dest
            {
                const Node &dest = dg.m_nodes.at(dest_name);
                dg.m_nodes.at(ghost_name).m_outgoing.emplace_back(ghost_name, dest.m_name, edge_attrs);
            }
        } else {
            const int edge_dir = rank_diff < 0 ? -1 : 1;
            std::string_view prev_name = source_name;
            for (size_t rank = dg.m_nodes.at(source_name).m_render_attrs.m_rank + edge_dir;
                 rank != dg.m_nodes.at(dest_name).m_render_attrs.m_rank; rank += edge_dir) {
                std::optional<std::string_view> color;
                // again, isolate edge reference because it's a massive foot gun
                {
                    const Edge &edge = dg.m_nodes.at(source_name).m_outgoing.at(edge_idx);
                    color = getAttrTransformedOrDefault(edge.m_attrs, "color", std::nullopt, std::optional);
                }
                const std::string_view ghost_name = newGhostNode(dg, rank, color);
                dg.m_nodes.at(prev_name).m_outgoing.emplace_back(prev_name, ghost_name, edge_attrs);
                prev_name = ghost_name;
            }
            dg.m_nodes.at(prev_name).m_outgoing.emplace_back(prev_name, dest_name, edge_attrs);
        }
    }
}

void Digraph::insertGhostNodes() {
    auto keys = std::views::keys(m_nodes);
    for (const std::vector real_node_names(keys.begin(), keys.end());
         const std::string_view &name: real_node_names) {
        Node &node = m_nodes.at(name);
        const size_t n_edges = node.m_outgoing.size();
        for (size_t i = 0; i < n_edges; i++) {
            decomposeEdgeIfRequired(*this, node.m_name, node.m_outgoing[i].m_dest, i);
        }
    }
}
