#pragma once

#include "timetracker/infra/SqliteStmt.hpp"
#include "timetracker/util/Result.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace timetracker::detail {

/**
 * @brief Message used when a statement cannot be prepared.
 */
inline constexpr const char* kPrepareFailed = "Failed to prepare statement.";

/**
 * @brief Message used when a read does not finish.
 */
inline constexpr const char* kReadFailed = "Failed to read rows.";

/**
 * @brief Message used for a write failure that is not a known constraint.
 */
inline constexpr const char* kSaveFailed = "Failed to save the row.";

/**
 * @brief Message used when a check constraint rejects a row.
 */
inline constexpr const char* kCheckFailed = "The row failed a database check.";

/**
 * @brief A prepare attempt. @c rc is the SQLite code and @c statement is empty on failure.
 */
struct Prepared {
    SqliteStmt statement;
    int rc{0};
};

/**
 * @brief Prepare trusted SQL.
 * @param db Open connection.
 * @param sql SQL text owned by the program.
 * @return The statement and result code.
 */
[[nodiscard]] inline Prepared prepare(sqlite3* db, const char* sql) {
    Prepared prepared;
    prepared.statement = prepareStmt(db, sql, prepared.rc);
    return prepared;
}

/**
 * @brief Bind text, including embedded null bytes, as a transient copy.
 * @param statement Prepared statement.
 * @param index 1-based parameter index.
 * @param value Bytes to copy into the statement.
 */
inline void bindText(sqlite3_stmt* statement, int index, std::string_view value) {
    sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

/**
 * @brief Copy a text column. Null becomes an empty string.
 * @param statement Statement positioned on a row.
 * @param index 0-based column index.
 * @return An owning copy of the column text.
 */
[[nodiscard]] inline std::string columnText(sqlite3_stmt* statement, int index) {
    const auto* text = sqlite3_column_text(statement, index);
    if (text == nullptr) {
        return {};
    }
    const auto size = sqlite3_column_bytes(statement, index);
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(size));
}

/**
 * @brief Copy a nullable text column.
 * @param statement Statement positioned on a row.
 * @param index 0-based column index.
 * @return The text, or empty when the column is null.
 */
[[nodiscard]] inline std::optional<std::string> columnOptionalText(sqlite3_stmt* statement,
                                                                   int index) {
    if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
        return std::nullopt;
    }
    return columnText(statement, index);
}

/**
 * @brief Read a nullable integer id.
 * @param statement Statement positioned on a row.
 * @param index 0-based column index.
 * @return The id, or empty when the column is null.
 */
[[nodiscard]] inline std::optional<std::int64_t> columnOptionalId(sqlite3_stmt* statement,
                                                                  int index) {
    if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_int64(statement, index);
}

/**
 * @brief Map the current SQLite constraint to a caller-facing sentence.
 * @param db Connection whose extended error is current.
 * @param uniqueMessage Text for a unique violation.
 * @param foreignKeyMessage Text for a foreign-key violation.
 * @param checkMessage Text for a check violation.
 * @return The matching sentence. Other failures use @c kSaveFailed.
 */
[[nodiscard]] std::string writeFailureMessage(sqlite3* db, std::string_view uniqueMessage,
                                              std::string_view foreignKeyMessage,
                                              std::string_view checkMessage);

/**
 * @brief Insert one row and return its new id.
 * @param db Open connection.
 * @param sql Insert statement with bound parameters.
 * @param bind Writes the parameters.
 * @param uniqueMessage Text for a unique violation.
 * @param foreignKeyMessage Text for a foreign-key violation.
 * @param checkMessage Text for a check violation.
 * @return The new row id, or a prepare or constraint error.
 */
template <typename Bind>
[[nodiscard]] Result<std::int64_t> insertReturningId(sqlite3* db, const char* sql, Bind bind,
                                                     std::string_view uniqueMessage,
                                                     std::string_view foreignKeyMessage,
                                                     std::string_view checkMessage) {
    auto prepared = prepare(db, sql);
    if (prepared.rc != SQLITE_OK) {
        return Result<std::int64_t>::err(kPrepareFailed);
    }
    bind(prepared.statement.get());
    const int rc = sqlite3_step(prepared.statement.get());
    if (rc != SQLITE_DONE) {
        return Result<std::int64_t>::err(
            writeFailureMessage(db, uniqueMessage, foreignKeyMessage, checkMessage));
    }
    return Result<std::int64_t>::ok(sqlite3_last_insert_rowid(db));
}

/**
 * @brief Run an update or delete that must change one row.
 * @param db Open connection.
 * @param sql Statement with bound parameters.
 * @param bind Writes the parameters.
 * @param missingMessage Text when no row changed.
 * @param uniqueMessage Text for a unique violation.
 * @param foreignKeyMessage Text for a foreign-key violation.
 * @param checkMessage Text for a check violation.
 * @return Success, or a prepare, constraint, or missing-row error.
 */
template <typename Bind>
[[nodiscard]] Result<void> writeOne(sqlite3* db, const char* sql, Bind bind,
                                    const char* missingMessage, std::string_view uniqueMessage,
                                    std::string_view foreignKeyMessage,
                                    std::string_view checkMessage) {
    auto prepared = prepare(db, sql);
    if (prepared.rc != SQLITE_OK) {
        return Result<void>::err(kPrepareFailed);
    }
    bind(prepared.statement.get());
    const int rc = sqlite3_step(prepared.statement.get());
    if (rc != SQLITE_DONE) {
        return Result<void>::err(
            writeFailureMessage(db, uniqueMessage, foreignKeyMessage, checkMessage));
    }
    if (sqlite3_changes(db) == 0) {
        return Result<void>::err(missingMessage);
    }
    return Result<void>::ok();
}

/**
 * @brief Read every row of a statement that has no parameters.
 * @param db Open connection.
 * @param sql Select statement.
 * @param read Maps the current row. It must copy text out before it returns.
 * @return The rows, or a prepare or read error.
 */
template <typename Row, typename Read>
[[nodiscard]] Result<std::vector<Row>> listAll(sqlite3* db, const char* sql, Read read) {
    auto prepared = prepare(db, sql);
    if (prepared.rc != SQLITE_OK) {
        return Result<std::vector<Row>>::err(kPrepareFailed);
    }
    std::vector<Row> rows;
    int rc = 0;
    while ((rc = sqlite3_step(prepared.statement.get())) == SQLITE_ROW) {
        rows.push_back(read(prepared.statement.get()));
    }
    if (rc != SQLITE_DONE) {
        return Result<std::vector<Row>>::err(kReadFailed);
    }
    return Result<std::vector<Row>>::ok(std::move(rows));
}

/**
 * @brief Read the row with one bound id.
 * @param db Open connection.
 * @param sql Select statement with one parameter.
 * @param id Bound id.
 * @param read Maps the current row.
 * @param missingMessage Text when no row is found.
 * @return The row, or a prepare, read, or missing-row error.
 */
template <typename Row, typename Read>
[[nodiscard]] Result<Row> findById(sqlite3* db, const char* sql, std::int64_t id, Read read,
                                   const char* missingMessage) {
    auto prepared = prepare(db, sql);
    if (prepared.rc != SQLITE_OK) {
        return Result<Row>::err(kPrepareFailed);
    }
    sqlite3_bind_int64(prepared.statement.get(), 1, id);
    const int rc = sqlite3_step(prepared.statement.get());
    if (rc == SQLITE_DONE) {
        return Result<Row>::err(missingMessage);
    }
    if (rc != SQLITE_ROW) {
        return Result<Row>::err(kReadFailed);
    }
    return Result<Row>::ok(read(prepared.statement.get()));
}

/**
 * @brief Report whether a one-parameter existence query returns a row.
 * @param db Open connection.
 * @param sql Select statement that returns a row when the condition holds.
 * @param id Bound id.
 * @return True when a row exists, or a prepare or read error.
 */
[[nodiscard]] inline Result<bool> rowExists(sqlite3* db, const char* sql, std::int64_t id) {
    auto prepared = prepare(db, sql);
    if (prepared.rc != SQLITE_OK) {
        return Result<bool>::err(kPrepareFailed);
    }
    sqlite3_bind_int64(prepared.statement.get(), 1, id);
    const int rc = sqlite3_step(prepared.statement.get());
    if (rc == SQLITE_DONE) {
        return Result<bool>::ok(false);
    }
    if (rc != SQLITE_ROW) {
        return Result<bool>::err(kReadFailed);
    }
    return Result<bool>::ok(true);
}

}  // namespace timetracker::detail
