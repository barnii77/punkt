#pragma once

#include <cstdint>
#include <string_view>

namespace punkt {
size_t stringViewToSizeTHex(const std::string_view &sv, const std::string_view &attr_name);

size_t stringViewToSizeT(const std::string_view &sv, const std::string_view &attr_name);

float stringViewToFloat(const std::string_view &sv, const std::string_view &attr_name);

void parseColor(const std::string_view &color, uint8_t &r, uint8_t &g, uint8_t &b);
}

#define getAttrOrDefault(attrs, attr_name, default_value) ((attrs).contains(attr_name) ? (attrs).at(attr_name) : (default_value))
#define getAttrTransformedOrDefault(attrs, attr_name, default_value, transformation) ((attrs).contains(attr_name) ? transformation((attrs).at(attr_name)) : (default_value))
#define getAttrTransformedCheckedOrDefault(attrs, attr_name, default_value, transformation) ((attrs).contains(attr_name) ? transformation((attrs).at(attr_name), (attr_name)) : (default_value))
