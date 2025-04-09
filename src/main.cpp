#include "punkt/api/punkt.h"

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
    const char *path = "examples/graphviz_gallery/siblings.dot";
    // path = "examples/demo_inheritance.dot";
    // path = "examples/graphviz_gallery/ninja.dot";
    // path = "examples/demo3.dot";
    // path = "examples/demo8.dot";
    const std::string test_input = readInputFile(path);
    const auto font_path = "resources/fonts/tinyfont.psf";
    punktRun(test_input.data(), font_path);
}

////////////////////// TODOS //////////////////////

// TODO the current pipeline only works for simple graphs... I need to rework this most likely in a way that optimizes
// much more effectively on large graphs...
// TODO maybe I should introduce node weights in group sweep (i.e. give ghost nodes very little influence)
// TODO maybe if I simply let the x optimization determine the rank ordering as well that could lead to better results?
    // would be aware of node widths, however, couldn't do the bubble reordering
        // maybe I should combine my current orderer with this so the x opt determines order but seeded with the order
        // produced by the naive barycenter ordering and bubble reordering?
// TODO update unit tests - they don't work anymore (5 fail)
// TODO should I add de-cycling to the rank assignment as a preliminary step like graphviz does?
// TODO re-route edge trajectories so they go into the arrow tail instead
// TODO I have a massive antialiasing problem
// TODO adapt the GLRenderer so that I can use it for clusters, i.e. with multiple digraphs at once
    // TODO though I should probably just write out the glyph quads of the cluster into the glyph quads of the parent
    // graph right before rendering (after x optimization and stuff)
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
