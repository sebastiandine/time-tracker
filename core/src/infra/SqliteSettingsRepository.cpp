#include "timetracker/infra/SqliteSettingsRepository.hpp"

#include "SqlUtil.hpp"

namespace timetracker {

SqliteSettingsRepository::SqliteSettingsRepository(SqliteDatabase& database) : db_(database) {}

Result<std::optional<std::string>> SqliteSettingsRepository::get(const std::string& key) {
    auto prepared =
        detail::prepare(db_.handle(), "SELECT value FROM Setting WHERE key = ?;");
    if (prepared.rc != SQLITE_OK) {
        return Result<std::optional<std::string>>::err(detail::kPrepareFailed);
    }
    detail::bindText(prepared.statement.get(), 1, key);
    const int rc = sqlite3_step(prepared.statement.get());
    if (rc == SQLITE_DONE) {
        return Result<std::optional<std::string>>::ok(std::nullopt);
    }
    if (rc != SQLITE_ROW) {
        return Result<std::optional<std::string>>::err(detail::kReadFailed);
    }
    return Result<std::optional<std::string>>::ok(
        detail::columnText(prepared.statement.get(), 0));
}

Result<void> SqliteSettingsRepository::set(const std::string& key, const std::string& value) {
    return detail::writeOne(
        db_.handle(),
        "INSERT INTO Setting (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
        [&](sqlite3_stmt* statement) {
            detail::bindText(statement, 1, key);
            detail::bindText(statement, 2, value);
        },
        "Failed to save the row.", "Failed to save the row.", "Failed to save the row.",
        "Unknown setting.");
}

}  // namespace timetracker
