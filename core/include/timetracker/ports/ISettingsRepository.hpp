#pragma once

#include "timetracker/util/Result.hpp"

#include <optional>
#include <string>

namespace timetracker {

/**
 * @brief Key-value settings stored in the database.
 *
 * Not thread-safe. The only accepted key is @c theme.
 */
class ISettingsRepository {
public:
    virtual ~ISettingsRepository() = default;

    /**
     * @brief Read one setting.
     * @param key Setting name.
     * @return The value, an empty optional when the key is absent, or an error.
     */
    [[nodiscard]] virtual Result<std::optional<std::string>> get(const std::string& key) = 0;

    /**
     * @brief Insert or replace one setting.
     * @param key Setting name. Only @c theme is accepted by the database.
     * @param value New value.
     * @return Success, or an error when the key is not allowed.
     */
    [[nodiscard]] virtual Result<void> set(const std::string& key, const std::string& value) = 0;
};

}  // namespace timetracker
