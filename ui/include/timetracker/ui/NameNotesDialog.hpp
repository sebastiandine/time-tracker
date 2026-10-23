#pragma once

#include <QDialog>
#include <QString>

#include <optional>
#include <utility>

class QLineEdit;
class QPlainTextEdit;

namespace timetracker::ui {

/**
 * @brief Dialog that collects a name and a note for a new catalog row.
 */
class NameNotesDialog final : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Create the dialog.
     * @param title Window title.
     * @param parent Parent widget.
     */
    NameNotesDialog(const QString& title, QWidget* parent);

    /**
     * @brief Name typed by the user.
     * @return The name field.
     */
    [[nodiscard]] QString name() const;

    /**
     * @brief Notes typed by the user.
     * @return The notes field.
     */
    [[nodiscard]] QString notes() const;

    /**
     * @brief Ask for a name and notes.
     * @param parent Parent widget.
     * @param title Window title.
     * @return The trimmed fields, or empty when the user cancels or the name is blank.
     */
    [[nodiscard]] static std::optional<std::pair<QString, QString>> ask(QWidget* parent,
                                                                        const QString& title);

    /**
     * @brief Ask for a replacement note.
     * @param parent Parent widget.
     * @param current Note shown when the dialog opens.
     * @return The new note, or empty when the user cancels.
     */
    [[nodiscard]] static std::optional<QString> askNote(QWidget* parent, const QString& current);

private:
    QLineEdit* nameEdit_{nullptr};
    QPlainTextEdit* notesEdit_{nullptr};
};

}  // namespace timetracker::ui
