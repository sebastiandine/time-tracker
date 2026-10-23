#pragma once

#include "timetracker/ports/IDatabaseSession.hpp"
#include "timetracker/util/Result.hpp"

#include <filesystem>

struct sqlite3;

namespace timetracker {

/**
 * @brief Schema version this build knows how to open.
 *
 * Bump only by adding a new migration step. A database newer than this value
 * is rejected.
 */
inline constexpr int kSchemaVersion = 1;

/**
 * @brief One SQLite connection.
 *
 * Not thread-safe and not copyable. The composition root owns it and passes
 * it by reference to repositories. @c handle() is for adapters and tests.
 * File databases use WAL. Memory databases skip WAL. After a successful open,
 * an authorizer allows only ordinary reads and writes.
 */
class SqliteDatabase final : public IDatabaseSession {
public:
    SqliteDatabase() = default;
    ~SqliteDatabase() override;

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;

    /**
     * @brief Open or create a database file, migrate it, and install the authorizer.
     * @param dbPath Filesystem path. The parent directory must already exist.
     * @return Success, or an error when open or migration fails.
     */
    [[nodiscard]] Result<void> open(const std::filesystem::path& dbPath) override;

    /**
     * @brief Open a private in-memory database and migrate it.
     * @return Success, or an error when open or migration fails.
     */
    [[nodiscard]] Result<void> openMemory();

    /**
     * @brief Apply migration steps until @c kSchemaVersion.
     * @return Success, or an error when the database is newer or a step fails.
     * @pre The authorizer is not installed. Migration runs SQL that the authorizer denies.
     */
    [[nodiscard]] Result<void> migrate();

    /**
     * @brief Copy this connection into @p destination with the SQLite backup API.
     * @param destination Destination connection. Null fails.
     * @return Success, or an error when the copy does not finish.
     */
    [[nodiscard]] Result<void> backupTo(sqlite3* destination);

    /**
     * @brief Install the write/read authorizer.
     * @return Success, or an error when SQLite rejects the callback.
     */
    [[nodiscard]] Result<void> installAuthorizer();

    /**
     * @brief Borrow the native connection.
     * @return The open handle, or null before a successful open.
     */
    [[nodiscard]] sqlite3* handle() const noexcept { return db_; }

private:
    void close() noexcept;

    /**
     * @brief Open @p path and configure it.
     * @param path SQLite filename, or @c :memory:.
     * @param fileDb True for a file, which enables WAL.
     * @return Success, or an error when open or configuration fails.
     */
    [[nodiscard]] Result<void> openRaw(const char* path, bool fileDb);

    /**
     * @brief Apply connection pragmas, migrate, and install the authorizer.
     * @param fileDb True when WAL should be enabled.
     * @return Success, or the first configuration error.
     */
    [[nodiscard]] Result<void> configure(bool fileDb);

    sqlite3* db_{nullptr};
};

}  // namespace timetracker
