#pragma once

#include "timetracker/domain/Records.hpp"

#include <QTreeWidget>

#include <cstdint>
#include <optional>
#include <utility>

class QTreeWidgetItem;

namespace timetracker::ui {

/**
 * @brief Kind of node stored on a catalog tree item.
 */
enum class TreeNodeKind { Category = 1, MainTopic = 2, SubTopic = 3 };

/**
 * @brief The topic selected in a catalog tree.
 */
struct TreeSelection {
    TreeNodeKind kind{TreeNodeKind::Category};
    std::int64_t id{0};
    std::int64_t parentId{0};
    std::int64_t categoryId{0};
};

/**
 * @brief Replace the tree contents with the catalog.
 * @param tree Destination. Existing items are removed.
 * @param snapshot Catalog in display order.
 */
void fillCatalogTree(QTreeWidget* tree, const CatalogSnapshot& snapshot);

/**
 * @brief Read the ids stored on one item.
 * @param item Tree item, or null.
 * @return The selection, or empty when @p item is null.
 */
[[nodiscard]] std::optional<TreeSelection> readSelection(const QTreeWidgetItem* item);

/**
 * @brief Select the item with this kind and id, if it is in the tree.
 * @param tree Tree to search.
 * @param kind Node kind.
 * @param id Node id.
 */
void selectNode(QTreeWidget* tree, TreeNodeKind kind, std::int64_t id);

/**
 * @brief Name and notes for the selected catalog row.
 * @param snapshot Current catalog.
 * @param selection Selected node.
 * @return The stored name and notes, or empty strings when the row is gone.
 */
[[nodiscard]] std::pair<std::string, std::string> nameAndNotes(const CatalogSnapshot& snapshot,
                                                               const TreeSelection& selection);

/**
 * @brief Category / main / subtopic label for a time entry.
 * @param snapshot Current catalog.
 * @param entry Time entry whose topic names are needed.
 * @return A slash-separated label, or "Unknown topic" when the topic is missing.
 */
[[nodiscard]] QString topicPath(const CatalogSnapshot& snapshot, const TimeEntry& entry);

}  // namespace timetracker::ui
