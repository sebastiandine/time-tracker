#include "timetracker/ui/NameNotesDialog.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace timetracker::ui {

NameNotesDialog::NameNotesDialog(const QString& title, QWidget* parent) : QDialog(parent) {
    setWindowTitle(title);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("Name"), this));
    nameEdit_ = new QLineEdit(this);
    layout->addWidget(nameEdit_);
    layout->addWidget(new QLabel(QStringLiteral("Notes"), this));
    notesEdit_ = new QPlainTextEdit(this);
    notesEdit_->setPlaceholderText(QStringLiteral("Optional notes"));
    layout->addWidget(notesEdit_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(buttons);
    connect(nameEdit_, &QLineEdit::textChanged, this, [buttons](const QString& text) {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!text.trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    resize(420, 280);
    nameEdit_->setFocus();
}

QString NameNotesDialog::name() const { return nameEdit_->text(); }

QString NameNotesDialog::notes() const { return notesEdit_->toPlainText(); }

std::optional<std::pair<QString, QString>> NameNotesDialog::ask(QWidget* parent,
                                                                const QString& title) {
    NameNotesDialog dialog(title, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return std::make_pair(dialog.name().trimmed(), dialog.notes());
}

std::optional<QString> NameNotesDialog::askNote(QWidget* parent, const QString& current) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Edit note"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* notes = new QPlainTextEdit(current, &dialog);
    layout->addWidget(notes);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.resize(420, 220);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return notes->toPlainText();
}

}  // namespace timetracker::ui
