#pragma once

#include <cstdint>
#include <string_view>

namespace punkt {
size_t stringViewToSizeT(const std::string_view &sv, const std::string_view &attr_name);

float stringViewToFloat(const std::string_view &sv, const std::string_view &attr_name);
}
