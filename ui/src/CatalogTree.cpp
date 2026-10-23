#include "CatalogTree.hpp"

#include <QTreeWidgetItemIterator>

namespace timetracker::ui {
namespace {

constexpr int kKindRole = Qt::UserRole;
constexpr int kIdRole = Qt::UserRole + 1;
constexpr int kParentRole = Qt::UserRole + 2;
constexpr int kCategoryRole = Qt::UserRole + 3;

void setNode(QTreeWidgetItem* item, TreeNodeKind kind, std::int64_t id, std::int64_t parentId,
             std::int64_t categoryId, const std::string& name) {
    item->setText(0, QString::fromStdString(name));
    item->setData(0, kKindRole, static_cast<int>(kind));
    item->setData(0, kIdRole, static_cast<qlonglong>(id));
    item->setData(0, kParentRole, static_cast<qlonglong>(parentId));
    item->setData(0, kCategoryRole, static_cast<qlonglong>(categoryId));
}

[[nodiscard]] const Category* findCategory(const CatalogSnapshot& snapshot, std::int64_t id) {
    for (const auto& category : snapshot.categories) {
        if (category.id == id) {
            return &category;
        }
    }
    return nullptr;
}

[[nodiscard]] const MainTopic* findMain(const CatalogSnapshot& snapshot, std::int64_t id) {
    for (const auto& topic : snapshot.mainTopics) {
        if (topic.id == id) {
            return &topic;
        }
    }
    return nullptr;
}

[[nodiscard]] const SubTopic* findSub(const CatalogSnapshot& snapshot, std::int64_t id) {
    for (const auto& topic : snapshot.subTopics) {
        if (topic.id == id) {
            return &topic;
        }
    }
    return nullptr;
}

}  // namespace

void fillCatalogTree(QTreeWidget* tree, const CatalogSnapshot& snapshot) {
    tree->clear();
    tree->setHeaderHidden(true);
    for (const auto& category : snapshot.categories) {
        auto* categoryItem = new QTreeWidgetItem(tree);
        setNode(categoryItem, TreeNodeKind::Category, category.id, 0, category.id, category.name);
        for (const auto& mainTopic : snapshot.mainTopics) {
            if (mainTopic.categoryId != category.id) {
                continue;
            }
            auto* mainItem = new QTreeWidgetItem(categoryItem);
            setNode(mainItem, TreeNodeKind::MainTopic, mainTopic.id, category.id, category.id,
                    mainTopic.name);
            for (const auto& subTopic : snapshot.subTopics) {
                if (subTopic.mainTopicId != mainTopic.id) {
                    continue;
                }
                auto* subItem = new QTreeWidgetItem(mainItem);
                setNode(subItem, TreeNodeKind::SubTopic, subTopic.id, mainTopic.id, category.id,
                        subTopic.name);
            }
        }
        categoryItem->setExpanded(true);
    }
    tree->expandAll();
}

std::optional<TreeSelection> readSelection(const QTreeWidgetItem* item) {
    if (item == nullptr) {
        return std::nullopt;
    }
    TreeSelection selection;
    selection.kind = static_cast<TreeNodeKind>(item->data(0, kKindRole).toInt());
    selection.id = item->data(0, kIdRole).toLongLong();
    selection.parentId = item->data(0, kParentRole).toLongLong();
    selection.categoryId = item->data(0, kCategoryRole).toLongLong();
    return selection;
}

void selectNode(QTreeWidget* tree, TreeNodeKind kind, std::int64_t id) {
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        const auto selection = readSelection(*it);
        if (selection.has_value() && selection->kind == kind && selection->id == id) {
            tree->setCurrentItem(*it);
            return;
        }
    }
}

std::pair<std::string, std::string> nameAndNotes(const CatalogSnapshot& snapshot,
                                                 const TreeSelection& selection) {
    if (selection.kind == TreeNodeKind::Category) {
        const auto* category = findCategory(snapshot, selection.id);
        if (category != nullptr) {
            return {category->name, category->notes};
        }
    } else if (selection.kind == TreeNodeKind::MainTopic) {
        const auto* topic = findMain(snapshot, selection.id);
        if (topic != nullptr) {
            return {topic->name, topic->notes};
        }
    } else {
        const auto* topic = findSub(snapshot, selection.id);
        if (topic != nullptr) {
            return {topic->name, topic->notes};
        }
    }
    return {};
}

QString topicPath(const CatalogSnapshot& snapshot, const TimeEntry& entry) {
    if (entry.subTopicId.has_value()) {
        const auto* sub = findSub(snapshot, *entry.subTopicId);
        const auto* main = sub != nullptr ? findMain(snapshot, sub->mainTopicId) : nullptr;
        const auto* category = main != nullptr ? findCategory(snapshot, main->categoryId) : nullptr;
        if (sub != nullptr && main != nullptr && category != nullptr) {
            return QString("%1 / %2 / %3")
                .arg(QString::fromStdString(category->name), QString::fromStdString(main->name),
                     QString::fromStdString(sub->name));
        }
    }
    if (entry.mainTopicId.has_value()) {
        const auto* main = findMain(snapshot, *entry.mainTopicId);
        const auto* category = main != nullptr ? findCategory(snapshot, main->categoryId) : nullptr;
        if (main != nullptr && category != nullptr) {
            return QString("%1 / %2")
                .arg(QString::fromStdString(category->name), QString::fromStdString(main->name));
        }
    }
    return QStringLiteral("Unknown topic");
}

}  // namespace timetracker::ui
