#include "timetracker/ui/LibraryPage.hpp"

#include "CatalogTree.hpp"
#include "UiUtil.hpp"
#include "timetracker/ui/NameNotesDialog.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace timetracker::ui {

LibraryPage::LibraryPage(CatalogService& catalog, QWidget* parent)
    : QWidget(parent), catalog_(catalog) {
    buildLayout();
}

void LibraryPage::buildLayout() {
    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    emptyLabel_ = new QLabel(QStringLiteral("No categories yet. Add a category to begin."), this);

    nameEdit_ = new QLineEdit(this);
    notesEdit_ = new QPlainTextEdit(this);
    notesEdit_->setPlaceholderText(QStringLiteral("Optional notes"));
    auto* form = new QFormLayout();
    form->addRow(QStringLiteral("Name"), nameEdit_);
    form->addRow(QStringLiteral("Notes"), notesEdit_);

    auto* addCategory = new QPushButton(QStringLiteral("Add category"), this);
    addMainButton_ = new QPushButton(QStringLiteral("Add main topic"), this);
    addSubButton_ = new QPushButton(QStringLiteral("Add subtopic"), this);
    saveButton_ = new QPushButton(QStringLiteral("Save"), this);
    deleteButton_ = new QPushButton(QStringLiteral("Delete"), this);
    auto* buttons = new QHBoxLayout();
    buttons->addWidget(addCategory);
    buttons->addWidget(addMainButton_);
    buttons->addWidget(addSubButton_);
    buttons->addStretch();
    buttons->addWidget(saveButton_);
    buttons->addWidget(deleteButton_);

    auto* editor = new QWidget(this);
    auto* editorLayout = new QVBoxLayout(editor);
    editorLayout->addLayout(form);
    editorLayout->addLayout(buttons);
    editorLayout->addStretch();

    auto* left = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->addWidget(tree_);
    leftLayout->addWidget(emptyLabel_);

    auto* splitter = new QSplitter(this);
    splitter->addWidget(left);
    splitter->addWidget(editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(splitter);

    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &LibraryPage::loadEditor);
    connect(addCategory, &QPushButton::clicked, this, &LibraryPage::onAddCategory);
    connect(addMainButton_, &QPushButton::clicked, this, &LibraryPage::onAddMainTopic);
    connect(addSubButton_, &QPushButton::clicked, this, &LibraryPage::onAddSubTopic);
    connect(saveButton_, &QPushButton::clicked, this, &LibraryPage::onSave);
    connect(deleteButton_, &QPushButton::clicked, this, &LibraryPage::onDelete);
    loadEditor();
}

void LibraryPage::setSnapshot(const CatalogSnapshot& snapshot) {
    snapshot_ = snapshot;
    const auto current = readSelection(tree_->currentItem());
    const auto desiredKind = pendingSelect_ ? pendingSelect_->first : TreeNodeKind::Category;
    const auto desiredId = pendingSelect_ ? pendingSelect_->second : 0;
    const bool usePending = pendingSelect_.has_value();
    pendingSelect_.reset();
    const QSignalBlocker blocker(tree_);
    fillCatalogTree(tree_, snapshot_);
    if (usePending) {
        selectNode(tree_, desiredKind, desiredId);
    } else if (current.has_value()) {
        selectNode(tree_, current->kind, current->id);
    }
    emptyLabel_->setVisible(snapshot_.categories.empty());
    loadEditor();
}

void LibraryPage::loadEditor() {
    const auto selection = readSelection(tree_->currentItem());
    const bool selected = selection.has_value();
    nameEdit_->setEnabled(selected);
    notesEdit_->setEnabled(selected);
    saveButton_->setEnabled(selected);
    deleteButton_->setEnabled(selected);
    addMainButton_->setEnabled(selected);
    addSubButton_->setEnabled(selected && selection->kind != TreeNodeKind::Category);
    if (!selected) {
        nameEdit_->clear();
        notesEdit_->clear();
        return;
    }
    const auto fields = nameAndNotes(snapshot_, *selection);
    nameEdit_->setText(QString::fromStdString(fields.first));
    notesEdit_->setPlainText(QString::fromStdString(fields.second));
}

void LibraryPage::onAddCategory() {
    auto answer = NameNotesDialog::ask(this, QStringLiteral("Add category"));
    if (!answer.has_value()) {
        return;
    }
    auto created = catalog_.createCategory(answer->first.toStdString(), answer->second.toStdString());
    if (!created) {
        showError(this, created.error());
        return;
    }
    pendingSelect_ = std::make_pair(TreeNodeKind::Category, created.value().id);
    emit catalogChanged();
}

void LibraryPage::onAddMainTopic() {
    const auto selection = readSelection(tree_->currentItem());
    if (!selection.has_value()) {
        return;
    }
    auto answer = NameNotesDialog::ask(this, QStringLiteral("Add main topic"));
    if (!answer.has_value()) {
        return;
    }
    auto created = catalog_.createMainTopic(selection->categoryId, answer->first.toStdString(),
                                            answer->second.toStdString());
    if (!created) {
        showError(this, created.error());
        return;
    }
    pendingSelect_ = std::make_pair(TreeNodeKind::MainTopic, created.value().id);
    emit catalogChanged();
}

void LibraryPage::onAddSubTopic() {
    const auto selection = readSelection(tree_->currentItem());
    if (!selection.has_value() || selection->kind == TreeNodeKind::Category) {
        return;
    }
    const auto mainId =
        selection->kind == TreeNodeKind::MainTopic ? selection->id : selection->parentId;
    auto answer = NameNotesDialog::ask(this, QStringLiteral("Add subtopic"));
    if (!answer.has_value()) {
        return;
    }
    auto created =
        catalog_.createSubTopic(mainId, answer->first.toStdString(), answer->second.toStdString());
    if (!created) {
        showError(this, created.error());
        return;
    }
    pendingSelect_ = std::make_pair(TreeNodeKind::SubTopic, created.value().id);
    emit catalogChanged();
}

void LibraryPage::onSave() {
    const auto selection = readSelection(tree_->currentItem());
    if (!selection.has_value()) {
        return;
    }
    const auto name = nameEdit_->text().toStdString();
    const auto notes = notesEdit_->toPlainText().toStdString();
    bool saved = false;
    std::string error;
    if (selection->kind == TreeNodeKind::Category) {
        auto updated = catalog_.updateCategory(selection->id, name, notes);
        saved = updated.succeeded();
        error = updated.error();
    } else if (selection->kind == TreeNodeKind::MainTopic) {
        auto updated = catalog_.updateMainTopic(selection->id, name, notes);
        saved = updated.succeeded();
        error = updated.error();
    } else {
        auto updated = catalog_.updateSubTopic(selection->id, name, notes);
        saved = updated.succeeded();
        error = updated.error();
    }
    if (!saved) {
        showError(this, error);
        return;
    }
    pendingSelect_ = std::make_pair(selection->kind, selection->id);
    emit catalogChanged();
}

void LibraryPage::onDelete() {
    const auto selection = readSelection(tree_->currentItem());
    if (!selection.has_value()) {
        return;
    }
    if (!confirmDelete(this, nameEdit_->text())) {
        return;
    }
    Result<void> deleted = Result<void>::err("Nothing is selected.");
    if (selection->kind == TreeNodeKind::Category) {
        deleted = catalog_.deleteCategory(selection->id);
    } else if (selection->kind == TreeNodeKind::MainTopic) {
        deleted = catalog_.deleteMainTopic(selection->id);
    } else {
        deleted = catalog_.deleteSubTopic(selection->id);
    }
    if (!deleted) {
        showError(this, deleted.error());
        return;
    }
    emit catalogChanged();
}

}  // namespace timetracker::ui
