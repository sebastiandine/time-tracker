#pragma once

#include "timetracker/domain/Records.hpp"
#include "timetracker/util/Result.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace timetracker {

/**
 * @brief Format a UTC instant as @c YYYY-MM-DDTHH:MM:SSZ.
 * @param instant Instant truncated toward zero seconds by the function.
 * @return The text form, or an error when the instant cannot be formatted.
 */
[[nodiscard]] Result<std::string> formatUtc(std::chrono::system_clock::time_point instant);

/**
 * @brief Parse @c YYYY-MM-DDTHH:MM:SSZ.
 * @param text Exact 20-character UTC timestamp.
 * @return The instant, or an error when the text is not that form or the date is impossible.
 */
[[nodiscard]] Result<std::chrono::system_clock::time_point> parseUtc(std::string_view text);

/**
 * @brief Whole seconds from @p startUtc to @p endUtc.
 * @param startUtc Inclusive start, @c YYYY-MM-DDTHH:MM:SSZ.
 * @param endUtc Exclusive end in the same form. Equal instants are zero seconds.
 * @return The non-negative duration, or an error when a timestamp is invalid or the end is earlier.
 */
[[nodiscard]] Result<std::int64_t> secondsBetweenUtc(std::string_view startUtc,
                                                     std::string_view endUtc);

/**
 * @brief Format a duration as @c H:MM:SS.
 *
 * Hours are not wrapped at 24. Negative input is shown as @c 0:00:00.
 * @param seconds Duration in whole seconds.
 * @return The display text.
 */
[[nodiscard]] std::string formatDuration(std::int64_t seconds);

}  // namespace timetracker
