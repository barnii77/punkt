cmake_minimum_required(VERSION 3.15)
project(glad LANGUAGES C)

add_library(glad STATIC
        src/glad.c
)

target_include_directories(glad PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/KHR>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(glad PUBLIC c_std_99)
target_compile_options(glad PRIVATE -w)