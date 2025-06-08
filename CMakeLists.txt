cmake_minimum_required(VERSION 3.20)
project(punkt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

function(enable_asan)
    if (MSVC)
        set(asan_option "/fsanitize=address")
    else ()
        set(asan_option "-fsanitize=address")
    endif ()

    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("${asan_option}" has_asan_support)

    if (has_asan_support)
        add_compile_options("${asan_option}")
        message(STATUS "Compiling with ASan enabled")
    else ()
        message(WARNING "Compiling WITHOUT ASan (ASan is unsupported by the compiler)")
    endif ()
endfunction()

function(initialize_env)
    if ((CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_CONFIGURATION STREQUAL "Release"))
        set(release_mode TRUE)
    else ()
        set(release_mode FALSE)
    endif ()
    set(RELEASE_MODE ${release_mode} PARENT_SCOPE)

    if (release_mode)
        # in Release mode, bake font into executable by default
        set(punkt_bake_font_into_executable_default ON)
        set(punkt_remove_fps_counter_default ON)
    else ()
        set(punkt_bake_font_into_executable_default OFF)
        set(punkt_remove_fps_counter_default OFF)
        # address sanitizer for debug builds
        enable_asan()
    endif ()
    option(PUNKT_BAKE_FONT_INTO_EXECUTABLE "If enabled, bakes the raw binary font file content into a static variable at compile time so the executable is standalone" ${punkt_bake_font_into_executable_default})
    option(PUNKT_REMOVE_FPS_COUNTER "If set, removes the FPS counter from the application" ${punkt_remove_fps_counter_default})
endfunction()

function(add_project_subdirectories)
    add_subdirectory(third_party/glad)
    add_subdirectory(third_party/glfw)
    add_subdirectory(third_party/googletest)
endfunction()

function(setup_header_generation)
    set(shader_source_dir "${CMAKE_SOURCE_DIR}/src/shaders")
    set(SHADER_SOURCE_DIR "${shader_source_dir}" PARENT_SCOPE)
    file(GLOB shader_files
            "${shader_source_dir}/*.*"
            "${shader_source_dir}/glyph_loader/*.*"
    )
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${shader_files})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/color_map.txt")
    set(generated_parent_dir "${CMAKE_BINARY_DIR}/generated")
    set(GENERATED_PARENT_DIR "${generated_parent_dir}" PARENT_SCOPE)
    set(generated_dir "${generated_parent_dir}/generated")
    set(GENERATED_DIR "${generated_dir}" PARENT_SCOPE)
    file(MAKE_DIRECTORY "${generated_dir}/punkt/glyph_loader/")
    file(MAKE_DIRECTORY "${generated_dir}/punkt/shaders/")
endfunction()

function(convert_hex_to_c_array INPUT_HEX OUTPUT_VAR OUTPUT_VAR_N)
    string(LENGTH "${INPUT_HEX}" hex_len)
    math(EXPR num_bytes "${hex_len} / 2")
    set(formatted_resource "")

    foreach (i RANGE 0 ${num_bytes}-1)
        math(EXPR pos "2 * ${i}")
        string(SUBSTRING "${INPUT_HEX}" ${pos} 2 byte)

        if (i EQUAL 0)
            set(formatted_resource "\\x${byte}")
        else ()
            set(formatted_resource "${formatted_resource}\\x${byte}")
        endif ()
    endforeach ()

    set(${OUTPUT_VAR} "${formatted_resource}00" PARENT_SCOPE)
    math(EXPR num_bytes "${num_bytes} + 1")  # because of the null byte at the end
    set(${OUTPUT_VAR_N} ${num_bytes} PARENT_SCOPE)
endfunction()

function(generate_font_data_header)
    set(generated_font_resources_content "#pragma once\n\n#include <unordered_map>\n#include <string>\n\nstatic std::unordered_map<std::string_view, std::string> raw_font_data_map = {\n")

    file(GLOB font_files RELATIVE "${CMAKE_SOURCE_DIR}/resources/fonts/" "${CMAKE_SOURCE_DIR}/resources/fonts/*.*")
    if (NOT (font_files STREQUAL ""))
        message(STATUS "Detected font files (baking them into executable): ${font_files}")
    endif ()

    foreach (file ${font_files})
        set(font_path "resources/fonts/${file}")
        file(READ "${CMAKE_SOURCE_DIR}/${font_path}" RAW_RESOURCE_CONTENTS HEX)
        convert_hex_to_c_array("${RAW_RESOURCE_CONTENTS}" formatted_resource_contents formatted_resource_num_bytes)
        string(APPEND generated_font_resources_content "    {\"${font_path}\", std::string(\"${formatted_resource_contents}\", ${formatted_resource_num_bytes})},\n")
    endforeach ()

    string(APPEND generated_font_resources_content "};\n")

    file(GENERATE
            OUTPUT "${GENERATED_DIR}/punkt/glyph_loader/font_resources.hpp"
            CONTENT "${generated_font_resources_content}"
    )
endfunction()

function(escape_string INPUT_STRING OUTPUT_VAR)
    # Escape backslashes first.
    string(REPLACE "\\" "\\\\" result "${INPUT_STRING}")
    # Escape double quotes.
    string(REPLACE "\"" "\\\"" result "${result}")
    # Escape newlines.
    string(REPLACE "\n" "\\n" result "${result}")
    set(${OUTPUT_VAR} "${result}" PARENT_SCOPE)
endfunction()

function(listify_string INPUT_STRING OUTPUT_VAR)
    # replace newlines with semicolon
    string(REPLACE ";" "<|%SEMICOLON%|>" INPUT_STRING "${INPUT_STRING}")
    string(REPLACE "\n" ";" INPUT_STRING "${INPUT_STRING}")
    set(${OUTPUT_VAR} "${INPUT_STRING}" PARENT_SCOPE)
endfunction()

function(unlistify_string INPUT_STRING OUTPUT_VAR)
    # reconstruct original semicolons
    string(REPLACE ";" "\n" INPUT_STRING "${INPUT_STRING}")
    string(REPLACE "<|%SEMICOLON%|>" ";" INPUT_STRING "${INPUT_STRING}")
    set(${OUTPUT_VAR} "${INPUT_STRING}" PARENT_SCOPE)
endfunction()

# hacky cmake shader include system
function(resolve_shader_includes SHADER_CONTENT OUTPUT_VAR)
    listify_string("${SHADER_CONTENT}" SHADER_CONTENT)
    set(result "")
    foreach (line ${SHADER_CONTENT})
        string(REGEX MATCH "^#include \"([^\"]*)\"$" match "${line}")
        if (match)
            set(filename "${CMAKE_MATCH_1}")
            if (EXISTS "${SHADER_SOURCE_DIR}/${filename}")
                file(READ "${SHADER_SOURCE_DIR}/${filename}" file_content)
                listify_string("${file_content}" file_content)
                string(REGEX REPLACE "^#include \"[^\"]*\"$" "${file_content}" line "${line}")
            else ()
                message(FATAL_ERROR "File not found: ${SHADER_SOURCE_DIR}/${filename}")
            endif ()
        endif ()
        string(APPEND result "${line};")
    endforeach ()

    set(SHADER_CONTENT "${result}")
    unlistify_string("${SHADER_CONTENT}" SHADER_CONTENT)

    set(${OUTPUT_VAR} "${SHADER_CONTENT}" PARENT_SCOPE)
endfunction()

function(get_shader_var_value NAME EXT OUT_VAR)
    set(shader_path "${SHADER_SOURCE_DIR}/${NAME}.${EXT}")
    if (EXISTS "${shader_path}")
        file(READ "${shader_path}" shader_content)
        resolve_shader_includes("${shader_content}" shader_content)
        escape_string("${shader_content}" shader_content)
        set(shader_value "\"${shader_content}\"")
    else ()
        set(shader_value "nullptr")
    endif ()
    set(${OUT_VAR} "${shader_value}" PARENT_SCOPE)
endfunction()

function(add_shader_pair_to_shader_header_content NAME)
    get_shader_var_value("${NAME}" vert vertex_shader_value)
    get_shader_var_value("${NAME}" geom geometry_shader_value)
    get_shader_var_value("${NAME}" frag fragment_shader_value)

    string(REPLACE "/" "_" VAR_NAME "${NAME}")
    string(APPEND SHADER_HEADER_CONTENT "static const char *${VAR_NAME}_vertex_shader_code = ${vertex_shader_value};\nstatic const char *${VAR_NAME}_geometry_shader_code = ${geometry_shader_value};\nstatic const char *${VAR_NAME}_fragment_shader_code = ${fragment_shader_value};\n")
    set(SHADER_HEADER_CONTENT "${SHADER_HEADER_CONTENT}" PARENT_SCOPE)
endfunction()

function(generate_shader_code_header)
    set(SHADER_HEADER_CONTENT "#pragma once\n\n")

    add_shader_pair_to_shader_header_content(nodes)
    add_shader_pair_to_shader_header_content(chars)
    add_shader_pair_to_shader_header_content(edges)
    add_shader_pair_to_shader_header_content(edges_bezier)
    add_shader_pair_to_shader_header_content(arrows)
    add_shader_pair_to_shader_header_content(glyph_loader/ttf_cover_shader)
    add_shader_pair_to_shader_header_content(glyph_loader/ttf_stencil_shader)

    file(GENERATE
            OUTPUT "${GENERATED_DIR}/punkt/shaders/shaders.hpp"
            CONTENT "${SHADER_HEADER_CONTENT}"
    )
endfunction()

function(parse_color_map COLOR_MAP OUTPUT_VARIABLE)
    string(REPLACE "\n" ";" COLOR_MAP "${COLOR_MAP}")
    set(parsed_colors "")

    foreach (line ${COLOR_MAP})
        string(REGEX MATCH "(.+): #([0-9A-Fa-f]+)" match "${line}")
        if (match)
            string(REGEX REPLACE "(.+): #([0-9A-Fa-f]+)" "    {\"\\1\", 0x\\2}," entry "${line}")
            list(APPEND parsed_colors "${entry}")
        endif ()
    endforeach ()

    string(REPLACE ";" "\n" parsed_colors "${parsed_colors}")
    set(${OUTPUT_VARIABLE} "${parsed_colors}" PARENT_SCOPE)
endfunction()

function(generate_color_map_header)
    set(COLOR_MAP_HEADER_CONTENT "#pragma once\n\n#include <unordered_map>\n#include <string_view>\n#include <cstdint>\n\nstatic std::unordered_map<std::string_view, uint32_t> color_name_to_rgb = {\n")

    file(READ "${CMAKE_SOURCE_DIR}/resources/color_map.txt" COLOR_MAP_CONTENT)

    parse_color_map("${COLOR_MAP_CONTENT}" COLOR_MAP_BODY)
    string(APPEND COLOR_MAP_HEADER_CONTENT "${COLOR_MAP_BODY}\n};\n")

    file(GENERATE
            OUTPUT "${GENERATED_DIR}/punkt/color_map.hpp"
            CONTENT "${COLOR_MAP_HEADER_CONTENT}"
    )
endfunction()


############### main building procedure ###############
initialize_env()
add_project_subdirectories()
setup_header_generation()

add_library(punkt
        # source files
        src/dot.cpp
        src/punkt_run.cpp
        src/utils.cpp
        src/parser/dot_tokenizer.cpp
        src/parser/dot_parser.cpp
        src/layout/compute_ranks.cpp
        src/layout/handle_link_nodes.cpp
        src/layout/insert_ghost_nodes.cpp
        src/layout/order_nodes_horizontally.cpp
        src/layout/initial_node_layout.cpp
        src/layout/optimize_node_x_positions.cpp
        src/layout/common.cpp
        src/layout/graph_layout.cpp
        src/layout/edge_layout.cpp
        src/layout/edge_label_layout.cpp
        src/layout/populate_glyph_quads_with_text.cpp
        src/glyph_loader/glyph_loader.cpp
        src/glyph_loader/font_parser.cpp
        src/glyph_loader/glyph_utils.cpp
        src/glyph_loader/psf1_loader.cpp
        src/renderer/struct_graph_renderer_impl.cpp
        src/renderer/gl_preprocessing.cpp
        src/renderer/gl_renderer.cpp

        # header files
        include/punkt/api/punkt.h
        include/punkt/dot.hpp
        include/punkt/dot_tokenizer.hpp
        include/punkt/utils/int_types.hpp
        include/punkt/utils/utils.hpp
        include/punkt/dot_constants.hpp
        include/punkt/gl_renderer.hpp
        include/punkt/gl_error.hpp
        include/punkt/glyph_loader/glyph_loader.hpp
        include/punkt/glyph_loader/default_font_resources.hpp
        include/punkt/glyph_loader/psf1_loader.hpp
        include/punkt/layout/common.hpp
        include/punkt/layout/populate_glyph_quads_with_text.hpp
)
target_include_directories(punkt PRIVATE include/)
target_include_directories(punkt PRIVATE ${GENERATED_PARENT_DIR})
target_link_libraries(punkt PRIVATE glfw)
target_link_libraries(punkt PRIVATE glad)
target_compile_definitions(punkt PRIVATE $<$<CONFIG:Release>:PUNKT_RELEASE_BUILD>)
if (PUNKT_REMOVE_FPS_COUNTER)
    target_compile_definitions(punkt PRIVATE PUNKT_REMOVE_FPS_COUNTER)
endif ()

# codegen
generate_shader_code_header()
generate_color_map_header()
if (PUNKT_BAKE_FONT_INTO_EXECUTABLE)
    generate_font_data_header()
    target_compile_definitions(punkt PRIVATE PUNKT_BAKE_FONT_INTO_EXECUTABLE)
endif ()

function(add_dependent_target name file)
    add_executable(${name} ${file})
    target_include_directories(${name} PRIVATE include/)
    target_link_libraries(${name} PRIVATE gtest)
    target_link_libraries(${name} PRIVATE punkt)
endfunction()

add_dependent_target(main src/main.cpp)
add_dependent_target(tests tests/main.cpp)
target_sources(tests PRIVATE
        tests/test_tokenizer.cpp
        tests/test_parser.cpp
        tests/test_rank_assignment.cpp
        tests/test_ghost_node_insertion.cpp
        tests/test_horizontal_ordering.cpp
        tests/test_node_layout.cpp
        tests/test_graph_layout.cpp
)
target_link_libraries(tests PRIVATE glad)
