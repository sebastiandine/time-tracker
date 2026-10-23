#include "timetracker/services/SettingsService.hpp"

namespace timetracker {
namespace {

constexpr const char* kThemeKey = "theme";

[[nodiscard]] Result<ThemePreference> parseTheme(const std::string& value) {
    if (value == "system") {
        return Result<ThemePreference>::ok(ThemePreference::System);
    }
    if (value == "light") {
        return Result<ThemePreference>::ok(ThemePreference::Light);
    }
    if (value == "dark") {
        return Result<ThemePreference>::ok(ThemePreference::Dark);
    }
    return Result<ThemePreference>::err("Unknown theme.");
}

[[nodiscard]] const char* themeText(ThemePreference theme) {
    switch (theme) {
    case ThemePreference::System:
        return "system";
    case ThemePreference::Light:
        return "light";
    case ThemePreference::Dark:
        return "dark";
    }
    return "system";
}

}  // namespace

SettingsService::SettingsService(ISettingsRepository& settings) : settings_(settings) {}

Result<ThemePreference> SettingsService::theme() {
    auto stored = settings_.get(kThemeKey);
    if (!stored) {
        return Result<ThemePreference>::err(stored.error());
    }
    if (!stored.value().has_value()) {
        return Result<ThemePreference>::ok(ThemePreference::System);
    }
    return parseTheme(*stored.value());
}

Result<void> SettingsService::setTheme(ThemePreference theme) {
    return settings_.set(kThemeKey, themeText(theme));
}

}  // namespace timetracker
