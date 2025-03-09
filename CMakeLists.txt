cmake_minimum_required(VERSION 3.20)
project(punkt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/glad)
add_subdirectory(third_party/googletest)

add_library(punkt
        # source files
        src/dot.cpp
        src/dot_tokenizer.cpp
        src/dot_parser.cpp
        src/compute_ranks.cpp
        src/insert_ghost_nodes.cpp
        src/order_nodes_horizontally.cpp

        # header files
        include/punkt/dot_tokenizer.hpp
        include/punkt/dot.hpp
        include/punkt/dot_constants.hpp
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
)
target_include_directories(tests PRIVATE tests/)
