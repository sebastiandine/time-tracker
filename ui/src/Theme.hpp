#pragma once

#include "timetracker/domain/Records.hpp"

class QApplication;

namespace timetracker::ui {

/**
 * @brief Apply a light or dark Fusion palette.
 *
 * @c System reads the current operating-system color scheme. Unknown schemes
 * use the light palette.
 * @param preference Saved or chosen theme.
 */
void applyTheme(ThemePreference preference);

}  // namespace timetracker::ui
