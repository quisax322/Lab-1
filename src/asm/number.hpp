#pragma once

#include <charconv>
#include <cstdint>
#include <string_view>

inline std::errc parse_number_literal(std::string_view text, std::uint64_t& value) {
    int base = 10;
    if (text.substr(0, 2) == "0x" || text.substr(0, 2) == "0X") {
        base = 16;
        text.remove_prefix(2);
    } else if (text.substr(0, 2) == "0b" || text.substr(0, 2) == "0B") {
        base = 2;
        text.remove_prefix(2);
    }
    if (text.empty()) return std::errc::invalid_argument;
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value, base);
    return result.ptr == end ? result.ec : std::errc::invalid_argument;
}
