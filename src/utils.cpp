#include "punkt/dot.hpp"
#include "punkt/utils/utils.hpp"
#include "punkt/dot_constants.hpp"
#include "punkt/gl_error.hpp"
#include "generated/punkt/color_map.hpp"

#include <numeric>
#include <charconv>
#include <string>
#include <algorithm>
#include <cctype>
#include <string_view>
#include <cmath>
#include <cstddef>

using namespace punkt;

XOptPipelineStageSettings::LegalizerSettings::LegalizerSettings() = default;

XOptPipelineStageSettings::LegalizerSettings::LegalizerSettings(const LegalizationTiming legalization_timing,
                                                                const LegalizerSpecialInstruction
                                                                legalizer_special_instruction,
                                                                const bool try_cancel_mean_shift)
    : m_legalization_timing(legalization_timing), m_legalizer_special_instruction(legalizer_special_instruction),
      m_try_cancel_mean_shift(try_cancel_mean_shift) {
}

XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction
XOptPipelineStageSettings::LegalizerSettings::LegalizerSpecialInstruction::getExplodeInterGroupNodeSep(
    const float factor) {
    return LegalizerSpecialInstruction{Type::explode_inter_group_node_sep, factor};
}

XOptPipelineStageSettings::SweepSettings::SweepSettings() = default;

XOptPipelineStageSettings::SweepSettings::SweepSettings(const bool is_downward_sweep, const bool is_group_sweep,
                                                        const ssize_t sweep_n_ranks_limit)
    : m_is_downward_sweep(is_downward_sweep), m_is_group_sweep(is_group_sweep),
      m_sweep_n_ranks_limit(sweep_n_ranks_limit) {
}

XOptPipelineStageSettings::XOptPipelineStageSettings() = default;

XOptPipelineStageSettings::XOptPipelineStageSettings(const size_t repeats, const ssize_t max_iters,
                                                     const float initial_dampening, const float dampening_fadeout,
                                                     const float pull_towards_mean, const float regularization,
                                                     const SweepMode sweep_mode,
                                                     const LegalizerSettings legalizer_settings,
                                                     std::vector<SweepSettings> sweep_settings)
    : m_repeats(repeats), m_max_iters(max_iters), m_initial_dampening(initial_dampening),
      m_dampening_fadeout(dampening_fadeout), m_pull_towards_mean(pull_towards_mean), m_regularization(regularization),
      m_sweep_mode(sweep_mode), m_legalizer_settings(legalizer_settings), m_sweep_settings(std::move(sweep_settings)) {
}

bool punkt::caseInsensitiveEquals(const std::string_view lhs, const std::string_view rhs) {
    return std::ranges::equal(lhs, rhs, [](const unsigned char a, const unsigned char b) {
        return std::tolower(a) == std::tolower(b);
    });
}

size_t punkt::stringViewToSizeTHex(const std::string_view &sv, const std::string_view &attr_name) {
    size_t result = 0;
    if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result, 16); ec != std::errc()) {
        throw IllegalAttributeException(std::string(attr_name), std::string(sv));
    }
    return result;
}

size_t punkt::stringViewToSizeT(const std::string_view &sv, const std::string_view &attr_name) {
    size_t result = 0;
    if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result); ec != std::errc()) {
        throw IllegalAttributeException(std::string(attr_name), std::string(sv));
    }
    return result;
}

float punkt::stringViewToFloat(const std::string_view &sv, const std::string_view &attr_name) {
    float result = 0.0f;
    if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result); ec != std::errc()) {
        throw IllegalAttributeException(std::string(attr_name), std::string(sv));
    }
    return result;
}

void punkt::parseColor(const std::string_view &color, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) {
    if (color.starts_with("#")) {
        // rgb color
        if (color.size() != 7 ||
            !std::accumulate(color.begin() + 1, color.end(), true, [](const bool is_digit, const char c) {
                return is_digit && (std::isdigit(c) || 'a' <= c && c <= 'f');
            })) {
            throw IllegalAttributeException(std::string("color"), std::string(color));
        }
        r = static_cast<uint8_t>(stringViewToSizeTHex(color.substr(1, 3), "color@red"));
        g = static_cast<uint8_t>(stringViewToSizeTHex(color.substr(3, 5), "color@green"));
        b = static_cast<uint8_t>(stringViewToSizeTHex(color.substr(5, 7), "color@blue"));
        a = static_cast<uint8_t>(stringViewToSizeTHex(color.substr(7, 9), "color@alpha"));
    } else {
        // look it up in color map
        uint32_t packed;
        if (!color_name_to_rgb.contains(color)) {
            packed = color_name_to_rgb.at(default_color);
            // throw IllegalAttributeException(std::string("color"), std::string(color));
        } else {
            packed = color_name_to_rgb.at(color);
        }
        r = (packed >> 24) & 0xFF;
        g = (packed >> 16) & 0xFF;
        b = (packed >> 8) & 0xFF;
        a = (packed >> 0) & 0xFF;
    }
}


void punkt::evaluateBezier(const double t, const double p0_x, const double p0_y, const double p1_x, const double p1_y,
                           const double p2_x, const double p2_y, const double p3_x, const double p3_y, double &out_x,
                           double &out_y) {
    const double u = 1.0 - t;
    const double tt = t * t;
    const double uu = u * u;
    const double uuu = uu * u;
    const double ttt = tt * t;

    out_x = uuu * p0_x
            + 3.0 * uu * t * p1_x
            + 3.0 * u * tt * p2_x
            + ttt * p3_x;

    out_y = uuu * p0_y
            + 3.0 * uu * t * p1_y
            + 3.0 * u * tt * p2_y
            + ttt * p3_y;
}

double punkt::bezierCurveLength(const double p0_x, const double p0_y, const double p1_x, const double p1_y,
                                const double p2_x, const double p2_y, const double p3_x, const double p3_y) {
    double total_length = 0.0;

    double prev_x = p0_x;
    double prev_y = p0_y;

    for (size_t i = 1; i <= n_spline_divisions; ++i) {
        const double t = static_cast<double>(i) / n_spline_divisions;

        double curr_x, curr_y;
        evaluateBezier(t, p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, curr_x, curr_y);

        const double dx = curr_x - prev_x;
        const double dy = curr_y - prev_y;

        total_length += std::sqrt(dx * dx + dy * dy);

        prev_x = curr_x;
        prev_y = curr_y;
    }

    return total_length;
}

static void checkShaderCompileStatus(const GLuint shader) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }
}

static void checkProgramLinkStatus(const GLuint program) {
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking failed: " << infoLog << std::endl;
    }
}

GLuint punkt::createShaderProgram(const char *vertex_shader_code, const char *geometry_shader_code,
                                  const char *fragment_shader_code) {
    GLuint vertex_shader = 0;
    if (vertex_shader_code) {
        vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        GL_CHECK(glShaderSource(vertex_shader, 1, &vertex_shader_code, nullptr));
        GL_CHECK(glCompileShader(vertex_shader));
        checkShaderCompileStatus(vertex_shader);
    }

    GLuint geometry_shader = 0;
    if (geometry_shader_code) {
        geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
        GL_CHECK(glShaderSource(geometry_shader, 1, &geometry_shader_code, nullptr));
        GL_CHECK(glCompileShader(geometry_shader));
        checkShaderCompileStatus(geometry_shader);
    }

    GLuint fragment_shader = 0;
    if (fragment_shader_code) {
        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        GL_CHECK(glShaderSource(fragment_shader, 1, &fragment_shader_code, nullptr));
        GL_CHECK(glCompileShader(fragment_shader));
        checkShaderCompileStatus(fragment_shader);
    }

    const GLuint program = glCreateProgram();
    if (vertex_shader) {
        GL_CHECK(glAttachShader(program, vertex_shader));
    }
    if (geometry_shader) {
        GL_CHECK(glAttachShader(program, geometry_shader));
    }
    if (fragment_shader) {
        GL_CHECK(glAttachShader(program, fragment_shader));
    }
    GL_CHECK(glLinkProgram(program));
    checkProgramLinkStatus(program);

    GL_CHECK(glDeleteShader(vertex_shader));
    GL_CHECK(glDeleteShader(geometry_shader));
    GL_CHECK(glDeleteShader(fragment_shader));

    return program;
}

