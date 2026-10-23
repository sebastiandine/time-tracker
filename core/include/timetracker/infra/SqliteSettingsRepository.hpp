#pragma once

#include "timetracker/infra/SqliteDatabase.hpp"
#include "timetracker/ports/ISettingsRepository.hpp"

namespace timetracker {

/**
 * @brief SQLite implementation of @c ISettingsRepository.
 *
 * Not thread-safe. The referenced database must outlive this object.
 */
class SqliteSettingsRepository final : public ISettingsRepository {
public:
    /**
     * @brief Bind the repository to an open database.
     * @param database Connection owned by the caller. Must outlive this object.
     */
    explicit SqliteSettingsRepository(SqliteDatabase& database);

    [[nodiscard]] Result<std::optional<std::string>> get(const std::string& key) override;
    [[nodiscard]] Result<void> set(const std::string& key, const std::string& value) override;

private:
    SqliteDatabase& db_;
};

}  // namespace timetracker
