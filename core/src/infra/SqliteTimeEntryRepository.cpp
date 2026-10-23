#include "timetracker/infra/SqliteTimeEntryRepository.hpp"

#include "SqlUtil.hpp"

namespace timetracker {
namespace {

constexpr const char* kEntryColumns =
    "id, main_topic_id, sub_topic_id, started_at, ended_at, is_running, notes";

constexpr const char* kInsertMain =
    "INSERT INTO TimeEntry (main_topic_id, sub_topic_id, started_at, ended_at, is_running, notes) "
    "VALUES (?, NULL, ?, NULL, 1, ?);";

constexpr const char* kInsertSub =
    "INSERT INTO TimeEntry (main_topic_id, sub_topic_id, started_at, ended_at, is_running, notes) "
    "VALUES (NULL, ?, ?, NULL, 1, ?);";

constexpr const char* kMainFirst =
    "SELECT id, main_topic_id, sub_topic_id, started_at, ended_at, is_running, notes "
    "FROM TimeEntry WHERE main_topic_id = ? AND is_running = 0 "
    "ORDER BY started_at DESC, id DESC LIMIT ?;";

constexpr const char* kMainNext =
    "SELECT id, main_topic_id, sub_topic_id, started_at, ended_at, is_running, notes "
    "FROM TimeEntry WHERE main_topic_id = ? AND is_running = 0 "
    "AND (started_at < ? OR (started_at = ? AND id < ?)) "
    "ORDER BY started_at DESC, id DESC LIMIT ?;";

constexpr const char* kSubFirst =
    "SELECT id, main_topic_id, sub_topic_id, started_at, ended_at, is_running, notes "
    "FROM TimeEntry WHERE sub_topic_id = ? AND is_running = 0 "
    "ORDER BY started_at DESC, id DESC LIMIT ?;";

constexpr const char* kSubNext =
    "SELECT id, main_topic_id, sub_topic_id, started_at, ended_at, is_running, notes "
    "FROM TimeEntry WHERE sub_topic_id = ? AND is_running = 0 "
    "AND (started_at < ? OR (started_at = ? AND id < ?)) "
    "ORDER BY started_at DESC, id DESC LIMIT ?;";

constexpr const char* kOverlap =
    "SELECT c.id, c.name, "
    "CASE WHEN e.main_topic_id IS NOT NULL THEN m.id ELSE sm.id END, "
    "CASE WHEN e.main_topic_id IS NOT NULL THEN m.name ELSE sm.name END, "
    "s.id, s.name, e.started_at, e.ended_at "
    "FROM TimeEntry AS e "
    "LEFT JOIN MainTopic AS m ON m.id = e.main_topic_id "
    "LEFT JOIN SubTopic AS s ON s.id = e.sub_topic_id "
    "LEFT JOIN MainTopic AS sm ON sm.id = s.main_topic_id "
    "JOIN Category AS c ON c.id = COALESCE(m.category_id, sm.category_id) "
    "WHERE e.started_at < ? AND (e.ended_at IS NULL OR e.ended_at > ?);";

constexpr const char* kOverlapInCategory =
    "SELECT c.id, c.name, "
    "CASE WHEN e.main_topic_id IS NOT NULL THEN m.id ELSE sm.id END, "
    "CASE WHEN e.main_topic_id IS NOT NULL THEN m.name ELSE sm.name END, "
    "s.id, s.name, e.started_at, e.ended_at "
    "FROM TimeEntry AS e "
    "LEFT JOIN MainTopic AS m ON m.id = e.main_topic_id "
    "LEFT JOIN SubTopic AS s ON s.id = e.sub_topic_id "
    "LEFT JOIN MainTopic AS sm ON sm.id = s.main_topic_id "
    "JOIN Category AS c ON c.id = COALESCE(m.category_id, sm.category_id) "
    "WHERE e.started_at < ? AND (e.ended_at IS NULL OR e.ended_at > ?) AND c.id = ?;";

[[nodiscard]] TimeEntry readTimeEntry(sqlite3_stmt* statement) {
    TimeEntry row;
    row.id = sqlite3_column_int64(statement, 0);
    row.mainTopicId = detail::columnOptionalId(statement, 1);
    row.subTopicId = detail::columnOptionalId(statement, 2);
    row.startedAt = detail::columnText(statement, 3);
    row.endedAt = detail::columnOptionalText(statement, 4);
    row.running = sqlite3_column_int(statement, 5) != 0;
    row.notes = detail::columnText(statement, 6);
    return row;
}

[[nodiscard]] TrackedInterval readInterval(sqlite3_stmt* statement) {
    TrackedInterval row;
    row.categoryId = sqlite3_column_int64(statement, 0);
    row.categoryName = detail::columnText(statement, 1);
    row.mainTopicId = sqlite3_column_int64(statement, 2);
    row.mainTopicName = detail::columnText(statement, 3);
    row.subTopicId = detail::columnOptionalId(statement, 4);
    row.subTopicName = detail::columnText(statement, 5);
    row.startedAt = detail::columnText(statement, 6);
    row.endedAt = detail::columnOptionalText(statement, 7);
    return row;
}

[[nodiscard]] const char* recentSql(TimeTarget target, bool hasCursor) {
    if (target.kind == TimeTargetKind::MainTopic) {
        return hasCursor ? kMainNext : kMainFirst;
    }
    return hasCursor ? kSubNext : kSubFirst;
}

void bindRecent(sqlite3_stmt* statement, TimeTarget target, int limit,
                const std::optional<RecentCursor>& cursor) {
    sqlite3_bind_int64(statement, 1, target.id);
    if (!cursor.has_value()) {
        sqlite3_bind_int(statement, 2, limit);
        return;
    }
    detail::bindText(statement, 2, cursor->startedAt);
    detail::bindText(statement, 3, cursor->startedAt);
    sqlite3_bind_int64(statement, 4, cursor->id);
    sqlite3_bind_int(statement, 5, limit);
}

[[nodiscard]] Result<std::vector<TrackedInterval>> readIntervals(sqlite3_stmt* statement) {
    std::vector<TrackedInterval> rows;
    int rc = 0;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (sqlite3_column_type(statement, 2) == SQLITE_NULL) {
            return Result<std::vector<TrackedInterval>>::err("A time entry is missing its topic.");
        }
        rows.push_back(readInterval(statement));
    }
    if (rc != SQLITE_DONE) {
        return Result<std::vector<TrackedInterval>>::err(detail::kReadFailed);
    }
    return Result<std::vector<TrackedInterval>>::ok(std::move(rows));
}

}  // namespace

SqliteTimeEntryRepository::SqliteTimeEntryRepository(SqliteDatabase& database) : db_(database) {}

Result<TimeEntry> SqliteTimeEntryRepository::insertOpen(const TimeEntryDraft& draft) {
    const char* sql =
        draft.target.kind == TimeTargetKind::MainTopic ? kInsertMain : kInsertSub;
    auto id = detail::insertReturningId(
        db_.handle(), sql,
        [&](sqlite3_stmt* statement) {
            sqlite3_bind_int64(statement, 1, draft.target.id);
            detail::bindText(statement, 2, draft.startedAt);
            detail::bindText(statement, 3, draft.notes);
        },
        "A timer is already running.", "That topic no longer exists.", detail::kCheckFailed);
    if (!id) {
        return Result<TimeEntry>::err(id.error());
    }
    TimeEntry row;
    row.id = id.value();
    if (draft.target.kind == TimeTargetKind::MainTopic) {
        row.mainTopicId = draft.target.id;
    } else {
        row.subTopicId = draft.target.id;
    }
    row.startedAt = draft.startedAt;
    row.running = true;
    row.notes = draft.notes;
    return Result<TimeEntry>::ok(std::move(row));
}

Result<std::optional<TimeEntry>> SqliteTimeEntryRepository::findRunning() {
    const std::string sql = std::string("SELECT ") + kEntryColumns +
                            " FROM TimeEntry WHERE is_running = 1;";
    auto prepared = detail::prepare(db_.handle(), sql.c_str());
    if (prepared.rc != SQLITE_OK) {
        return Result<std::optional<TimeEntry>>::err(detail::kPrepareFailed);
    }
    const int rc = sqlite3_step(prepared.statement.get());
    if (rc == SQLITE_DONE) {
        return Result<std::optional<TimeEntry>>::ok(std::nullopt);
    }
    if (rc != SQLITE_ROW) {
        return Result<std::optional<TimeEntry>>::err(detail::kReadFailed);
    }
    return Result<std::optional<TimeEntry>>::ok(readTimeEntry(prepared.statement.get()));
}

Result<TimeEntry> SqliteTimeEntryRepository::findById(std::int64_t id) {
    const std::string sql =
        std::string("SELECT ") + kEntryColumns + " FROM TimeEntry WHERE id = ?;";
    return detail::findById<TimeEntry>(db_.handle(), sql.c_str(), id, readTimeEntry,
                                       "Time entry not found.");
}

Result<TimeEntry> SqliteTimeEntryRepository::closeEntry(std::int64_t id, const std::string& endedAt,
                                                        const std::string& notes) {
    auto written = detail::writeOne(
        db_.handle(),
        "UPDATE TimeEntry SET ended_at = ?, is_running = 0, notes = ? "
        "WHERE id = ? AND is_running = 1;",
        [&](sqlite3_stmt* statement) {
            detail::bindText(statement, 1, endedAt);
            detail::bindText(statement, 2, notes);
            sqlite3_bind_int64(statement, 3, id);
        },
        "Time entry not found.", "Failed to save the row.", "Failed to save the row.",
        detail::kCheckFailed);
    if (!written) {
        return Result<TimeEntry>::err(written.error());
    }
    return findById(id);
}

Result<TimeEntry> SqliteTimeEntryRepository::updateNotes(std::int64_t id, const std::string& notes) {
    auto written = detail::writeOne(
        db_.handle(), "UPDATE TimeEntry SET notes = ? WHERE id = ?;",
        [&](sqlite3_stmt* statement) {
            detail::bindText(statement, 1, notes);
            sqlite3_bind_int64(statement, 2, id);
        },
        "Time entry not found.", "Failed to save the row.", "Failed to save the row.",
        detail::kCheckFailed);
    if (!written) {
        return Result<TimeEntry>::err(written.error());
    }
    return findById(id);
}

Result<void> SqliteTimeEntryRepository::deleteClosed(std::int64_t id) {
    return detail::writeOne(
        db_.handle(), "DELETE FROM TimeEntry WHERE id = ? AND is_running = 0;",
        [&](sqlite3_stmt* statement) { sqlite3_bind_int64(statement, 1, id); },
        "Time entry not found.", "Failed to save the row.", "Failed to save the row.",
        detail::kCheckFailed);
}

Result<std::vector<TimeEntry>> SqliteTimeEntryRepository::listRecent(
    TimeTarget target, int limit, const std::optional<RecentCursor>& cursor) {
    if (limit <= 0) {
        return Result<std::vector<TimeEntry>>::ok({});
    }
    auto prepared = detail::prepare(db_.handle(), recentSql(target, cursor.has_value()));
    if (prepared.rc != SQLITE_OK) {
        return Result<std::vector<TimeEntry>>::err(detail::kPrepareFailed);
    }
    bindRecent(prepared.statement.get(), target, limit, cursor);
    std::vector<TimeEntry> rows;
    int rc = 0;
    while ((rc = sqlite3_step(prepared.statement.get())) == SQLITE_ROW) {
        rows.push_back(readTimeEntry(prepared.statement.get()));
    }
    if (rc != SQLITE_DONE) {
        return Result<std::vector<TimeEntry>>::err(detail::kReadFailed);
    }
    return Result<std::vector<TimeEntry>>::ok(std::move(rows));
}

Result<std::vector<TrackedInterval>> SqliteTimeEntryRepository::listOverlapping(
    const std::string& rangeStartUtc, const std::string& rangeEndUtc,
    std::optional<std::int64_t> categoryId) {
    const char* sql = categoryId.has_value() ? kOverlapInCategory : kOverlap;
    auto prepared = detail::prepare(db_.handle(), sql);
    if (prepared.rc != SQLITE_OK) {
        return Result<std::vector<TrackedInterval>>::err(detail::kPrepareFailed);
    }
    detail::bindText(prepared.statement.get(), 1, rangeEndUtc);
    detail::bindText(prepared.statement.get(), 2, rangeStartUtc);
    if (categoryId.has_value()) {
        sqlite3_bind_int64(prepared.statement.get(), 3, *categoryId);
    }
    return readIntervals(prepared.statement.get());
}

}  // namespace timetracker
