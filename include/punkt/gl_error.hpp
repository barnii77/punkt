#pragma once

#include <glad/glad.h>

#include <iostream>

#ifdef PUNKT_RELEASE_BUILD
#define GL_CHECK(expr) expr
#else
#define GL_CHECK(expr) do { \
    expr; \
    GLenum err = glGetError(); \
    if (err != GL_NO_ERROR) { \
        std::cerr << "OpenGL error(s) at " << __FILE__ << ":" << __LINE__ << " in call: " << #expr << std::endl; \
        while (err != GL_NO_ERROR) { \
            std::cerr << "OpenGL error: " << err << std::endl; \
            err = glGetError(); \
        } \
    } \
} while(0)
#endif

// Contrary to GL_CHECK, this is not removed in release builds
#define GL_CRITICAL_CHECK_ALL_ERRORS() do { \
    GLenum err = glGetError(); \
    if (err != GL_NO_ERROR) { \
        std::cerr << "OpenGL error(s) at " << __FILE__ << ":" << __LINE__ << " in call: " << std::endl; \
        while (err != GL_NO_ERROR) { \
            std::cerr << "OpenGL error: " << err << std::endl; \
            err = glGetError(); \
        } \
    } \
} while(0)
