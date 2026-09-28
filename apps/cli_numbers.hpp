#pragma once
#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <string_view>
namespace markov_cero::apps {
inline bool parse_size(const char* text, std::size_t& value) {
    const std::string_view input(text);
    const auto result = std::from_chars(input.data(), input.data()+input.size(), value);
    return result.ec == std::errc{} && result.ptr == input.data()+input.size();
}
inline bool parse_nonnegative(const char* text, double& value) {
    char* end = nullptr;
    errno = 0;
    value = std::strtod(text, &end);
    return !errno && end != text && *end == '\0' && std::isfinite(value) && value >= 0;
}
}
