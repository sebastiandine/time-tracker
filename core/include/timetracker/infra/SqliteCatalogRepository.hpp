#pragma once

#include "timetracker/infra/SqliteDatabase.hpp"
#include "timetracker/ports/ICatalogRepository.hpp"

namespace timetracker {

/**
 * @brief SQLite implementation of @c ICatalogRepository.
 *
 * Not thread-safe. The referenced database must outlive this object.
 */
class SqliteCatalogRepository final : public ICatalogRepository {
public:
    /**
     * @brief Bind the repository to an open database.
     * @param database Connection owned by the caller. Must outlive this object.
     */
    explicit SqliteCatalogRepository(SqliteDatabase& database);

    [[nodiscard]] Result<Category> createCategory(const std::string& name,
                                                  const std::string& notes) override;
    [[nodiscard]] Result<Category> updateCategory(std::int64_t id, const std::string& name,
                                                  const std::string& notes) override;
    [[nodiscard]] Result<void> deleteCategory(std::int64_t id) override;
    [[nodiscard]] Result<std::vector<Category>> listCategories() override;
    [[nodiscard]] Result<Category> findCategory(std::int64_t id) override;

    [[nodiscard]] Result<MainTopic> createMainTopic(std::int64_t categoryId, const std::string& name,
                                                    const std::string& notes) override;
    [[nodiscard]] Result<MainTopic> updateMainTopic(std::int64_t id, const std::string& name,
                                                    const std::string& notes) override;
    [[nodiscard]] Result<void> deleteMainTopic(std::int64_t id) override;
    [[nodiscard]] Result<std::vector<MainTopic>> listMainTopics() override;
    [[nodiscard]] Result<MainTopic> findMainTopic(std::int64_t id) override;

    [[nodiscard]] Result<SubTopic> createSubTopic(std::int64_t mainTopicId, const std::string& name,
                                                  const std::string& notes) override;
    [[nodiscard]] Result<SubTopic> updateSubTopic(std::int64_t id, const std::string& name,
                                                  const std::string& notes) override;
    [[nodiscard]] Result<void> deleteSubTopic(std::int64_t id) override;
    [[nodiscard]] Result<std::vector<SubTopic>> listSubTopics() override;
    [[nodiscard]] Result<SubTopic> findSubTopic(std::int64_t id) override;

    [[nodiscard]] Result<bool> categoryHasMainTopics(std::int64_t categoryId) override;
    [[nodiscard]] Result<bool> mainTopicHasSubTopics(std::int64_t mainTopicId) override;
    [[nodiscard]] Result<bool> mainTopicHasTimeEntries(std::int64_t mainTopicId) override;
    [[nodiscard]] Result<bool> subTopicHasTimeEntries(std::int64_t subTopicId) override;

private:
    SqliteDatabase& db_;
};

}  // namespace timetracker
