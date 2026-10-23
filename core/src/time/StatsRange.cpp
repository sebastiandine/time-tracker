#include "timetracker/time/StatsRange.hpp"

#include "time/Civil.hpp"
#include "timetracker/time/TimeFormat.hpp"

#include <chrono>
#include <cstdint>

namespace timetracker {
namespace {

constexpr int kMinOffsetMinutes = -840;
constexpr int kMaxOffsetMinutes = 840;

[[nodiscard]] Result<void> checkOffset(int offsetMinutes) {
    if (offsetMinutes < kMinOffsetMinutes || offsetMinutes > kMaxOffsetMinutes) {
        return Result<void>::err("The time zone offset is outside the supported range.");
    }
    return Result<void>::ok();
}

[[nodiscard]] bool validDate(CivilDate date) {
    if (date.year < 1970 || date.year > 9999) {
        return false;
    }
    const int days = detail::daysInMonth(date.year, date.month);
    return date.day >= 1 && date.day <= days;
}

// Days since 1970-01-01. Howard Hinnant's civil calendar conversion, so a
// report window does not depend on the process time zone.
[[nodiscard]] int daysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

void civilFromDays(int z, int& year, unsigned& month, unsigned& day) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned dayOfEra = static_cast<unsigned>(z - era * 146097);
    const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const int yearTemp = static_cast<int>(yearOfEra) + era * 400;
    const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const unsigned monthPart = (5 * dayOfYear + 2) / 153;
    day = dayOfYear - (153 * monthPart + 2) / 5 + 1;
    month = monthPart < 10 ? monthPart + 3 : monthPart - 9;
    year = yearTemp + (month <= 2 ? 1 : 0);
}

[[nodiscard]] CivilDate addDays(CivilDate date, int delta) {
    const int serial = daysFromCivil(date.year, static_cast<unsigned>(date.month),
                                     static_cast<unsigned>(date.day)) +
                       delta;
    int year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civilFromDays(serial, year, month, day);
    return CivilDate{year, static_cast<int>(month), static_cast<int>(day)};
}

[[nodiscard]] int weekdayMonday0(CivilDate date) {
    const int serial = daysFromCivil(date.year, static_cast<unsigned>(date.month),
                                     static_cast<unsigned>(date.day));
    const int weekday = (serial + 3) % 7;
    return weekday < 0 ? weekday + 7 : weekday;
}

[[nodiscard]] Result<std::string> midnightUtc(CivilDate date, int offsetMinutes) {
    if (!validDate(date)) {
        return Result<std::string>::err("That date is not valid.");
    }
    const auto serial = daysFromCivil(date.year, static_cast<unsigned>(date.month),
                                      static_cast<unsigned>(date.day));
    const auto utcSeconds = static_cast<std::int64_t>(serial) * 86400 -
                            static_cast<std::int64_t>(offsetMinutes) * 60;
    if (utcSeconds < 0) {
        return Result<std::string>::err("The date is outside the supported range.");
    }
    const auto instant =
        std::chrono::system_clock::time_point{std::chrono::seconds{utcSeconds}};
    return formatUtc(instant);
}

[[nodiscard]] Result<UtcRange> between(CivilDate start, CivilDate end, int offsetMinutes) {
    auto offset = checkOffset(offsetMinutes);
    if (!offset) {
        return Result<UtcRange>::err(offset.error());
    }
    auto startUtc = midnightUtc(start, offsetMinutes);
    if (!startUtc) {
        return Result<UtcRange>::err(startUtc.error());
    }
    auto endUtc = midnightUtc(end, offsetMinutes);
    if (!endUtc) {
        return Result<UtcRange>::err(endUtc.error());
    }
    UtcRange range;
    range.startUtc = std::move(startUtc.value());
    range.endUtc = std::move(endUtc.value());
    return Result<UtcRange>::ok(std::move(range));
}

[[nodiscard]] Result<UtcRange> dayRange(CivilDate day, int offsetMinutes) {
    if (!validDate(day)) {
        return Result<UtcRange>::err("That date is not valid.");
    }
    return between(day, addDays(day, 1), offsetMinutes);
}

[[nodiscard]] Result<UtcRange> weekRange(CivilDate day, int offsetMinutes) {
    if (!validDate(day)) {
        return Result<UtcRange>::err("That date is not valid.");
    }
    const auto monday = addDays(day, -weekdayMonday0(day));
    return between(monday, addDays(monday, 7), offsetMinutes);
}

[[nodiscard]] Result<UtcRange> monthRange(CivilDate day, int offsetMinutes) {
    if (!validDate(day)) {
        return Result<UtcRange>::err("That date is not valid.");
    }
    const CivilDate first{day.year, day.month, 1};
    const CivilDate next = day.month == 12 ? CivilDate{day.year + 1, 1, 1}
                                           : CivilDate{day.year, day.month + 1, 1};
    return between(first, next, offsetMinutes);
}

[[nodiscard]] Result<UtcRange> customRange(CivilDate start, CivilDate end, int offsetMinutes) {
    if (!validDate(start) || !validDate(end)) {
        return Result<UtcRange>::err("That date is not valid.");
    }
    const auto startSerial = daysFromCivil(start.year, static_cast<unsigned>(start.month),
                                           static_cast<unsigned>(start.day));
    const auto endSerial = daysFromCivil(end.year, static_cast<unsigned>(end.month),
                                         static_cast<unsigned>(end.day));
    if (endSerial < startSerial) {
        return Result<UtcRange>::err("End is before start.");
    }
    return between(start, addDays(end, 1), offsetMinutes);
}

[[nodiscard]] Result<UtcRange> allTimeRange() {
    UtcRange range;
    range.startUtc = "1970-01-01T00:00:00Z";
    range.endUtc = "2100-01-01T00:00:00Z";
    return Result<UtcRange>::ok(std::move(range));
}

}  // namespace

Result<UtcRange> boundsForPreset(StatsPreset preset, CivilDate localToday, int offsetMinutes,
                                 CivilDate customStart, CivilDate customEnd) {
    switch (preset) {
    case StatsPreset::AllTime:
        return allTimeRange();
    case StatsPreset::Today:
        return dayRange(localToday, offsetMinutes);
    case StatsPreset::ThisWeek:
        return weekRange(localToday, offsetMinutes);
    case StatsPreset::ThisMonth:
        return monthRange(localToday, offsetMinutes);
    case StatsPreset::Custom:
        return customRange(customStart, customEnd, offsetMinutes);
    }
    return Result<UtcRange>::err("Unknown statistics range.");
}

}  // namespace timetracker
