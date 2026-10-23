#pragma once

#include "timetracker/infra/SqliteDatabase.hpp"
#include "timetracker/ports/ITimeEntryRepository.hpp"

namespace timetracker {

/**
 * @brief SQLite implementation of @c ITimeEntryRepository.
 *
 * Not thread-safe. The referenced database must outlive this object.
 */
class SqliteTimeEntryRepository final : public ITimeEntryRepository {
public:
    /**
     * @brief Bind the repository to an open database.
     * @param database Connection owned by the caller. Must outlive this object.
     */
    explicit SqliteTimeEntryRepository(SqliteDatabase& database);

    [[nodiscard]] Result<TimeEntry> insertOpen(const TimeEntryDraft& draft) override;
    [[nodiscard]] Result<std::optional<TimeEntry>> findRunning() override;
    [[nodiscard]] Result<TimeEntry> findById(std::int64_t id) override;
    [[nodiscard]] Result<TimeEntry> closeEntry(std::int64_t id, const std::string& endedAt,
                                               const std::string& notes) override;
    [[nodiscard]] Result<TimeEntry> updateNotes(std::int64_t id, const std::string& notes) override;
    [[nodiscard]] Result<void> deleteClosed(std::int64_t id) override;
    [[nodiscard]] Result<std::vector<TimeEntry>> listRecent(
        TimeTarget target, int limit, const std::optional<RecentCursor>& cursor) override;
    [[nodiscard]] Result<std::vector<TrackedInterval>> listOverlapping(
        const std::string& rangeStartUtc, const std::string& rangeEndUtc,
        std::optional<std::int64_t> categoryId) override;

private:
    SqliteDatabase& db_;
};

}  // namespace timetracker
