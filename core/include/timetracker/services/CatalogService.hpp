#pragma once

#include "timetracker/ports/ICatalogRepository.hpp"

namespace timetracker {

/**
 * @brief Creates, renames, and deletes the topic catalog.
 *
 * Not copyable and not thread-safe. Names are trimmed and limited to 200
 * Unicode code points. Notes are trimmed and limited to 20,000. Deletes do
 * not cascade: a parent with children, or a topic with time entries, is refused.
 * The repository reference must outlive this object.
 */
class CatalogService {
public:
    /**
     * @brief Bind the service to a catalog store.
     * @param catalog Repository owned by the caller.
     */
    explicit CatalogService(ICatalogRepository& catalog);

    CatalogService(const CatalogService&) = delete;
    CatalogService& operator=(const CatalogService&) = delete;

    /**
     * @brief Create a category.
     * @param name Display name. Blank names are rejected.
     * @param notes Explanation. May be blank.
     * @return The stored row, or a validation or uniqueness error.
     */
    [[nodiscard]] Result<Category> createCategory(std::string name, std::string notes);

    /**
     * @brief Rename a category and replace its notes.
     * @param id Existing category id.
     * @param name New display name.
     * @param notes New explanation.
     * @return The stored row, or an error when validation, lookup, or uniqueness fails.
     */
    [[nodiscard]] Result<Category> updateCategory(std::int64_t id, std::string name,
                                                  std::string notes);

    /**
     * @brief Delete a category that has no main topics.
     * @param id Category id.
     * @return Success, or an error when the category still has topics or does not exist.
     */
    [[nodiscard]] Result<void> deleteCategory(std::int64_t id);

    /**
     * @brief Create a main topic.
     * @param categoryId Parent category.
     * @param name Display name.
     * @param notes Explanation.
     * @return The stored row, or an error when the parent, name, or uniqueness check fails.
     */
    [[nodiscard]] Result<MainTopic> createMainTopic(std::int64_t categoryId, std::string name,
                                                    std::string notes);

    /**
     * @brief Rename a main topic and replace its notes.
     * @param id Existing main topic id.
     * @param name New display name.
     * @param notes New explanation.
     * @return The stored row, or an error when validation, lookup, or uniqueness fails.
     */
    [[nodiscard]] Result<MainTopic> updateMainTopic(std::int64_t id, std::string name,
                                                    std::string notes);

    /**
     * @brief Delete a main topic that has no subtopics and no direct time entries.
     * @param id Main topic id.
     * @return Success, or an error when children, time entries, or the id block the delete.
     */
    [[nodiscard]] Result<void> deleteMainTopic(std::int64_t id);

    /**
     * @brief Create a subtopic.
     * @param mainTopicId Parent main topic.
     * @param name Display name.
     * @param notes Explanation.
     * @return The stored row, or an error when the parent, name, or uniqueness check fails.
     */
    [[nodiscard]] Result<SubTopic> createSubTopic(std::int64_t mainTopicId, std::string name,
                                                  std::string notes);

    /**
     * @brief Rename a subtopic and replace its notes.
     * @param id Existing subtopic id.
     * @param name New display name.
     * @param notes New explanation.
     * @return The stored row, or an error when validation, lookup, or uniqueness fails.
     */
    [[nodiscard]] Result<SubTopic> updateSubTopic(std::int64_t id, std::string name,
                                                  std::string notes);

    /**
     * @brief Delete a subtopic that has no time entries.
     * @param id Subtopic id.
     * @return Success, or an error when time entries exist or the id is missing.
     */
    [[nodiscard]] Result<void> deleteSubTopic(std::int64_t id);

    /**
     * @brief Load the whole catalog.
     * @return Categories, main topics, and subtopics in display order.
     */
    [[nodiscard]] Result<CatalogSnapshot> loadAll();

private:
    ICatalogRepository& catalog_;
};

}  // namespace timetracker
