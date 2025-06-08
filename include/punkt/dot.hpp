#pragma once

#include "punkt/dot_tokenizer.hpp"
#include "punkt/glyph_loader/glyph_loader.hpp"

#include <string>
#include <forward_list>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace punkt {
using Attrs = std::unordered_map<std::string_view, std::string_view>;

struct GlyphQuad {
    size_t m_left, m_top, m_right, m_bottom;
    render::glyph::GlyphCharInfo m_c;

    GlyphQuad(size_t left, size_t top, size_t right, size_t bottom, render::glyph::GlyphCharInfo c);
};

template<typename T>
struct Vector2 {
    T x;
    T y;

    bool operator==(const Vector2 &other) const = default;
};

struct EdgeRenderAttrs {
    std::vector<Vector2<size_t> > m_trajectory;
    std::vector<GlyphQuad> m_label_quads;
    std::vector<GlyphQuad> m_head_label_quads;
    std::vector<GlyphQuad> m_tail_label_quads;
    bool m_is_visible{}, m_is_part_of_self_connection{}, m_is_spline{};

    explicit EdgeRenderAttrs();
};

struct Edge {
    std::string_view m_source;
    std::string_view m_dest;
    Attrs m_attrs;
    EdgeRenderAttrs m_render_attrs{};

    Edge(std::string_view source, std::string_view destination);

    Edge(std::string_view source, std::string_view destination, Attrs attrs);
};

struct NodeRenderAttrs {
    size_t m_rank{}, m_width{}, m_height{}, m_border_thickness{}, m_x{}, m_y{};
    float m_barycenter_x{};
    bool m_is_ghost{}, m_is_io_port{};
    std::vector<GlyphQuad> m_quads;

    explicit NodeRenderAttrs();
};

struct RankDirConfig {
    bool m_is_sideways{}, m_is_reversed{};
};

struct Node {
    std::string_view m_name;
    std::vector<std::reference_wrapper<Edge> > m_ingoing;
    std::vector<Edge> m_outgoing;
    Attrs m_attrs;
    NodeRenderAttrs m_render_attrs{};

    Node(std::string_view name, Attrs attrs);

    void populateRenderInfo(render::glyph::GlyphLoader &glyph_loader, RankDirConfig rank_dir);
};

struct RankRenderAttrs {
    size_t m_rank_x{}, m_rank_y{}, m_rank_width{}, m_rank_height{};
};

struct DigraphRenderAttrs {
    size_t m_rank_sep{}, m_node_sep{}, m_graph_width{}, m_graph_height{}, m_graph_x{}, m_graph_y{},
            m_border_thickness{};
    RankDirConfig m_rank_dir{};
    std::vector<RankRenderAttrs> m_rank_render_attrs;
    std::vector<GlyphQuad> m_label_quads;
};

struct Digraph;

struct GraphRenderer {
    void *m_graph_renderer{};

    explicit GraphRenderer();

    ~GraphRenderer();

    void initialize(const Digraph &dg, render::glyph::GlyphLoader &glyph_loader);

    void notifyFramebufferSize(int width, int height) const;

    void renderFrame() const;

    void updateZoom(double factor, double cursor_x, double cursor_y, double window_width, double window_height) const;

    void resetZoom() const;

    void notifyCursorMovement(double dx, double dy) const;
};

struct Digraph {
    // Digraph takes ownership of the source for safety because there are string_view's to the source everywhere
    Digraph *m_parent{nullptr};
    std::forward_list<std::string> m_referenced_sources;
    std::string_view m_name;
    Attrs m_default_node_attrs;
    Attrs m_default_edge_attrs;
    std::unordered_set<size_t> m_io_port_ranks;
    std::unordered_map<std::string_view, size_t> m_cluster_order;
    std::unordered_map<std::string_view, Digraph> m_clusters;
    std::unordered_map<std::string_view, Node> m_nodes;
    std::vector<size_t> m_rank_counts;
    std::vector<std::vector<std::string_view> > m_per_rank_orderings;
    std::vector<std::unordered_map<std::string_view, size_t> > m_per_rank_orderings_index;
    std::vector<std::tuple<std::string_view, std::vector<std::string_view> > > m_rank_constraints;
    size_t m_n_ghost_nodes{};
    // graph attrs also exist. They store attributes like ranksep, nodesep, etc.
    Attrs m_attrs;
    DigraphRenderAttrs m_render_attrs;
    // wrapped in unique pointer for convenience, so I can make it opaque without having to manually deallocate
    GraphRenderer m_renderer;

    explicit Digraph();

    // parses digraph source
    explicit Digraph(std::string source);

    explicit Digraph(std::string_view source);

    void update(std::string_view extra_source);

    void preprocess(render::glyph::GlyphLoader &glyph_loader, std::string_view id_in_parent = "");

    void fuseClusterLinksIntoClusterSuperNodes();

    void convertParentLinksToIOPorts(std::string_view id_in_parent);

    void computeRanks();

    void insertGhostNodes();

    void deleteIngoingNodesVectors();

    void populateIngoingNodesVectors();

    void computeHorizontalOrderings();

    void computeNodeLayouts(render::glyph::GlyphLoader &glyph_loader);

    void computeGraphLayout(render::glyph::GlyphLoader &glyph_loader);

    void optimizeGraphLayout();

    void computeEdgeLayout();

    void computeEdgeLabelLayouts(render::glyph::GlyphLoader &glyph_loader);

    void constructFromTokens(std::span<tokenizer::Token> &tokens);

private:
    void swapNodesOnRank(std::string_view a, std::string_view b);
};

class UnexpectedTokenException final : std::exception {
    const tokenizer::Token m_token;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit UnexpectedTokenException(const tokenizer::Token &token);
};

class IllegalAttributeException final : std::exception {
    const std::string m_attr;
    const std::string m_value;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit IllegalAttributeException(std::string attr, std::string value);
};

class ReservedIdentifierException final : std::exception {
    const std::string m_name;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit ReservedIdentifierException(std::string name);
};
}
