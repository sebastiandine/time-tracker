#pragma once

#include "timetracker/domain/Records.hpp"
#include "timetracker/util/Result.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace timetracker {

/**
 * @brief Persistence for time entries.
 *
 * Not thread-safe. At most one row has @c running set. That row is committed
 * when timing starts, so a later process exit does not discard the start.
 */
class ITimeEntryRepository {
public:
    virtual ~ITimeEntryRepository() = default;

    /**
     * @brief Insert an open entry.
     * @param draft Target, UTC start, and note. @c startedAt is already formatted.
     * @return The stored row, or an error when a timer is already open or the topic is gone.
     */
    [[nodiscard]] virtual Result<TimeEntry> insertOpen(const TimeEntryDraft& draft) = 0;

    /**
     * @brief Load the open entry, if one exists.
     * @return The row, an empty optional when nothing is running, or an error.
     */
    [[nodiscard]] virtual Result<std::optional<TimeEntry>> findRunning() = 0;

    /**
     * @brief Load one entry.
     * @param id Time entry id.
     * @return The row, or an error when it does not exist.
     */
    [[nodiscard]] virtual Result<TimeEntry> findById(std::int64_t id) = 0;

    /**
     * @brief Close an open entry.
     * @param id Open entry id.
     * @param endedAt UTC end instant.
     * @param notes Note stored with the closed entry.
     * @return The closed row, or an error when that id is not open.
     */
    [[nodiscard]] virtual Result<TimeEntry> closeEntry(std::int64_t id, const std::string& endedAt,
                                                       const std::string& notes) = 0;

    /**
     * @brief Replace the note on an existing entry.
     * @param id Time entry id.
     * @param notes Replacement note.
     * @return The updated row, or an error when the id is missing.
     */
    [[nodiscard]] virtual Result<TimeEntry> updateNotes(std::int64_t id,
                                                        const std::string& notes) = 0;

    /**
     * @brief Delete a closed entry.
     * @param id Closed entry id.
     * @return Success, or an error when the id is missing or still running.
     */
    [[nodiscard]] virtual Result<void> deleteClosed(std::int64_t id) = 0;

    /**
     * @brief List closed entries for one topic, newest first.
     * @param target Main topic or subtopic. Entries on the other kind are excluded.
     * @param limit Maximum rows. Zero or less yields an empty page and does not query.
     * @param cursor Last row of the previous page. Empty starts at the newest row.
     * @return The page, ordered by start descending and id descending.
     */
    [[nodiscard]] virtual Result<std::vector<TimeEntry>> listRecent(
        TimeTarget target, int limit, const std::optional<RecentCursor>& cursor) = 0;

    /**
     * @brief List entries that overlap a UTC window.
     * @param rangeStartUtc Inclusive window start, @c YYYY-MM-DDTHH:MM:SSZ.
     * @param rangeEndUtc Exclusive window end, same format.
     * @param categoryId When set, only entries in that category are returned.
     * @return Overlapping intervals with display names. Open entries are included.
     */
    [[nodiscard]] virtual Result<std::vector<TrackedInterval>> listOverlapping(
        const std::string& rangeStartUtc, const std::string& rangeEndUtc,
        std::optional<std::int64_t> categoryId) = 0;
};

}  // namespace timetracker
