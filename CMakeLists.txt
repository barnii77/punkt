cmake_minimum_required(VERSION 3.20)
project(punkt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

# TODO add option for baking font into executable

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/glad)
add_subdirectory(third_party/googletest)

add_library(punkt
        # source files
        src/dot.cpp
        src/utils.cpp
        src/parser/dot_tokenizer.cpp
        src/parser/dot_parser.cpp
        src/layout/compute_ranks.cpp
        src/layout/insert_ghost_nodes.cpp
        src/layout/order_nodes_horizontally.cpp
        src/glyph_loader/glyph_loader.cpp
        src/glyph_loader/font_parser.cpp
        src/glyph_loader/glyph_utils.cpp
        src/layout/node_layout.cpp
        src/layout/graph_layout.cpp
        src/layout/edge_layout.cpp

        # header files
        include/punkt/dot.hpp
        include/punkt/int_types.hpp
        include/punkt/dot_tokenizer.hpp
        include/punkt/utils.hpp
        include/punkt/dot_constants.hpp
        include/punkt/glyph_loader/glyph_loader.hpp
        include/punkt/glyph_loader/default_font_resources.hpp
)
target_include_directories(punkt PRIVATE include/)
target_link_libraries(punkt PRIVATE glfw)
target_link_libraries(punkt PRIVATE glad)

function(add_target name file use_gtest)
    add_executable(${name} ${file})
    target_include_directories(${name} PRIVATE include/)
    if (use_gtest)
        target_link_libraries(${name} PRIVATE gtest)
    endif ()
    target_link_libraries(${name} PRIVATE punkt)
endfunction()

add_target(main src/main.cpp FALSE)
add_target(tests tests/main.cpp TRUE)
target_sources(tests PRIVATE
        tests/test_tokenizer.cpp
        tests/test_parser.cpp
        tests/test_rank_assignment.cpp
        tests/test_ghost_node_insertion.cpp
        tests/test_horizontal_ordering.cpp
        tests/test_node_layout.cpp
        tests/test_graph_layout.cpp
)
target_include_directories(tests PRIVATE tests/)
