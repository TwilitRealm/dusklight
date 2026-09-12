#pragma once

#include <fmt/format.h>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace dusk::ui {

struct ByteFormat {
    int gibFractionDigits = 1;
    int mibFractionDigits = 1;
};

inline std::string format_count(uint64_t value) {
    if (value >= 1'000'000) {
        return fmt::format("{:.1f}m", static_cast<double>(value) / 1'000'000.0);
    }
    if (value >= 1'000) {
        return fmt::format("{:.1f}k", static_cast<double>(value) / 1'000.0);
    }
    return fmt::format("{}", value);
}

inline std::string display_date(std::string_view timestamp) {
    return std::string{timestamp.substr(0, 10)};
}

inline std::string relative_date(std::string_view timestamp) {
    if (timestamp.size() < 10 || timestamp[4] != '-' || timestamp[7] != '-') {
        return display_date(timestamp);
    }
    int yearValue = 0;
    unsigned monthValue = 0;
    unsigned dayValue = 0;
    const auto parse = [](std::string_view text, auto& value) {
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc{} && end == text.data() + text.size();
    };
    if (!parse(timestamp.substr(0, 4), yearValue) || !parse(timestamp.substr(5, 2), monthValue) ||
        !parse(timestamp.substr(8, 2), dayValue))
    {
        return display_date(timestamp);
    }

    const std::chrono::year_month_day date{
        std::chrono::year{yearValue}, std::chrono::month{monthValue}, std::chrono::day{dayValue}};
    if (!date.ok()) {
        return display_date(timestamp);
    }
    const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    const auto age = (today - std::chrono::sys_days{date}).count();
    if (age < 0) {
        return display_date(timestamp);
    }
    if (age == 0) {
        return "today";
    }
    if (age == 1) {
        return "yesterday";
    }
    if (age < 7) {
        return fmt::format("{} days ago", age);
    }
    if (age < 30) {
        const auto weekCount = age / 7;
        return fmt::format("{} week{} ago", weekCount, weekCount == 1 ? "" : "s");
    }
    if (age < 365) {
        const auto monthCount = age / 30;
        return fmt::format("{} month{} ago", monthCount, monthCount == 1 ? "" : "s");
    }
    const auto yearCount = age / 365;
    return fmt::format("{} year{} ago", yearCount, yearCount == 1 ? "" : "s");
}

inline std::string format_bytes(uint64_t bytes, ByteFormat options = {}) {
    constexpr double kiB = 1024.0;
    constexpr double miB = kiB * 1024.0;
    constexpr double giB = miB * 1024.0;
    if (bytes >= static_cast<uint64_t>(giB)) {
        return fmt::format(
            "{:.{}f} GiB", static_cast<double>(bytes) / giB, options.gibFractionDigits);
    }
    if (bytes >= static_cast<uint64_t>(miB)) {
        return fmt::format(
            "{:.{}f} MiB", static_cast<double>(bytes) / miB, options.mibFractionDigits);
    }
    if (bytes >= static_cast<uint64_t>(kiB)) {
        return fmt::format("{:.0f} KiB", static_cast<double>(bytes) / kiB);
    }
    return fmt::format("{} B", bytes);
}

// Truncates without splitting a UTF-8 sequence.
inline std::string snippet(std::string_view text, size_t maxBytes) {
    if (text.size() <= maxBytes) {
        return std::string{text};
    }
    size_t end = maxBytes;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
        --end;
    }
    return fmt::format("{}...", text.substr(0, end));
}

}  // namespace dusk::ui
