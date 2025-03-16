cmake_minimum_required(VERSION 3.20)
project(punkt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

if ((CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_CONFIGURATION STREQUAL "Release"))
    # in Release mode, bake font into executable by default
    set(PUNKT_BAKE_FONT_INTO_EXECUTABLE_DEFAULT ON)
else ()
    set(PUNKT_BAKE_FONT_INTO_EXECUTABLE_DEFAULT OFF)
endif ()

option(PUNKT_BAKE_FONT_INTO_EXECUTABLE "If enabled, bakes the raw binary font file content into a static variable at compile time so the executable is standalone" ${PUNKT_BAKE_FONT_INTO_EXECUTABLE_DEFAULT})

add_subdirectory(third_party/glad)
add_subdirectory(third_party/glfw)
add_subdirectory(third_party/googletest)

# generate some header files
file(GLOB SHADER_FILES
        "${CMAKE_SOURCE_DIR}/src/shaders/*.*"
        "${CMAKE_SOURCE_DIR}/src/shaders/*.*"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SHADER_FILES})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/color_map.txt")
set(GENERATED_PARENT_DIR "${CMAKE_BINARY_DIR}/generated")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}/punkt/glyph_loader/")
file(MAKE_DIRECTORY "${GENERATED_DIR}/punkt/shaders/")

function(convert_hex_to_c_array INPUT_HEX OUTPUT_VAR)
    string(LENGTH "${INPUT_HEX}" hex_len)
    math(EXPR num_bytes "${hex_len} / 2")
    set(formatted_resource "")

    foreach (i RANGE 0 ${num_bytes}-1)
        math(EXPR pos "2 * ${i}")
        string(SUBSTRING "${INPUT_HEX}" ${pos} 2 byte)

        if (i EQUAL 0)
            set(formatted_resource "0x${byte}")
        else ()
            set(formatted_resource "${formatted_resource}, 0x${byte}")
        endif ()
    endforeach ()

    set(${OUTPUT_VAR} "${formatted_resource}00" PARENT_SCOPE)
endfunction()

if (${PUNKT_BAKE_FONT_INTO_EXECUTABLE})
    set(GENERATED_FONT_RESOURCES_CONTENT "#pragma once\n\n#include <unordered_map>\n#include <string>\n\nstatic std::unordered_map<std::string_view, std::string> raw_font_data_map = {\n")

    file(GLOB FONT_FILES RELATIVE "${CMAKE_SOURCE_DIR}/resources/fonts/" "${CMAKE_SOURCE_DIR}/resources/fonts/*.*")
    if (NOT (FONT_FILES STREQUAL ""))
        message(STATUS "Detected font files (baking them into executable): ${FONT_FILES}")
    endif()

    foreach (FILE ${FONT_FILES})
        file(READ "${CMAKE_SOURCE_DIR}/resources/fonts/${FILE}" RAW_RESOURCE_CONTENTS HEX)
        convert_hex_to_c_array("${RAW_RESOURCE_CONTENTS}" FORMATTED_RESOURCE_CONTENTS)
        string(APPEND GENERATED_FONT_RESOURCES_CONTENT "    {\"${FILE}\", std::string((char []) {${FORMATTED_RESOURCE_CONTENTS}})},\n")
    endforeach ()

    string(APPEND GENERATED_FONT_RESOURCES_CONTENT "};\n")

    file(GENERATE
            OUTPUT "${GENERATED_DIR}/punkt/glyph_loader/font_resources.hpp"
            CONTENT "${GENERATED_FONT_RESOURCES_CONTENT}"
    )
endif ()

set(SHADER_HEADER_CONTENT "#pragma once\n\n")

function(escape_string INPUT_STRING OUTPUT_VAR)
    # Escape backslashes first.
    string(REPLACE "\\" "\\\\" result "${INPUT_STRING}")
    # Escape double quotes.
    string(REPLACE "\"" "\\\"" result "${result}")
    # Escape newlines.
    string(REPLACE "\n" "\\n" result "${result}")
    set(${OUTPUT_VAR} "${result}" PARENT_SCOPE)
endfunction()

function(add_shader_pair_to_shader_header_content NAME)
    set(shader_path "${CMAKE_SOURCE_DIR}/src/shaders/${NAME}.vert")
    file(READ "${shader_path}" vertex_shader_content)
    escape_string("${vertex_shader_content}" vertex_shader_content)

    set(shader_path "${CMAKE_SOURCE_DIR}/src/shaders/${NAME}.geom")
    if (EXISTS "${shader_path}")
        file(READ "${shader_path}" geometry_shader_content)
        escape_string("${geometry_shader_content}" geometry_shader_content)
        set(geometry_shader_value "\"${geometry_shader_content}\"")
    else ()
        set(geometry_shader_value "nullptr")
    endif ()

    set(shader_path "${CMAKE_SOURCE_DIR}/src/shaders/${NAME}.frag")
    file(READ "${shader_path}" fragment_shader_content)
    escape_string("${fragment_shader_content}" fragment_shader_content)

    string(APPEND SHADER_HEADER_CONTENT "static const char *${NAME}_vertex_shader_code = \"${vertex_shader_content}\";\nstatic const char *${NAME}_geometry_shader_code = ${geometry_shader_value};\nstatic const char *${NAME}_fragment_shader_code = \"${fragment_shader_content}\";\n")
    set(SHADER_HEADER_CONTENT "${SHADER_HEADER_CONTENT}" PARENT_SCOPE)
endfunction()

add_shader_pair_to_shader_header_content(nodes)
add_shader_pair_to_shader_header_content(chars)
add_shader_pair_to_shader_header_content(edges)

file(GENERATE
        OUTPUT "${GENERATED_DIR}/punkt/shaders/shaders.hpp"
        CONTENT "${SHADER_HEADER_CONTENT}"
)

# generate color map
set(COLOR_MAP_HEADER_CONTENT "#pragma once\n\n#include <unordered_map>\n#include <string_view>\n#include <cstdint>\n\nstatic std::unordered_map<std::string_view, uint32_t> color_name_to_rgb = {\n")

file(READ "${CMAKE_SOURCE_DIR}/resources/color_map.txt" COLOR_MAP_CONTENT)

function(parse_color_map COLOR_MAP OUTPUT_VARIABLE)
    string(REPLACE "\n" ";" COLOR_MAP "${COLOR_MAP}")
    set(parsed_colors "")

    foreach(line ${COLOR_MAP})
        string(REGEX MATCH "(.+): #([0-9A-Fa-f]+)" match "${line}")
        if (match)
            string(REGEX REPLACE "(.+): #([0-9A-Fa-f]+)" "    {\"\\1\", 0x\\2}," entry "${line}")
            list(APPEND parsed_colors "${entry}")
        endif()
    endforeach()

    string(REPLACE ";" "\n" parsed_colors "${parsed_colors}")
    set(${OUTPUT_VARIABLE} "${parsed_colors}" PARENT_SCOPE)
endfunction()

parse_color_map("${COLOR_MAP_CONTENT}" COLOR_MAP_BODY)
string(APPEND COLOR_MAP_HEADER_CONTENT "${COLOR_MAP_BODY}\n};\n")

file(GENERATE
        OUTPUT "${GENERATED_DIR}/punkt/color_map.hpp"
        CONTENT "${COLOR_MAP_HEADER_CONTENT}"
)

# compile the project
add_library(punkt
        # source files
        src/dot.cpp
        src/punkt_run.cpp
        src/utils.cpp
        src/parser/dot_tokenizer.cpp
        src/parser/dot_parser.cpp
        src/layout/compute_ranks.cpp
        src/layout/insert_ghost_nodes.cpp
        src/layout/order_nodes_horizontally.cpp
        src/layout/node_layout.cpp
        src/layout/graph_layout.cpp
        src/layout/edge_layout.cpp
        src/glyph_loader/glyph_loader.cpp
        src/glyph_loader/font_parser.cpp
        src/glyph_loader/glyph_utils.cpp
        src/renderer/struct_graph_renderer_impl.cpp
        src/renderer/gl_preprocessing.cpp
        src/renderer/gl_renderer.cpp

        # header files
        include/punkt/punkt.h
        include/punkt/dot.hpp
        include/punkt/int_types.hpp
        include/punkt/dot_tokenizer.hpp
        include/punkt/utils.hpp
        include/punkt/dot_constants.hpp
        include/punkt/gl_renderer.hpp
        include/punkt/gl_error.hpp
        include/punkt/glyph_loader/glyph_loader.hpp
        include/punkt/glyph_loader/default_font_resources.hpp
)
target_include_directories(punkt PRIVATE include/)
target_include_directories(punkt PRIVATE ${GENERATED_PARENT_DIR})
target_link_libraries(punkt PRIVATE glfw)
target_link_libraries(punkt PRIVATE glad)
target_compile_definitions(punkt PRIVATE $<$<CONFIG:Release>:PUNKT_RELEASE_BUILD>)

function(add_target name file)
    add_executable(${name} ${file})
    target_include_directories(${name} PRIVATE include/)
    target_link_libraries(${name} PRIVATE gtest)
    target_link_libraries(${name} PRIVATE punkt)
endfunction()

add_target(main src/main.cpp)
add_target(tests tests/main.cpp)
target_sources(tests PRIVATE
        tests/test_tokenizer.cpp
        tests/test_parser.cpp
        tests/test_rank_assignment.cpp
        tests/test_ghost_node_insertion.cpp
        tests/test_horizontal_ordering.cpp
        tests/test_node_layout.cpp
        tests/test_graph_layout.cpp
)
