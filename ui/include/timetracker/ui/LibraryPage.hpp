#pragma once

#include "CatalogTree.hpp"
#include "timetracker/domain/Records.hpp"
#include "timetracker/services/CatalogService.hpp"

#include <QWidget>

#include <optional>
#include <utility>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;

namespace timetracker::ui {

/**
 * @brief Create, rename, and delete categories, main topics, and subtopics.
 *
 * The catalog service must outlive this page.
 */
class LibraryPage final : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Build the library page.
     * @param catalog Catalog service.
     * @param parent Parent widget.
     */
    explicit LibraryPage(CatalogService& catalog, QWidget* parent = nullptr);

    /**
     * @brief Replace the tree with a new catalog snapshot.
     * @param snapshot Current catalog.
     */
    void setSnapshot(const CatalogSnapshot& snapshot);

signals:
    /**
     * @brief The catalog was created, renamed, or deleted.
     */
    void catalogChanged();

private:
    void buildLayout();
    void loadEditor();
    void onAddCategory();
    void onAddMainTopic();
    void onAddSubTopic();
    void onSave();
    void onDelete();

    CatalogService& catalog_;
    CatalogSnapshot snapshot_;
    std::optional<std::pair<TreeNodeKind, std::int64_t>> pendingSelect_;
    QTreeWidget* tree_{nullptr};
    QLabel* emptyLabel_{nullptr};
    QLineEdit* nameEdit_{nullptr};
    QPlainTextEdit* notesEdit_{nullptr};
    QPushButton* addMainButton_{nullptr};
    QPushButton* addSubButton_{nullptr};
    QPushButton* saveButton_{nullptr};
    QPushButton* deleteButton_{nullptr};
};

}  // namespace timetracker::ui
