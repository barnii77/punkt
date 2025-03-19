#include "punkt/dot.hpp"
#include "punkt/utils.hpp"
#include "generated/punkt/color_map.hpp"

#include <numeric>
#include <charconv>
#include <string>

using namespace punkt;

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

void punkt::parseColor(const std::string_view &color, uint8_t &r, uint8_t &g, uint8_t &b) {
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
    } else {
        // look it up in color map
        if (!color_name_to_rgb.contains(color)) {
            throw IllegalAttributeException(std::string("color"), std::string(color));
        }
        const uint32_t packed = color_name_to_rgb.at(color);
        r = (packed >> 16) & 0xFF;
        g = (packed >> 8) & 0xFF;
        b = (packed >> 0) & 0xFF;
    }
}
