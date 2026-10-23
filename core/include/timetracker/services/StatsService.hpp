#pragma once

#include "timetracker/ports/IClock.hpp"
#include "timetracker/ports/ITimeEntryRepository.hpp"
#include "timetracker/time/StatsRange.hpp"

#include <optional>

namespace timetracker {

/**
 * @brief Adds up time inside a UTC window.
 *
 * Not copyable and not thread-safe. An open entry contributes time only up to
 * the clock and only inside the window. References must outlive this object.
 */
class StatsService {
public:
    /**
     * @brief Bind the service to the entry store and clock.
     * @param entries Source of overlapping intervals.
     * @param clock End instant for an entry that is still running.
     */
    StatsService(ITimeEntryRepository& entries, IClock& clock);

    StatsService(const StatsService&) = delete;
    StatsService& operator=(const StatsService&) = delete;

    /**
     * @brief Summarize time that overlaps @p range.
     * @param range Inclusive start and exclusive end, both UTC text.
     * @param categoryId When set, only that category is included.
     * @return Hierarchical totals, or an error when a stored timestamp cannot be read.
     */
    [[nodiscard]] Result<StatsReport> summarize(const UtcRange& range,
                                                std::optional<std::int64_t> categoryId);

private:
    ITimeEntryRepository& entries_;
    IClock& clock_;
};

}  // namespace timetracker
