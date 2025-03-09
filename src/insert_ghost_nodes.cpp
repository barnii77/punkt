#include "punkt/dot.hpp"

#include <cstdint>
#include <ranges>
#include <string>
#include <cassert>

using namespace dot;

static std::string_view newGhostNode(Digraph &dg, const size_t rank) {
    const std::string name = std::string("@") + std::to_string(dg.m_n_ghost_nodes++);
    const Attrs attrs = {{"@type", "ghost"}};
    dg.m_generated_sources.push_front(std::move(name));
    Node ghost(dg.m_generated_sources.front(), attrs);
    ghost.m_render_attrs.m_rank = rank;
    ghost.m_render_attrs.m_is_ghost = true;
    assert(!dg.m_nodes.contains(ghost.m_name));
    const std::string_view ghost_name = ghost.m_name;
    dg.m_nodes.emplace(ghost_name, std::move(ghost));
    dg.m_rank_counts.at(rank)++;
    return ghost_name;
}

// if necessary, decomposes an edge into multiple other edges connecting ghost nodes
static void decomposeEdgeIfRequired(Digraph &dg, Edge &edge) {
    Node &source = dg.m_nodes.at(edge.m_source);
    Node &dest = dg.m_nodes.at(edge.m_dest);
    if (const ssize_t rank_diff = dest.m_render_attrs.m_rank - source.m_render_attrs.m_rank;
        rank_diff != -1 && rank_diff != 1) {
        // must decompose edge into ghost node edges (therefore, the original edge becomes invisible)
        edge.m_render_attrs.m_is_visible = false;
        Attrs edge_attrs = edge.m_attrs;

        if (rank_diff == 0) {
            // special case handling for when two nodes are on the same rank
            const size_t rank = source.m_render_attrs.m_rank;
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
                const size_t position_seed = dg.m_rank_counts.at(rank - 1) + dg.m_rank_counts.at(rank) + dg.m_rank_counts.at(rank + 1);
                ghost_rank = position_seed % 2 ? rank - 1 : rank + 1;
            }
            const std::string_view ghost_name = newGhostNode(dg, ghost_rank);
            source.m_outgoing.emplace_back(source.m_name, ghost_name, edge_attrs);
            dg.m_nodes.at(ghost_name).m_outgoing.emplace_back(ghost_name, dest.m_name, edge_attrs);
        } else {
            const int edge_dir = rank_diff < 0 ? -1 : 1;
            std::string_view prev_name = source.m_name;
            for (size_t rank = source.m_render_attrs.m_rank + edge_dir; rank != dest.m_render_attrs.m_rank;
                 rank += edge_dir) {
                const std::string_view ghost_name = newGhostNode(dg, rank);
                dg.m_nodes.at(prev_name).m_outgoing.emplace_back(prev_name, ghost_name, edge_attrs);
                prev_name = ghost_name;
            }
            dg.m_nodes.at(prev_name).m_outgoing.emplace_back(prev_name, dest.m_name, edge_attrs);
        }
    }
}

void Digraph::insertGhostNodes() {
    for (Node &node: std::views::values(m_nodes)) {
        for (Edge &edge: node.m_outgoing) {
            decomposeEdgeIfRequired(*this, edge);
        }
    }
}
