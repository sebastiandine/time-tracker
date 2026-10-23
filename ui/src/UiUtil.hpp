#pragma once

#include <QMessageBox>
#include <QString>
#include <QWidget>

#include <string>

namespace timetracker::ui {

/**
 * @brief Show a warning for a service error.
 * @param parent Parent window.
 * @param message Error text from a @c Result.
 */
inline void showError(QWidget* parent, const std::string& message) {
    QMessageBox::warning(parent, QStringLiteral("Time Tracker"), QString::fromStdString(message));
}

/**
 * @brief Ask before a delete.
 * @param parent Parent window.
 * @param label Name of the row being deleted.
 * @return True when the user confirms.
 */
inline bool confirmDelete(QWidget* parent, const QString& label) {
    const auto answer = QMessageBox::question(
        parent, QStringLiteral("Time Tracker"),
        QStringLiteral("Delete \"%1\"? This cannot be undone.").arg(label));
    return answer == QMessageBox::Yes;
}

}  // namespace timetracker::ui
