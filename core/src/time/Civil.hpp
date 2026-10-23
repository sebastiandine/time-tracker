#pragma once

namespace timetracker::detail {

/**
 * @brief Report whether @p year is a Gregorian leap year.
 * @param year Full year, such as 2024.
 * @return True for a leap year.
 */
[[nodiscard]] inline bool isLeapYear(int year) {
    if (year % 400 == 0) {
        return true;
    }
    if (year % 100 == 0) {
        return false;
    }
    return year % 4 == 0;
}

/**
 * @brief Days in a civil month.
 * @param year Full year, used for February.
 * @param month Month from 1 through 12.
 * @return The day count, or 0 when @p month is outside 1 through 12.
 */
[[nodiscard]] inline int daysInMonth(int year, int month) {
    if (month < 1 || month > 12) {
        return 0;
    }
    constexpr int kDays[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return kDays[month];
}

}  // namespace timetracker::detail
