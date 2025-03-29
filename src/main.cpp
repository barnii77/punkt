#include "punkt/punkt.h"

#include <string>
#include <fstream>

class FileNotFoundException final : std::exception {
    const std::string m_path;

    [[nodiscard]] const char *what() const noexcept override;

public:
    explicit FileNotFoundException(std::string path);
};

FileNotFoundException::FileNotFoundException(std::string path)
    : m_path(std::move(path)) {
}

const char *FileNotFoundException::what() const noexcept {
    const std::string msg = std::string("Font not found: \"") + std::string(m_path);
    const auto m = new char[msg.length() + 1];
    msg.copy(m, msg.length());
    m[msg.length()] = '\0';
    return m;
}

static std::string readInputFile(const char *path) {
    std::ifstream font_file(path, std::ios::binary);
    if (!font_file) {
        throw FileNotFoundException(path);
    }

    font_file.seekg(0, std::ios::end);
    const std::streamsize file_size = font_file.tellg();
    font_file.seekg(0, std::ios::beg);

    std::string out(file_size, '\0');
    font_file.read(out.data(), file_size);
    return out;
}

int main() {
    const char *path = "examples/graphviz_gallery/ninja.dot";
    path = "examples/demo5.dot";
    const std::string test_input = readInputFile(path);
    const auto font_path = "resources/fonts/tinyfont.psf";
    punktRun(test_input.data(), font_path);
}

////////////////////// TODOS //////////////////////

// TODO in optimize_node_x_positions.cpp, I have a todo where I have to recompute graph layout after node optimization
    // I should move part of that computation after optimization (or twice if I want to do the rejection from below)
// TODO I have a massive antialiasing problem
// TODO should I only apply pull towards mean in x opt to groups, but not individual nodes?
    // if I do that, make sure to weight the groups by group size
        // I feel like the pull should be stronger in more dense ranks in general and should be weaker (not stronger as
        // it currently unintentionally is) in sparse ranks. See ninja.dot for why
// TODO I think I have a bug where the x optimization will actually position nodes at (rank+1) slightly to
// right/down compared to where they should actually be... would explain the fact demo2.dot and the ninja.dot example
// are so weird (by which I mean that there are no straight edges where there should be straight edges and more iters
// do not help)
// TODO test the stability of my x optimization
    // TODO figure out why it explodes with examples/demo3.dot without regularization and pull towards mean
    // TODO should I bound it? (e.g. 2 * graph_dims)
// TODO handle spline edges
// TODO do something with x optimization (optimize_node_x_positions.cpp) to incentivise straight lines slightly more
// TODO maybe add something that rates the x positioning before and after x optimization and discards the optimization
// results in favor of the initial positioning if the score is worse? It seems the barycenter x positioning better only
// on some graphs, not universally better.
// TODO adapt the GLRenderer so that I can use it for clusters, i.e. with multiple digraphs at once
// TODO a cluster should become a single super-node when doing all my layout computations. This means it is literally
// treated as a single (giant) node with special attributes (@type = "cluster", @code = <string_view into the graph
// source defining the cluster>) and assigned a single rank. Once we have computed the horizontal orderings, we convert
// the node into a digraph by calling constructor and preprocess on Digraph(cluster_source).
// Now, here's the really tricky part:
// We have to re-route edges according to their `@source`s and `@dest`s now. For this, I am thinking of introducing a
// new concept called "rail nodes", similar to ghost nodes, but they are not vertically centered like every other node
// type. Instead, they are placed at the height of a particular rank within a particular cluster and have the height of
// the bottom of that rank. To be specific, the cluster rank it will be positioned at is the rank above the `@dest`
// rank (for @source substitution the process is similar). Depending on whether the edge is coming from the right or
// left, the rail node will be placed right or left of the cluster node. Then, edges are routed through those rail nodes
// normally. Once we have constructed these "rails" (the name makes sense because the edges kind of glide along the
// vertical axis of the rank until a certain point, then the rail stops and they "derail"), we can do our node basic
// positioning, then, once implemented, the additional spring/barycenter repositioning (however you want to call it) and
// finally we can upward inherit all the nodes in the cluster into the main digraph by adding the cluster node position.
// Also, don't forget to temporarily substitute edges to any nodes belonging to the cluster with an edge to the cluster
// super-node. Use attributes @source and @dest to store the actual sources and destinations.
// TODO handle DPI properly, currently, 1 dot = 1 pixel, but that's simply not true
// TODO handle font loading manually - write a minimal TTF parser and renderer
// TODO make sure this builds using all major build systems (make, ninja, visual studio msbuild)
// TODO maybe handle xlabels on edges eventually (external label, i.e. the edge goes through the left edge of the left
// glyph. At this point I can do trigonometry like with the arrows and rotate the text label node instead of the center)
// TODO maybe handle color gradients on edges eventually
// TODO handle `labelangle` attribute right after the glyph quads have been populated for an edge relative to the top
// TODO maybe I could add drawing on top xD (i.e. you have a separate surface that you draw into in black and that
// gets rendered last so it overdraws the graph
// TODO XDD I can add animations because I don't have to comply with the dot language spec exactly, just roughly
// TODO include examples from graphviz docs in examples/
