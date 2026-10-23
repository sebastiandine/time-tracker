#include "timetracker/services/CatalogService.hpp"

#include "text/TextRules.hpp"

namespace timetracker {

CatalogService::CatalogService(ICatalogRepository& catalog) : catalog_(catalog) {}

Result<Category> CatalogService::createCategory(std::string name, std::string notes) {
    auto fields = detail::validatedNameAndNotes(name, notes);
    if (!fields) {
        return Result<Category>::err(fields.error());
    }
    return catalog_.createCategory(fields.value().name, fields.value().notes);
}

Result<Category> CatalogService::updateCategory(std::int64_t id, std::string name,
                                                std::string notes) {
    auto fields = detail::validatedNameAndNotes(name, notes);
    if (!fields) {
        return Result<Category>::err(fields.error());
    }
    return catalog_.updateCategory(id, fields.value().name, fields.value().notes);
}

Result<void> CatalogService::deleteCategory(std::int64_t id) {
    auto children = catalog_.categoryHasMainTopics(id);
    if (!children) {
        return Result<void>::err(children.error());
    }
    if (children.value()) {
        return Result<void>::err("Delete the main topics in this category first.");
    }
    return catalog_.deleteCategory(id);
}

Result<MainTopic> CatalogService::createMainTopic(std::int64_t categoryId, std::string name,
                                                  std::string notes) {
    auto fields = detail::validatedNameAndNotes(name, notes);
    if (!fields) {
        return Result<MainTopic>::err(fields.error());
    }
    auto parent = catalog_.findCategory(categoryId);
    if (!parent) {
        return Result<MainTopic>::err(parent.error());
    }
    return catalog_.createMainTopic(categoryId, fields.value().name, fields.value().notes);
}

Result<MainTopic> CatalogService::updateMainTopic(std::int64_t id, std::string name,
                                                  std::string notes) {
    auto fields = detail::validatedNameAndNotes(name, notes);
    if (!fields) {
        return Result<MainTopic>::err(fields.error());
    }
    return catalog_.updateMainTopic(id, fields.value().name, fields.value().notes);
}

Result<void> CatalogService::deleteMainTopic(std::int64_t id) {
    auto subtopics = catalog_.mainTopicHasSubTopics(id);
    if (!subtopics) {
        return Result<void>::err(subtopics.error());
    }
    if (subtopics.value()) {
        return Result<void>::err("Delete the subtopics in this topic first.");
    }
    auto entries = catalog_.mainTopicHasTimeEntries(id);
    if (!entries) {
        return Result<void>::err(entries.error());
    }
    if (entries.value()) {
        return Result<void>::err("Delete the time entries on this topic first.");
    }
    return catalog_.deleteMainTopic(id);
}

Result<SubTopic> CatalogService::createSubTopic(std::int64_t mainTopicId, std::string name,
                                                std::string notes) {
    auto fields = detail::validatedNameAndNotes(name, notes);
    if (!fields) {
        return Result<SubTopic>::err(fields.error());
    }
    auto parent = catalog_.findMainTopic(mainTopicId);
    if (!parent) {
        return Result<SubTopic>::err(parent.error());
    }
    return catalog_.createSubTopic(mainTopicId, fields.value().name, fields.value().notes);
}

Result<SubTopic> CatalogService::updateSubTopic(std::int64_t id, std::string name,
                                                std::string notes) {
    auto fields = detail::validatedNameAndNotes(name, notes);
    if (!fields) {
        return Result<SubTopic>::err(fields.error());
    }
    return catalog_.updateSubTopic(id, fields.value().name, fields.value().notes);
}

Result<void> CatalogService::deleteSubTopic(std::int64_t id) {
    auto entries = catalog_.subTopicHasTimeEntries(id);
    if (!entries) {
        return Result<void>::err(entries.error());
    }
    if (entries.value()) {
        return Result<void>::err("Delete the time entries on this subtopic first.");
    }
    return catalog_.deleteSubTopic(id);
}

Result<CatalogSnapshot> CatalogService::loadAll() {
    auto categories = catalog_.listCategories();
    if (!categories) {
        return Result<CatalogSnapshot>::err(categories.error());
    }
    auto mainTopics = catalog_.listMainTopics();
    if (!mainTopics) {
        return Result<CatalogSnapshot>::err(mainTopics.error());
    }
    auto subTopics = catalog_.listSubTopics();
    if (!subTopics) {
        return Result<CatalogSnapshot>::err(subTopics.error());
    }
    CatalogSnapshot snapshot;
    snapshot.categories = std::move(categories.value());
    snapshot.mainTopics = std::move(mainTopics.value());
    snapshot.subTopics = std::move(subTopics.value());
    return Result<CatalogSnapshot>::ok(std::move(snapshot));
}

}  // namespace timetracker
