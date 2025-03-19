#pragma once

#include "punkt/dot_tokenizer.hpp"
#include "punkt/glyph_loader/glyph_loader.hpp"

#include <string>
#include <forward_list>
#include <vector>
#include <memory>
#include <unordered_map>
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
    bool m_is_visible{};

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
    bool m_is_ghost{};
    std::vector<GlyphQuad> m_quads;

    explicit NodeRenderAttrs();
};

struct Node {
    std::string_view m_name;
    std::vector<std::reference_wrapper<Edge> > m_ingoing;
    std::vector<Edge> m_outgoing;
    Attrs m_attrs;
    NodeRenderAttrs m_render_attrs{};

    Node(std::string_view name, Attrs attrs);

    void populateRenderInfo(const render::glyph::GlyphLoader &glyph_loader);
};

struct RankRenderAttrs {
    size_t m_rank_x{}, m_rank_y{}, m_rank_width{}, m_rank_height{};
};

struct DigraphRenderAttrs {
    size_t m_rank_sep{}, m_node_sep{}, m_graph_width{}, m_graph_height{};
    std::vector<RankRenderAttrs> m_rank_render_attrs;
};

struct Digraph;

struct GraphRenderer {
    void *m_graph_renderer{};

    explicit GraphRenderer();

    ~GraphRenderer();

    void initialize(const Digraph &dg, render::glyph::GlyphLoader &glyph_loader);

    void notifyFramebufferSize(int width, int height) const;

    void renderFrame() const;

    void updateZoom(float factor) const;

    void resetZoom() const;

    void notifyCursorMovement(double dx, double dy) const;
};

struct Digraph {
    // Digraph takes ownership of the source for safety because there are string_view's to the source everywhere
    std::string m_source;
    std::forward_list<std::string> m_generated_sources;
    std::string_view m_name;
    Attrs m_default_node_attrs;
    Attrs m_default_edge_attrs;
    std::unordered_map<std::string_view, Node> m_nodes;
    std::vector<size_t> m_rank_counts;
    std::vector<std::vector<std::string_view> > m_per_rank_orderings;
    std::vector<std::unordered_map<std::string_view, size_t> > m_per_rank_orderings_index;
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

    void preprocess(render::glyph::GlyphLoader &glyph_loader);

    void computeRanks();

    void insertGhostNodes();

    void deleteIngoingNodesVectors();

    void populateIngoingNodesVectors();

    void computeHorizontalOrderings();

    void computeNodeLayouts(render::glyph::GlyphLoader &glyph_loader);

    void computeGraphLayout();

    void computeEdgeLayout();

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
