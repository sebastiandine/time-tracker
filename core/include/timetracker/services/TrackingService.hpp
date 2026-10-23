#pragma once

#include "timetracker/ports/ICatalogRepository.hpp"
#include "timetracker/ports/IClock.hpp"
#include "timetracker/ports/ITimeEntryRepository.hpp"

namespace timetracker {

/**
 * @brief How many closed entries the track tab asks for in one page.
 */
inline constexpr int kRecentPageSize = 50;

/**
 * @brief Starts and stops the single open timer.
 *
 * Not copyable and not thread-safe. Start commits @c started_at immediately.
 * Elapsed time is @c now minus that instant, so time while the process is
 * gone still counts. References must outlive this object.
 */
class TrackingService {
public:
    /**
     * @brief Bind the service to its stores and clock.
     * @param catalog Used to reject a start on a missing topic.
     * @param entries Time-entry store.
     * @param clock UTC clock. Tests pass a fake.
     */
    TrackingService(ICatalogRepository& catalog, ITimeEntryRepository& entries, IClock& clock);

    TrackingService(const TrackingService&) = delete;
    TrackingService& operator=(const TrackingService&) = delete;

    /**
     * @brief Start timing a main topic or subtopic.
     * @param target Topic to time. A category is not a valid target.
     * @param notes Note stored on the open row. May be empty.
     * @return The open row, or an error when a timer is already running, the topic
     *         is missing, or the note is too long.
     */
    [[nodiscard]] Result<TimeEntry> start(TimeTarget target, std::string notes);

    /**
     * @brief Stop the open timer and store the note.
     * @param notes Note saved with the closed row.
     * @return The closed row, or an error when nothing is running, the clock is
     *         earlier than the start, or the note is too long.
     */
    [[nodiscard]] Result<TimeEntry> stop(std::string notes);

    /**
     * @brief Replace the note on the open row.
     * @param notes New note.
     * @return The updated row, or an error when nothing is running or the note is too long.
     */
    [[nodiscard]] Result<TimeEntry> updateRunningNote(std::string notes);

    /**
     * @brief Replace the note on a closed row.
     * @param id Closed entry id.
     * @param notes New note.
     * @return The updated row, or an error when the entry is missing, still running,
     *         or the note is too long.
     */
    [[nodiscard]] Result<TimeEntry> updateClosedNote(std::int64_t id, std::string notes);

    /**
     * @brief Delete a closed entry.
     * @param id Closed entry id.
     * @return Success, or an error when the entry is missing or still running.
     */
    [[nodiscard]] Result<void> deleteEntry(std::int64_t id);

    /**
     * @brief Load the open entry.
     * @return The row, an empty optional when nothing is running, or an error.
     */
    [[nodiscard]] Result<std::optional<TimeEntry>> current();

    /**
     * @brief Seconds since the open entry started.
     * @return A non-negative duration. A clock that moved backward yields zero.
     *         An error is returned when nothing is running or the start cannot be read.
     */
    [[nodiscard]] Result<std::int64_t> elapsedSeconds();

    /**
     * @brief List one page of closed entries for a topic.
     * @param target Main topic or subtopic.
     * @param limit Maximum rows. Negative is treated as zero.
     * @param cursor Last row of the previous page, or empty for the newest page.
     * @return The page, newest first.
     */
    [[nodiscard]] Result<std::vector<TimeEntry>> listRecent(
        TimeTarget target, int limit, const std::optional<RecentCursor>& cursor);

private:
    /**
     * @brief Confirm the target topic exists.
     * @param target Main topic or subtopic id.
     * @return Success, or the not-found error from the catalog.
     */
    [[nodiscard]] Result<void> targetExists(TimeTarget target);

    ICatalogRepository& catalog_;
    ITimeEntryRepository& entries_;
    IClock& clock_;
};

}  // namespace timetracker
