#include "punkt/dot.hpp"
#include "punkt/utils/int_types.hpp"
#include "punkt/utils/utils.hpp"

#include <ranges>
#include <string>
#include <cassert>
#include <vector>
#include <utility>
#include <algorithm>
#include <optional>

using namespace punkt;

static std::string_view newGhostNode(Digraph &dg, const size_t rank, std::optional<std::string_view> color) {
    std::string name = std::string("@") + std::to_string(dg.m_n_ghost_nodes++);

    Attrs attrs = {{"@type", "ghost"}, {"shape", "none"}};
    if (color.has_value()) {
        attrs.insert_or_assign("fillcolor", color.value());
    } else {
        attrs.insert_or_assign("fillcolor", "black");
    }

    dg.m_referenced_sources.push_front(std::move(name));
    Node ghost(dg.m_referenced_sources.front(), std::move(attrs));
    ghost.m_render_attrs.m_rank = rank;
    ghost.m_render_attrs.m_is_ghost = true;
    assert(!dg.m_nodes.contains(ghost.m_name));
    const std::string_view ghost_name = ghost.m_name;
    dg.m_nodes.emplace(ghost_name, std::move(ghost));
    dg.m_rank_counts.at(rank)++;

    return ghost_name;
}

static Attrs getGhostEdgeAttrs(const Attrs &edge_attrs, const size_t edge_num, const size_t n_total_edges) {
    assert(n_total_edges > 1);
    Attrs out = edge_attrs;

    // these are not inherited from original edge to ghost edges
    constexpr std::string_view label = "label";
    constexpr std::string_view headlabel = "headlabel";
    constexpr std::string_view taillabel = "taillabel";
    out.erase(label);
    out.erase(headlabel);
    out.erase(taillabel);

    if (edge_num == 0 && edge_attrs.contains(taillabel)) {
        out.insert_or_assign(taillabel, edge_attrs.at(taillabel));
    }

    if (edge_num == n_total_edges / 2 && edge_attrs.contains(label)) {
        if (edge_num * 2 == n_total_edges) {
            // even number of total edges - middle is at the beginning of ghost edge
            out.insert_or_assign(taillabel, edge_attrs.at(label));
        } else {
            // odd number of total edges - middle is in the middle ghost edge
            out.insert_or_assign(label, edge_attrs.at(label));
        }
    }

    if (edge_num == n_total_edges - 1 && edge_attrs.contains(headlabel)) {
        out.insert_or_assign(headlabel, edge_attrs.at(headlabel));
    }

    return out;
}

// if necessary, decomposes an edge into multiple other edges connecting ghost nodes
static void decomposeEdgeIfRequired(Digraph &dg, const std::string_view source_name, const std::string_view dest_name,
                                    const size_t edge_idx, const bool has_top_io_port, const bool has_bottom_io_port) {
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
            // Special case handling for when two nodes are on the same rank
            const size_t rank = dg.m_nodes.at(source_name).m_render_attrs.m_rank;
            size_t ghost_rank;
            if (rank == 0) {
                ghost_rank = 1;
                if (dg.m_rank_counts.size() == 1) {
                    assert(!has_top_io_port && !has_bottom_io_port);
                    dg.m_rank_counts.resize(2);
                } else if (dg.m_io_port_ranks.contains(ghost_rank)) {
                    // Handle the very particular case where I have one rank with self-links and the rank *below*, which
                    // is the only other rank, is an IO port rank and must not be used for ghost node placement.
                    // Fix: Insert a rank below the current rank to act as a ghost rank.
                    assert(dg.m_io_port_ranks.size() == 1 && has_bottom_io_port && dg.m_rank_counts.size() == 2);
                    dg.m_rank_counts.resize(3);
                    std::swap(dg.m_rank_counts[1], dg.m_rank_counts[2]);
                    for (Node &n: std::views::values(dg.m_nodes)) {
                        if (n.m_render_attrs.m_rank == ghost_rank) {
                            n.m_render_attrs.m_rank++;
                        }
                    }
                }
            } else if (rank == dg.m_rank_counts.size() - 1) {
                assert(dg.m_rank_counts.size() >= 2);
                ghost_rank = dg.m_rank_counts.size() - 2;
                if (dg.m_io_port_ranks.contains(ghost_rank)) {
                    // Handle the very particular case where I have one rank with self-links and the rank *above*, which
                    // is the only other rank, is an IO port rank and must not be used for ghost node placement.
                    // Fix: Insert a rank below the current rank to act as a ghost rank.
                    assert(rank == 1 && dg.m_rank_counts.size() == 2);
                    dg.m_rank_counts.resize(dg.m_rank_counts.size() + 1);
                    ghost_rank = dg.m_rank_counts.size() - 1;
                }
            } else {
                // This way of deciding where to insert the ghost node will lead to alternating between inserting above
                // and below and therefore balances the edges cleanly
                const size_t position_seed = dg.m_rank_counts.at(rank - 1) + dg.m_rank_counts.at(rank) +
                                             dg.m_rank_counts.at(rank + 1);
                const ssize_t direction = position_seed % 2 ? -1 : 1;
                ghost_rank = rank + direction;
                if (dg.m_io_port_ranks.contains(ghost_rank)) {
                    ghost_rank = rank - direction;
                    if (dg.m_io_port_ranks.contains(ghost_rank)) {
                        // Handle the very particular case where I have three ranks, rank is 1 (in the middle) and above
                        // and below are both IO ranks not suitable for ghost node placement.
                        // Fix: Insert a rank below the current rank to act as a ghost rank.
                        assert(
                            rank == 1 && dg.m_io_port_ranks.contains(rank - 1) && dg.m_io_port_ranks.contains(rank + 1)
                            && dg.m_io_port_ranks.size() == 2 && dg.m_rank_counts.size() == 3);
                        ghost_rank = 2;
                        dg.m_rank_counts.resize(4);
                        std::swap(dg.m_rank_counts[2], dg.m_rank_counts[3]);
                        for (Node &n: std::views::values(dg.m_nodes)) {
                            if (n.m_render_attrs.m_rank == ghost_rank) {
                                n.m_render_attrs.m_rank++;
                            }
                        }
                    }
                }
            }

            const std::optional<std::string_view> color = getAttrTransformedOrDefault(
                dg.m_nodes.at(source_name).m_outgoing.at(edge_idx).m_attrs, "color", std::nullopt, std::optional);
            const std::string_view ghost_name = newGhostNode(dg, ghost_rank, color);
            // source -> ghost
            {
                Node &source = dg.m_nodes.at(source_name);
                source.m_outgoing.emplace_back(source.m_name, ghost_name, getGhostEdgeAttrs(edge_attrs, 0, 2));
                source.m_outgoing.back().m_render_attrs.m_is_part_of_self_connection = true;
            }
            // ghost -> dest
            {
                const Node &dest = dg.m_nodes.at(dest_name);
                Node &ghost = dg.m_nodes.at(ghost_name);
                ghost.m_outgoing.emplace_back(ghost_name, dest.m_name, getGhostEdgeAttrs(edge_attrs, 1, 2));
                ghost.m_outgoing.back().m_render_attrs.m_is_part_of_self_connection = true;
            }
        } else {
            const int edge_dir = rank_diff < 0 ? -1 : 1;
            const size_t n_total_edges = rank_diff < 0 ? -rank_diff : rank_diff;
            std::string_view prev_name = source_name;
            size_t edge_num = 0;
            for (size_t rank = dg.m_nodes.at(source_name).m_render_attrs.m_rank + edge_dir;
                 rank != dg.m_nodes.at(dest_name).m_render_attrs.m_rank; rank += edge_dir, edge_num++) {
                std::optional<std::string_view> color;
                // again, isolate edge reference because it's a massive foot gun
                {
                    const Edge &edge = dg.m_nodes.at(source_name).m_outgoing.at(edge_idx);
                    color = getAttrTransformedOrDefault(edge.m_attrs, "color", std::nullopt, std::optional);
                }
                const std::string_view ghost_name = newGhostNode(dg, rank, color);
                dg.m_nodes.at(prev_name).m_outgoing.emplace_back(prev_name, ghost_name,
                                                                 getGhostEdgeAttrs(
                                                                     edge_attrs, edge_num, n_total_edges));
                prev_name = ghost_name;
            }
            dg.m_nodes.at(prev_name).m_outgoing.emplace_back(prev_name, dest_name,
                                                             getGhostEdgeAttrs(edge_attrs, edge_num, n_total_edges));
        }
    }
}

void Digraph::insertGhostNodes() {
    bool has_top_io_port = m_io_port_ranks.contains(0);
    bool has_bottom_io_port = m_io_port_ranks.size() == 2 || m_io_port_ranks.size() == 1 && !m_io_port_ranks.
                              contains(0);

    auto keys = std::views::keys(m_nodes);
    for (const std::vector real_node_names(keys.begin(), keys.end());
         const std::string_view &name: real_node_names) {
        Node &node = m_nodes.at(name);
        const size_t n_edges = node.m_outgoing.size();
        for (size_t i = 0; i < n_edges; i++) {
            decomposeEdgeIfRequired(*this, node.m_name, node.m_outgoing[i].m_dest, i, has_top_io_port,
                                    has_bottom_io_port);
        }
    }
}
