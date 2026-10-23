#pragma once

#include "timetracker/domain/Records.hpp"
#include "timetracker/ports/ISettingsRepository.hpp"

namespace timetracker {

/**
 * @brief Reads and writes the theme preference.
 *
 * Not copyable and not thread-safe. A missing row means @c System.
 * The repository reference must outlive this object.
 */
class SettingsService {
public:
    /**
     * @brief Bind the service to the settings store.
     * @param settings Repository owned by the caller.
     */
    explicit SettingsService(ISettingsRepository& settings);

    SettingsService(const SettingsService&) = delete;
    SettingsService& operator=(const SettingsService&) = delete;

    /**
     * @brief Load the saved theme.
     * @return @c System when no theme has been saved, or an error when the stored
     *         value is not a known theme or the read fails.
     */
    [[nodiscard]] Result<ThemePreference> theme();

    /**
     * @brief Save the theme.
     * @param theme Preference to store.
     * @return Success, or an error when the write fails.
     */
    [[nodiscard]] Result<void> setTheme(ThemePreference theme);

private:
    ISettingsRepository& settings_;
};

}  // namespace timetracker
