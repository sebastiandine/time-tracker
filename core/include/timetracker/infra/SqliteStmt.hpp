#pragma once

#include <memory>

struct sqlite3;
struct sqlite3_stmt;

namespace timetracker {

/**
 * @brief Which SQLite constraint a failed write hit.
 */
enum class SqliteConstraint { None, Unique, ForeignKey, NotNull, Check, Other };

/**
 * @brief Releases a prepared statement.
 */
struct SqliteStmtDeleter {
    /**
     * @brief Finalize @p statement.
     * @param statement Statement pointer. Null is ignored.
     */
    void operator()(sqlite3_stmt* statement) const noexcept;
};

/**
 * @brief Owning prepared statement. A failed prepare is an empty pointer.
 */
using SqliteStmt = std::unique_ptr<sqlite3_stmt, SqliteStmtDeleter>;

/**
 * @brief Prepare SQL. On failure the returned pointer is empty and @p rc is not OK.
 * @param db Open connection.
 * @param sql Trusted SQL text. Not user input.
 * @param rc Receives the SQLite result code.
 * @return An owning statement, or empty on failure.
 */
[[nodiscard]] SqliteStmt prepareStmt(sqlite3* db, const char* sql, int& rc);

/**
 * @brief Classify the connection's current extended error.
 * @param db Open connection.
 * @return The matching constraint, or @c Other when the code is not a constraint.
 */
[[nodiscard]] SqliteConstraint classifyConstraint(sqlite3* db) noexcept;

}  // namespace timetracker
