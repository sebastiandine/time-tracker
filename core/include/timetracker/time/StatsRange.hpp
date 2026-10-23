#pragma once

#include "timetracker/util/Result.hpp"

#include <string>

namespace timetracker {

/**
 * @brief A calendar date in the user's local zone. Month and day are 1-based.
 */
struct CivilDate {
    int year{1970};
    int month{1};
    int day{1};
};

/**
 * @brief Inclusive UTC start and exclusive UTC end of a statistics window.
 *
 * Both strings are @c YYYY-MM-DDTHH:MM:SSZ so they sort as text.
 */
struct UtcRange {
    std::string startUtc;
    std::string endUtc;
};

/**
 * @brief Which window the statistics tab is asking for.
 */
enum class StatsPreset { Today, ThisWeek, ThisMonth, AllTime, Custom };

/**
 * @brief Convert a local preset into a UTC window.
 *
 * The process time zone is not read. @p offsetMinutes is local time minus UTC,
 * so UTC+2 is 120. Weeks start on Monday. @c AllTime ignores the dates and the
 * offset. Custom dates are inclusive local midnights; the returned end is the
 * midnight after @p customEnd.
 * @param preset Which window to build.
 * @param localToday Today's date in the local zone. Used by today, week, and month.
 * @param offsetMinutes Local offset from UTC in minutes, from -840 through 840.
 * @param customStart First local day of a custom window.
 * @param customEnd Last local day of a custom window.
 * @return The UTC window, or an error when a date or the offset is not usable.
 */
[[nodiscard]] Result<UtcRange> boundsForPreset(StatsPreset preset, CivilDate localToday,
                                               int offsetMinutes, CivilDate customStart,
                                               CivilDate customEnd);

}  // namespace timetracker
