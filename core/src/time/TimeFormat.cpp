#include "timetracker/time/TimeFormat.hpp"

#include "time/Civil.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace timetracker {
namespace {

[[nodiscard]] bool readDigits(std::string_view text, std::size_t offset, int width, int& value) {
    int parsed = 0;
    for (int index = 0; index < width; ++index) {
        const auto character = text[offset + static_cast<std::size_t>(index)];
        if (character < '0' || character > '9') {
            return false;
        }
        parsed = parsed * 10 + (character - '0');
    }
    value = parsed;
    return true;
}

[[nodiscard]] std::time_t utcSeconds(std::tm parts) {
#if defined(_WIN32)
    return _mkgmtime(&parts);
#else
    return timegm(&parts);
#endif
}

}  // namespace

Result<std::string> formatUtc(std::chrono::system_clock::time_point instant) {
    const auto floored = std::chrono::floor<std::chrono::seconds>(instant);
    const std::time_t seconds = std::chrono::system_clock::to_time_t(floored);
    std::tm parts{};
#if defined(_WIN32)
    if (gmtime_s(&parts, &seconds) != 0) {
        return Result<std::string>::err("The timestamp is not valid.");
    }
#else
    if (gmtime_r(&seconds, &parts) == nullptr) {
        return Result<std::string>::err("The timestamp is not valid.");
    }
#endif
    char buffer[32];
    const int written =
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                      parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour,
                      parts.tm_min, parts.tm_sec);
    if (written != 20) {
        return Result<std::string>::err("The timestamp is not valid.");
    }
    return Result<std::string>::ok(std::string{buffer});
}

Result<std::chrono::system_clock::time_point> parseUtc(std::string_view text) {
    if (text.size() != 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != 'Z') {
        return Result<std::chrono::system_clock::time_point>::err("The timestamp is not valid.");
    }
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    const bool digits = readDigits(text, 0, 4, year) && readDigits(text, 5, 2, month) &&
                        readDigits(text, 8, 2, day) && readDigits(text, 11, 2, hour) &&
                        readDigits(text, 14, 2, minute) && readDigits(text, 17, 2, second);
    if (!digits || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59) {
        return Result<std::chrono::system_clock::time_point>::err("The timestamp is not valid.");
    }
    const int maxDay = detail::daysInMonth(year, month);
    if (day < 1 || day > maxDay) {
        return Result<std::chrono::system_clock::time_point>::err("The timestamp is not valid.");
    }
    std::tm parts{};
    parts.tm_year = year - 1900;
    parts.tm_mon = month - 1;
    parts.tm_mday = day;
    parts.tm_hour = hour;
    parts.tm_min = minute;
    parts.tm_sec = second;
    const std::time_t seconds = utcSeconds(parts);
    if (seconds == static_cast<std::time_t>(-1)) {
        return Result<std::chrono::system_clock::time_point>::err("The timestamp is not valid.");
    }
    return Result<std::chrono::system_clock::time_point>::ok(
        std::chrono::system_clock::from_time_t(seconds));
}

Result<std::int64_t> secondsBetweenUtc(std::string_view startUtc, std::string_view endUtc) {
    auto start = parseUtc(startUtc);
    if (!start) {
        return Result<std::int64_t>::err(start.error());
    }
    auto end = parseUtc(endUtc);
    if (!end) {
        return Result<std::int64_t>::err(end.error());
    }
    if (end.value() < start.value()) {
        return Result<std::int64_t>::err("The clock is earlier than the start time.");
    }
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(end.value() - start.value()).count();
    return Result<std::int64_t>::ok(seconds);
}

std::string formatDuration(std::int64_t seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    const auto hours = seconds / 3600;
    const auto minutes = (seconds % 3600) / 60;
    const auto remain = seconds % 60;
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%lld:%02lld:%02lld", static_cast<long long>(hours),
                  static_cast<long long>(minutes), static_cast<long long>(remain));
    return std::string{buffer};
}

}  // namespace timetracker
