#include "punkt/api/punkt.h"

#include <string>
#include <iostream>
#include <fstream>

class FileNotFoundException final : std::exception {
    const std::string m_path;

public:
    [[nodiscard]] const char *what() const noexcept override;

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

static void runTestMode() {
    const char *path = "examples/graphviz_gallery/siblings.dot";
    // path = "examples/demo_inheritance.dot";
    // path = "examples/graphviz_gallery/ninja.dot";
    // path = "examples/demo3.dot";
    // path = "examples/demo8.dot";
    // path = "temp/trigger_x_weirdness.dot";
    const std::string test_input = readInputFile(path);
    const auto font_path = "resources/fonts/tinyfont.psf";
    punktRun(test_input.data(), font_path);
}

static void printHelp() {
    std::cout << "punkt - A tiny clone of dot from graphviz" << std::endl << std::endl << "Usage:" << std::endl <<
            "punkt file/path.dot" << std::endl << std::endl << "Additional flags:" << std::endl <<
            "\t--help\tShow this message" << std::endl << "\t-h\tShow this message";
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        printHelp();
    } else if (argc == 2) {
        if (const std::string_view arg = argv[1]; arg == "--run-test-mode") {
            runTestMode();
        } else if (arg == "--help" || arg == "-h") {
            printHelp();
        } else {
            const char *path = argv[1];
            std::string test_input;
            try {
                test_input = readInputFile(path);
            } catch (const FileNotFoundException &e) {
                std::cerr << "Error: " << e.what();
                return 1;
            }
            try {
                const auto font_path = "resources/fonts/tinyfont.psf";
                punktRun(test_input.data(), font_path);
            } catch (const std::exception &e) {
                std::cerr << "Error: " << e.what();
            }
        }
    } else {
        std::cerr << "Takes at most 1 argument: {graph_file_path}";
    }
}

////////////////////// TODOS //////////////////////

// TODO I can't really do the parent link to io port conversion after rank assignment...
// TODO the first part of cluster preprocessing (all the io port handling stuff) must be done before a graph itself is
// processed, i.e. I need to split Digraph::preprocess so that the IO port handling is a separate function that is
// called on all clusters as the very first thing Digraph::preprocess does after writing the parent pointers in the
// clusters. This will allow me to then delete the outgoing nodes in favor of IO ports immediately after that.
// TODO I have to introduce a kind of connections map for cluster super-nodes
// TODO DO TODO IN compute_ranks.cpp
// TODO FINISH handle_link_nodes.cpp
// TODO ADD SPECIAL CASE HANDLING TO per_rank_ordering assignment for IO port ranks so they get assigned according to
// the ordering in the parent graph
// TODO also, I should actually assign the per-rank ordering vectors in handle_link_nodes.cpp already and should
// only modify the initial ordering assignment so it skips ranks where the ordering vectors are already populated.
// TODO except the above is really shitty software architecture, I have to rethink how I do info propagation.
// this probably requires more refactors.
// TODO I have to actually store which node an edge was originally meant to route to when replacing link nodes with
// cluster super nodes... i.e. the cluster super node edges must store @to A
// TODO figure out how to do this with nested clusters
// TODO populate the m_io_port_ranks set
// TODO I have to do a thing similar to ghost edges where I only place the arrow attrs on the correct edges when I have
// IO port edges
// TODO make sure to filter out edges like A -> B inside of clusters when neither A nor B are owned by the cluster
// TODO I think I'll just delete them, but this is important. Not deleting them (or moving them) would cause
// out of bounds memory accesses due to invalidated ingoing node refs in the super-node insertion

// TODO I could add record nodes, i.e. allow nodes to have internal splits (splitting lines)

// TODO add preprocessing step before rank assignment that replaces all references to nodes with an attr @link (see
// parser) with a single @cluster_N node in all edges and *deletes the original nodes*. I also have to add a similar
// mechanism for nodes referenced inside a cluster. When inside a cluster, nodes are referenced that are unknown, I have
// to note that down somehow and if, later on, a node is defined in the parent digraph, the original implicitly created
// node has to be replaced with out port links inside the cluster and the out port has to link to the parent node. Also
// a similar mechanism is needed for IO port decomposition (i.e. inserting an IO port in in and out connections)
// TODO @link can be either "parent" or "cluster_N"
// TODO I need to add a feature with which you can extend digraphs with more source code after init because of subgraphs
// e.g. subgraph x{A;B;} subgraph y{C;D;} subgraph x{E;F;}
// TODO I have to destroy cluster super-nodes at the end too
// TODO introduce some sort of concept of IO port ranks that cannot be reordered by x optimization but still moved while
// everything else can be moved, so I need to add special case handling to x opt and the (currently unused) reordering.
// TODO use and populate the m_fixed_ordering_ranks
// TODO think about in what order I want to do rank assignment because the rank assignment of the parent graph should
// influence that of the children
// TODO I'll create the super-node first, then run parent rank assignment, then I'll insert the IO-ports in the
// cluster (top or bottom depending on where the other node is relative to the cluster's super-node)
// TODO I have to leak out layout info from clusters eventually
// TODO I have to do the layout pipeline of the parent graph first (until the x optimization, i.e. when the x layout has
// been determined) and then I know the outer ordering of above and below ranks. Then, I can insert the IO ports in the
// appropriate order and keep their positions fixed (TODO figure out how to without affecting the result of x opt).
// Then I can run the (full) layout pipeline of the cluster.
// Except sh*t, I need the x opt result of the inner for the outer x opt because super-node dimensions are needed
// So here's what I'll do:
// I'll run the parent pipeline up until initial layout
// I'll run initial layout of the cluster
// I'll run parent x opt
// I'll finish cluster pipeline
// I'll re-run parent x opt and finish parent pipeline
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
// TODO but wait... it would probably be smarter to route edges into the top and bottom of super-nodes, i.e. introduce
// an "IO rank", for the lack of a better word, on the top and bottom so that the ghost nodes spawned there are placed
// on the border of the cluster. Then, you just treat those as normal nodes, run the layout pipeline on the cluster, and
// simply hook up those top and bottom ports with cluster-external nodes. The entire idea of rail nodes doesn't play too
// well with my layout engine and very similar visuals as the visuals rail nodes create can emerge from this top/bottom
// IO port mechanism
// TODO this implies throwing a magic attr onto the IO ports that tells you what inner node it connects to

// TODO the current pipeline only works for simple graphs... I need to rework this most likely in a way that optimizes
// much more effectively on large graphs...
// TODO figure out why weird suboptimal edge routing happens with the iterative optimization
// TODO also why is there often a rightward trend
// TODO I could add bubble reordering into the mix with the actual x optimization, i.e. interleaving the x opt with
// bubble reordering steps where it will swap adjacent nodes in the ordering
// TODO I should probably fix those little weird spots splines get on each ghost node when they don't go straight down
// TODO handle cluster=true in subgraphs; eg subgraph X {cluster=true; A; B; C}
// TODO update unit tests - they don't work anymore (5 fail)
// TODO should I add de-cycling to the rank assignment as a preliminary step like graphviz does?
// TODO re-route edge trajectories so they go into the arrow tail instead
// TODO I have a massive antialiasing problem
// TODO adapt the GLRenderer so that I can use it for clusters, i.e. with multiple digraphs at once
// TODO though I should probably just write out the glyph quads of the cluster into the glyph quads of the parent
// graph right before rendering (after x optimization and stuff)

// TODO handle DPI properly, currently, 1 dot = 1 pixel, but that's simply not true
// TODO handle font loading manually - write a minimal TTF parser and renderer
// TODO make sure this builds using all major build systems (make, ninja, visual studio msbuild)
// TODO maybe handle xlabels on edges eventually (external label, i.e. the edge goes through the left edge of the left
// glyph. At this point I can do trigonometry like with the arrows and rotate the text label node instead of the center)
// TODO maybe handle color gradients on edges eventually
// TODO handle `labelangle` attribute right after the glyph quads have been populated for an edge relative to the top
// TODO maybe I could add drawing on top xD (i.e. you have a separate surface that you draw into in black and that
// gets rendered last so it overdraws the graph
// TODO include examples from graphviz docs in examples/
