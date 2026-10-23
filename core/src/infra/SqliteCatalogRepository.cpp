#include "timetracker/infra/SqliteCatalogRepository.hpp"

#include "SqlUtil.hpp"

namespace timetracker {
namespace {

[[nodiscard]] Category readCategory(sqlite3_stmt* statement) {
    Category row;
    row.id = sqlite3_column_int64(statement, 0);
    row.name = detail::columnText(statement, 1);
    row.notes = detail::columnText(statement, 2);
    return row;
}

[[nodiscard]] MainTopic readMainTopic(sqlite3_stmt* statement) {
    MainTopic row;
    row.id = sqlite3_column_int64(statement, 0);
    row.categoryId = sqlite3_column_int64(statement, 1);
    row.name = detail::columnText(statement, 2);
    row.notes = detail::columnText(statement, 3);
    return row;
}

[[nodiscard]] SubTopic readSubTopic(sqlite3_stmt* statement) {
    SubTopic row;
    row.id = sqlite3_column_int64(statement, 0);
    row.mainTopicId = sqlite3_column_int64(statement, 1);
    row.name = detail::columnText(statement, 2);
    row.notes = detail::columnText(statement, 3);
    return row;
}

void bindNameNotes(sqlite3_stmt* statement, const std::string& name, const std::string& notes) {
    detail::bindText(statement, 1, name);
    detail::bindText(statement, 2, notes);
}

}  // namespace

SqliteCatalogRepository::SqliteCatalogRepository(SqliteDatabase& database) : db_(database) {}

Result<Category> SqliteCatalogRepository::createCategory(const std::string& name,
                                                         const std::string& notes) {
    auto id = detail::insertReturningId(
        db_.handle(), "INSERT INTO Category (name, notes) VALUES (?, ?);",
        [&](sqlite3_stmt* statement) { bindNameNotes(statement, name, notes); },
        "A category with that name already exists.", "Failed to save the row.",
        detail::kCheckFailed);
    if (!id) {
        return Result<Category>::err(id.error());
    }
    Category row;
    row.id = id.value();
    row.name = name;
    row.notes = notes;
    return Result<Category>::ok(std::move(row));
}

Result<Category> SqliteCatalogRepository::updateCategory(std::int64_t id, const std::string& name,
                                                         const std::string& notes) {
    auto written = detail::writeOne(
        db_.handle(), "UPDATE Category SET name = ?, notes = ? WHERE id = ?;",
        [&](sqlite3_stmt* statement) {
            bindNameNotes(statement, name, notes);
            sqlite3_bind_int64(statement, 3, id);
        },
        "Category not found.", "A category with that name already exists.",
        "Failed to save the row.", detail::kCheckFailed);
    if (!written) {
        return Result<Category>::err(written.error());
    }
    Category row;
    row.id = id;
    row.name = name;
    row.notes = notes;
    return Result<Category>::ok(std::move(row));
}

Result<void> SqliteCatalogRepository::deleteCategory(std::int64_t id) {
    return detail::writeOne(
        db_.handle(), "DELETE FROM Category WHERE id = ?;",
        [&](sqlite3_stmt* statement) { sqlite3_bind_int64(statement, 1, id); },
        "Category not found.", "Failed to save the row.", "Failed to save the row.",
        detail::kCheckFailed);
}

Result<std::vector<Category>> SqliteCatalogRepository::listCategories() {
    return detail::listAll<Category>(
        db_.handle(), "SELECT id, name, notes FROM Category ORDER BY name COLLATE NOCASE ASC, id ASC;",
        readCategory);
}

Result<Category> SqliteCatalogRepository::findCategory(std::int64_t id) {
    return detail::findById<Category>(
        db_.handle(), "SELECT id, name, notes FROM Category WHERE id = ?;", id, readCategory,
        "Category not found.");
}

Result<MainTopic> SqliteCatalogRepository::createMainTopic(std::int64_t categoryId,
                                                           const std::string& name,
                                                           const std::string& notes) {
    auto id = detail::insertReturningId(
        db_.handle(), "INSERT INTO MainTopic (category_id, name, notes) VALUES (?, ?, ?);",
        [&](sqlite3_stmt* statement) {
            sqlite3_bind_int64(statement, 1, categoryId);
            detail::bindText(statement, 2, name);
            detail::bindText(statement, 3, notes);
        },
        "A main topic with that name already exists.", "Category not found.", detail::kCheckFailed);
    if (!id) {
        return Result<MainTopic>::err(id.error());
    }
    MainTopic row;
    row.id = id.value();
    row.categoryId = categoryId;
    row.name = name;
    row.notes = notes;
    return Result<MainTopic>::ok(std::move(row));
}

Result<MainTopic> SqliteCatalogRepository::updateMainTopic(std::int64_t id, const std::string& name,
                                                           const std::string& notes) {
    auto written = detail::writeOne(
        db_.handle(), "UPDATE MainTopic SET name = ?, notes = ? WHERE id = ?;",
        [&](sqlite3_stmt* statement) {
            bindNameNotes(statement, name, notes);
            sqlite3_bind_int64(statement, 3, id);
        },
        "Main topic not found.", "A main topic with that name already exists.",
        "Failed to save the row.", detail::kCheckFailed);
    if (!written) {
        return Result<MainTopic>::err(written.error());
    }
    return findMainTopic(id);
}

Result<void> SqliteCatalogRepository::deleteMainTopic(std::int64_t id) {
    return detail::writeOne(
        db_.handle(), "DELETE FROM MainTopic WHERE id = ?;",
        [&](sqlite3_stmt* statement) { sqlite3_bind_int64(statement, 1, id); },
        "Main topic not found.", "Failed to save the row.", "Delete the subtopics in this topic first.",
        detail::kCheckFailed);
}

Result<std::vector<MainTopic>> SqliteCatalogRepository::listMainTopics() {
    return detail::listAll<MainTopic>(
        db_.handle(),
        "SELECT id, category_id, name, notes FROM MainTopic "
        "ORDER BY category_id ASC, name COLLATE NOCASE ASC, id ASC;",
        readMainTopic);
}

Result<MainTopic> SqliteCatalogRepository::findMainTopic(std::int64_t id) {
    return detail::findById<MainTopic>(
        db_.handle(), "SELECT id, category_id, name, notes FROM MainTopic WHERE id = ?;", id,
        readMainTopic, "Main topic not found.");
}

Result<SubTopic> SqliteCatalogRepository::createSubTopic(std::int64_t mainTopicId,
                                                         const std::string& name,
                                                         const std::string& notes) {
    auto id = detail::insertReturningId(
        db_.handle(), "INSERT INTO SubTopic (main_topic_id, name, notes) VALUES (?, ?, ?);",
        [&](sqlite3_stmt* statement) {
            sqlite3_bind_int64(statement, 1, mainTopicId);
            detail::bindText(statement, 2, name);
            detail::bindText(statement, 3, notes);
        },
        "A subtopic with that name already exists.", "Main topic not found.", detail::kCheckFailed);
    if (!id) {
        return Result<SubTopic>::err(id.error());
    }
    SubTopic row;
    row.id = id.value();
    row.mainTopicId = mainTopicId;
    row.name = name;
    row.notes = notes;
    return Result<SubTopic>::ok(std::move(row));
}

Result<SubTopic> SqliteCatalogRepository::updateSubTopic(std::int64_t id, const std::string& name,
                                                         const std::string& notes) {
    auto written = detail::writeOne(
        db_.handle(), "UPDATE SubTopic SET name = ?, notes = ? WHERE id = ?;",
        [&](sqlite3_stmt* statement) {
            bindNameNotes(statement, name, notes);
            sqlite3_bind_int64(statement, 3, id);
        },
        "Subtopic not found.", "A subtopic with that name already exists.", "Failed to save the row.",
        detail::kCheckFailed);
    if (!written) {
        return Result<SubTopic>::err(written.error());
    }
    return findSubTopic(id);
}

Result<void> SqliteCatalogRepository::deleteSubTopic(std::int64_t id) {
    return detail::writeOne(
        db_.handle(), "DELETE FROM SubTopic WHERE id = ?;",
        [&](sqlite3_stmt* statement) { sqlite3_bind_int64(statement, 1, id); },
        "Subtopic not found.", "Failed to save the row.",
        "Delete the time entries on this subtopic first.", detail::kCheckFailed);
}

Result<std::vector<SubTopic>> SqliteCatalogRepository::listSubTopics() {
    return detail::listAll<SubTopic>(
        db_.handle(),
        "SELECT id, main_topic_id, name, notes FROM SubTopic "
        "ORDER BY main_topic_id ASC, name COLLATE NOCASE ASC, id ASC;",
        readSubTopic);
}

Result<SubTopic> SqliteCatalogRepository::findSubTopic(std::int64_t id) {
    return detail::findById<SubTopic>(
        db_.handle(), "SELECT id, main_topic_id, name, notes FROM SubTopic WHERE id = ?;", id,
        readSubTopic, "Subtopic not found.");
}

Result<bool> SqliteCatalogRepository::categoryHasMainTopics(std::int64_t categoryId) {
    return detail::rowExists(db_.handle(),
                             "SELECT 1 FROM MainTopic WHERE category_id = ? LIMIT 1;", categoryId);
}

Result<bool> SqliteCatalogRepository::mainTopicHasSubTopics(std::int64_t mainTopicId) {
    return detail::rowExists(
        db_.handle(), "SELECT 1 FROM SubTopic WHERE main_topic_id = ? LIMIT 1;", mainTopicId);
}

Result<bool> SqliteCatalogRepository::mainTopicHasTimeEntries(std::int64_t mainTopicId) {
    return detail::rowExists(
        db_.handle(), "SELECT 1 FROM TimeEntry WHERE main_topic_id = ? LIMIT 1;", mainTopicId);
}

Result<bool> SqliteCatalogRepository::subTopicHasTimeEntries(std::int64_t subTopicId) {
    return detail::rowExists(db_.handle(),
                             "SELECT 1 FROM TimeEntry WHERE sub_topic_id = ? LIMIT 1;", subTopicId);
}

}  // namespace timetracker
