#include "punkt/dot.hpp"
#include "punkt/utils.hpp"

#include <charconv>

using namespace punkt;

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
