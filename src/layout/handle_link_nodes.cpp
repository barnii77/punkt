#include "punkt/dot.hpp"

#include <ranges>
#include <cassert>

using namespace punkt;

constexpr std::string_view link_attr = "@link";
constexpr std::string_view type_attr = "@type";

void Digraph::fuseClusterLinksIntoClusterSuperNodes() {
    std::vector<std::string_view> nodes_to_iter;
    for (const Node &node: std::views::values(m_nodes)) {
        if (!node.m_attrs.contains(link_attr)) {
            continue;
        }
        if (const std::string_view ref = node.m_attrs.at(link_attr); !ref.starts_with("cluster_")) {
            continue;
        }
        nodes_to_iter.emplace_back(node.m_name);
    }

    for (const auto &key: std::views::keys(m_clusters)) {
        const std::string new_node_name = "@" + std::string(key);
        m_nodes.emplace(new_node_name, Node{new_node_name, {}});
    }

    // insert cluster nodes
    for (const auto node_name: nodes_to_iter) {
        Node &node = m_nodes.at(node_name);
        const std::string_view ref = node.m_attrs.at(link_attr);
        const std::string new_node_name = "@" + std::string(ref);
        assert(m_nodes.contains(new_node_name));
        // TODO I possibly need to attach an attr here which indicates where it was originally pointing to
        for (const auto &edge: node.m_ingoing) {
            assert(edge.get().m_dest == node.m_name);
            edge.get().m_dest = m_nodes.at(new_node_name).m_name;
        }
        // TODO I possibly need to attach an attr here which indicates where it was originally coming from
        for (auto &edge: node.m_outgoing) {
            assert(edge.m_source == node.m_name);
            edge.m_source = m_nodes.at(new_node_name).m_name;
        }
        auto &new_node_outgoing = m_nodes.at(new_node_name).m_outgoing;
        new_node_outgoing.reserve(new_node_outgoing.size() + node.m_outgoing.size());
        new_node_outgoing.insert(new_node_outgoing.end(), std::make_move_iterator(node.m_outgoing.begin()),
                                 std::make_move_iterator(node.m_outgoing.end()));
    }

    // delete @link=cluster_N nodes
    for (auto it = m_nodes.begin(); it != m_nodes.end();) {
        if (it->second.m_attrs.contains(link_attr) && it->second.m_attrs.at(link_attr).starts_with("cluster_")) {
            it = m_nodes.erase(std::move(it));
        } else {
            ++it;
        }
    }
}

void Digraph::convertParentLinksToIOPorts(const std::string_view id_in_parent) {
    if (!m_parent) {
        return;
    }

    // TODO make sure to only create an entry in m_per_rank_orderings and m_rank_counts if necessary, i.e. if there are
    // io ports in that direction.. otherwise prob segfault in reorder_nodes_horiz.cpp
    // TODO I must split parent links into IO nodes, which are like ghost nodes: one ingoing, one outgoing
    // TODO populate node.m_is_io_port
    // TODO I have to delete parent link nodes but at the same time I have to be able to figure out who was connecting
    // to me from the parent graph
    assert(m_per_rank_orderings.empty());
    const Digraph &parent = *m_parent;
    const Node &super_node = parent.m_nodes.at(id_in_parent);
    for (const Node &node: std::views::values(m_nodes)) {
        if (!node.m_render_attrs.m_is_io_port) {
            continue;
        }
        assert(node.m_outgoing.size() == 1 && node.m_ingoing.size() == 1);
        const Edge &edge_out = *node.m_outgoing.begin();
        const Edge &edge_in = node.m_ingoing.begin()->get();
        const Node &io_source = m_nodes.at(edge_in.m_source);
        const Node &io_dest = m_nodes.at(edge_out.m_dest);
        const Node *external_node;
        if (io_source.m_attrs.contains(link_attr)) {
            assert(io_source.m_attrs.at(link_attr) == "parent");
            external_node = &io_source;
        } else {
            assert(io_dest.m_attrs.at(link_attr) == "parent");
            external_node = &io_dest;
        }

        // TODO use super_node to determine whether to insert it at the top or bottom
    }

    // delete @link=parent nodes
    for (auto it = m_nodes.begin(); it != m_nodes.end();) {
        if (it->second.m_attrs.contains(link_attr)) {
            assert(it->second.m_attrs.at(link_attr) == "parent");
            it = m_nodes.erase(std::move(it));
        } else {
            ++it;
        }
    }
}
