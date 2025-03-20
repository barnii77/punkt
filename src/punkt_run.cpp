#include "punkt/punkt.h"
#include "punkt/dot.hpp"
#include "punkt/glyph_loader/glyph_loader.hpp"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cmath>
#include <ctime>
#include <iostream>

constexpr double zoom_base = 1.1f;

struct PunktUIState {
    // all this stuff is volatile because any callback could change it at any point in time (& compiler is unaware)
    volatile double m_cursor_x{}, m_cursor_y{}, m_cursor_dx{}, m_cursor_dy{}, m_zoom_update{1.0f};
    volatile bool m_reset_zoom{}, m_left_mouse_button_down{}, m_is_first_cursor_action{true};

    void frameDone(GLFWwindow *window, const punkt::Digraph &dg);
};

void PunktUIState::frameDone(GLFWwindow *window, const punkt::Digraph &dg) {
    dg.m_renderer.updateZoom(static_cast<float>(m_zoom_update));
    if (m_reset_zoom) {
        dg.m_renderer.resetZoom();
    }
    dg.m_renderer.notifyCursorMovement(m_cursor_dx, m_cursor_dy);
    m_reset_zoom = false;
    m_zoom_update = 1.0f;
    m_cursor_dx = 0;
    m_cursor_dy = 0;
}

static PunktUIState ui_state;

static size_t fps_counter_last_time;
static size_t fps_counter_frame_count = 0;

static void fpsCounterInit() {
    fps_counter_last_time = std::time(nullptr);
    std::cout << "WARNING: This build contains the debug FPS counter. You can disable it through CMake options."
            << std::endl;
}

static void fpsCounterNotifyFrameDone() {
    fps_counter_frame_count++;
    if (const std::time_t time = std::time(nullptr); time - fps_counter_last_time > 0) {
        // 1 second elapsed - print frame notification
        std::cout << "FPS: " << fps_counter_frame_count << std::endl;
        fps_counter_frame_count = 0;
        fps_counter_last_time = time;
    }
}

static void errorCallback(const int error, const char *description) {
    std::fprintf(stderr, "Error (code %x): %s\n", error, description);
    std::abort();
}

static void keyCallback(GLFWwindow *window, const int key, const int scancode, const int action, const int mods) {
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        ui_state.m_reset_zoom = true;
    }
}

static void cursorCallback(GLFWwindow *window, const double x, const double y) {
    if (ui_state.m_is_first_cursor_action) {
        ui_state.m_is_first_cursor_action = false;
    } else if (ui_state.m_left_mouse_button_down) {
        ui_state.m_cursor_dx += x - ui_state.m_cursor_x;
        ui_state.m_cursor_dy += y - ui_state.m_cursor_y;
    }
    ui_state.m_cursor_x = x;
    ui_state.m_cursor_y = y;
}

static void mouseButtonCallback(GLFWwindow *window, const int button, const int action, const int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            ui_state.m_left_mouse_button_down = true;
        } else if (action == GLFW_RELEASE) {
            ui_state.m_left_mouse_button_down = false;
        }
    }
}

static void mouseWheelCallback(GLFWwindow *window, const double x_offset, const double y_offset) {
    ui_state.m_zoom_update *= std::pow(zoom_base, y_offset);
}

static GLFWwindow *setupGL() {
    glfwSetErrorCallback(errorCallback);

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x MSAA for antialiasing

    GLFWwindow *window = glfwCreateWindow(640, 480, "punkt", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, mouseWheelCallback);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
    // TODO add cli argument to disable vsync by setting the parameter here to 0
    glfwSwapInterval(1);

    return window;
}

static void terminateGL(GLFWwindow *window) {
    glfwDestroyWindow(window);
    glfwTerminate();
}

// TODO maybe I should only re-render when user input has been received to reduce cpu/gpu time consumption?
void punktRun(const char *graph_source_cstr, const char *font_path_relative_to_project_root_cstr) {
    GLFWwindow *window = setupGL();

    const std::string_view graph_source(graph_source_cstr);
    const std::string_view font_path = font_path_relative_to_project_root_cstr
                                           ? std::string_view(font_path_relative_to_project_root_cstr)
                                           : std::string_view();
    punkt::Digraph dg(graph_source);
    punkt::render::glyph::GlyphLoader glyph_loader = font_path_relative_to_project_root_cstr
                                                         ? punkt::render::glyph::GlyphLoader{std::string(font_path)}
                                                         : punkt::render::glyph::GlyphLoader{};
    dg.preprocess(glyph_loader);
    dg.m_renderer.initialize(dg, glyph_loader);
#ifndef PUNKT_REMOVE_FPS_COUNTER
    fpsCounterInit();
#endif

    while (!glfwWindowShouldClose(window)) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        dg.m_renderer.notifyFramebufferSize(width, height);
        dg.m_renderer.renderFrame();
        glfwSwapBuffers(window);
        glfwPollEvents();
        ui_state.frameDone(window, dg);
#ifndef PUNKT_REMOVE_FPS_COUNTER
        fpsCounterNotifyFrameDone();
#endif
    }
    terminateGL(window);
}
