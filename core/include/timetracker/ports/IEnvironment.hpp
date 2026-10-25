#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace timetracker {

/**
 * @brief Read-only view of process environment variables.
 *
 * An empty value is reported as missing. Not thread-safe.
 */
class IEnvironment {
public:
    virtual ~IEnvironment() = default;

    /**
     * @brief Look up one variable.
     * @param name Variable name, such as @c LOCALAPPDATA or @c HOME.
     * @return The value, or empty when the variable is unset or blank.
     */
    [[nodiscard]] virtual std::optional<std::string> get(std::string_view name) const = 0;

    /**
     * @brief List variables that have a non-empty value.
     * @return Name and value pairs. Blank values are omitted. Order is not significant.
     */
    [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>> entries() const = 0;
};

}  // namespace timetracker
