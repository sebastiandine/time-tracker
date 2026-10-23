#pragma once

#include "timetracker/domain/Records.hpp"
#include "timetracker/util/Result.hpp"

#include <cstdint>
#include <vector>

namespace timetracker {

/**
 * @brief Persistence for categories, main topics, and subtopics.
 *
 * Not thread-safe. Callers validate names and notes before writing. Delete
 * methods do not cascade; the service refuses a delete that still has children.
 */
class ICatalogRepository {
public:
    virtual ~ICatalogRepository() = default;

    /**
     * @brief Insert a category.
     * @param name Trimmed display name.
     * @param notes Trimmed explanation. May be empty.
     * @return The stored row, or an error when the name is already used.
     */
    [[nodiscard]] virtual Result<Category> createCategory(const std::string& name,
                                                          const std::string& notes) = 0;

    /**
     * @brief Replace a category's name and notes.
     * @param id Existing category id.
     * @param name Trimmed display name.
     * @param notes Trimmed explanation. May be empty.
     * @return The stored row, or an error when the id is missing or the name is taken.
     */
    [[nodiscard]] virtual Result<Category> updateCategory(std::int64_t id, const std::string& name,
                                                          const std::string& notes) = 0;

    /**
     * @brief Delete a category that the caller has already checked is empty.
     * @param id Category id.
     * @return Success, or an error when the id is missing.
     */
    [[nodiscard]] virtual Result<void> deleteCategory(std::int64_t id) = 0;

    /**
     * @brief List every category.
     * @return Rows ordered by ASCII case-insensitive name, then id.
     */
    [[nodiscard]] virtual Result<std::vector<Category>> listCategories() = 0;

    /**
     * @brief Load one category.
     * @param id Category id.
     * @return The row, or an error when it does not exist.
     */
    [[nodiscard]] virtual Result<Category> findCategory(std::int64_t id) = 0;

    /**
     * @brief Insert a main topic.
     * @param categoryId Parent category.
     * @param name Trimmed display name.
     * @param notes Trimmed explanation. May be empty.
     * @return The stored row, or an error when the parent is missing or the name is taken.
     */
    [[nodiscard]] virtual Result<MainTopic> createMainTopic(std::int64_t categoryId,
                                                            const std::string& name,
                                                            const std::string& notes) = 0;

    /**
     * @brief Replace a main topic's name and notes.
     * @param id Existing main topic id.
     * @param name Trimmed display name.
     * @param notes Trimmed explanation. May be empty.
     * @return The stored row, or an error when the id is missing or the name is taken.
     */
    [[nodiscard]] virtual Result<MainTopic> updateMainTopic(std::int64_t id, const std::string& name,
                                                            const std::string& notes) = 0;

    /**
     * @brief Delete a main topic that the caller has already checked is empty.
     * @param id Main topic id.
     * @return Success, or an error when the id is missing.
     */
    [[nodiscard]] virtual Result<void> deleteMainTopic(std::int64_t id) = 0;

    /**
     * @brief List every main topic.
     * @return Rows ordered by category id, then ASCII case-insensitive name, then id.
     */
    [[nodiscard]] virtual Result<std::vector<MainTopic>> listMainTopics() = 0;

    /**
     * @brief Load one main topic.
     * @param id Main topic id.
     * @return The row, or an error when it does not exist.
     */
    [[nodiscard]] virtual Result<MainTopic> findMainTopic(std::int64_t id) = 0;

    /**
     * @brief Insert a subtopic.
     * @param mainTopicId Parent main topic.
     * @param name Trimmed display name.
     * @param notes Trimmed explanation. May be empty.
     * @return The stored row, or an error when the parent is missing or the name is taken.
     */
    [[nodiscard]] virtual Result<SubTopic> createSubTopic(std::int64_t mainTopicId,
                                                          const std::string& name,
                                                          const std::string& notes) = 0;

    /**
     * @brief Replace a subtopic's name and notes.
     * @param id Existing subtopic id.
     * @param name Trimmed display name.
     * @param notes Trimmed explanation. May be empty.
     * @return The stored row, or an error when the id is missing or the name is taken.
     */
    [[nodiscard]] virtual Result<SubTopic> updateSubTopic(std::int64_t id, const std::string& name,
                                                          const std::string& notes) = 0;

    /**
     * @brief Delete a subtopic that the caller has already checked has no time entries.
     * @param id Subtopic id.
     * @return Success, or an error when the id is missing.
     */
    [[nodiscard]] virtual Result<void> deleteSubTopic(std::int64_t id) = 0;

    /**
     * @brief List every subtopic.
     * @return Rows ordered by main topic id, then ASCII case-insensitive name, then id.
     */
    [[nodiscard]] virtual Result<std::vector<SubTopic>> listSubTopics() = 0;

    /**
     * @brief Load one subtopic.
     * @param id Subtopic id.
     * @return The row, or an error when it does not exist.
     */
    [[nodiscard]] virtual Result<SubTopic> findSubTopic(std::int64_t id) = 0;

    /**
     * @brief Report whether a category still has main topics.
     * @param categoryId Category id.
     * @return True when at least one main topic exists.
     */
    [[nodiscard]] virtual Result<bool> categoryHasMainTopics(std::int64_t categoryId) = 0;

    /**
     * @brief Report whether a main topic still has subtopics.
     * @param mainTopicId Main topic id.
     * @return True when at least one subtopic exists.
     */
    [[nodiscard]] virtual Result<bool> mainTopicHasSubTopics(std::int64_t mainTopicId) = 0;

    /**
     * @brief Report whether time was logged directly on a main topic.
     * @param mainTopicId Main topic id.
     * @return True when at least one time entry points at the main topic.
     */
    [[nodiscard]] virtual Result<bool> mainTopicHasTimeEntries(std::int64_t mainTopicId) = 0;

    /**
     * @brief Report whether time was logged on a subtopic.
     * @param subTopicId Subtopic id.
     * @return True when at least one time entry points at the subtopic.
     */
    [[nodiscard]] virtual Result<bool> subTopicHasTimeEntries(std::int64_t subTopicId) = 0;
};

}  // namespace timetracker
