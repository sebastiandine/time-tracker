#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace timetracker {

/**
 * @brief A top-level grouping. Time is not logged on a category.
 */
struct Category {
    std::int64_t id{0};
    std::string name;
    std::string notes;
};

/**
 * @brief A topic inside one category. Time may be logged on it directly.
 */
struct MainTopic {
    std::int64_t id{0};
    std::int64_t categoryId{0};
    std::string name;
    std::string notes;
};

/**
 * @brief A topic inside one main topic. Time may be logged on it.
 */
struct SubTopic {
    std::int64_t id{0};
    std::int64_t mainTopicId{0};
    std::string name;
    std::string notes;
};

/**
 * @brief Which kind of topic a timer or history query refers to.
 */
enum class TimeTargetKind { MainTopic, SubTopic };

/**
 * @brief Identity of a main topic or a subtopic. Never a category.
 */
struct TimeTarget {
    TimeTargetKind kind{TimeTargetKind::MainTopic};
    std::int64_t id{0};
};

/**
 * @brief One recorded interval.
 *
 * Exactly one of @c mainTopicId and @c subTopicId is set. @c running is true
 * exactly when @c endedAt is empty. Instants are UTC @c YYYY-MM-DDTHH:MM:SSZ.
 */
struct TimeEntry {
    std::int64_t id{0};
    std::optional<std::int64_t> mainTopicId;
    std::optional<std::int64_t> subTopicId;
    std::string startedAt;
    std::optional<std::string> endedAt;
    bool running{false};
    std::string notes;
};

/**
 * @brief Fields used to insert an open time entry.
 */
struct TimeEntryDraft {
    TimeTarget target;
    std::string startedAt;
    std::string notes;
};

/**
 * @brief Position of the last row already shown in a newest-first history page.
 */
struct RecentCursor {
    std::string startedAt;
    std::int64_t id{0};
};

/**
 * @brief One stored interval plus the names needed to add it up.
 *
 * @c subTopicId is empty when the time was logged on the main topic itself.
 * @c endedAt is empty while the entry is still running.
 */
struct TrackedInterval {
    std::int64_t categoryId{0};
    std::string categoryName;
    std::int64_t mainTopicId{0};
    std::string mainTopicName;
    std::optional<std::int64_t> subTopicId;
    std::string subTopicName;
    std::string startedAt;
    std::optional<std::string> endedAt;
};

/**
 * @brief How one statistics table row should be read.
 */
enum class StatsLineKind { MainTotal, Direct, SubTopic };

/**
 * @brief Label of a main-topic total row.
 */
inline constexpr std::string_view kStatsTotalLabel = "Total";

/**
 * @brief Label of time that was logged on the main topic rather than a subtopic.
 */
inline constexpr std::string_view kStatsDirectLabel = "(recorded on the main topic)";

/**
 * @brief One display row in a statistics report.
 *
 * @c percent is the row's share of the filtered total, rounded, from 0 to 100.
 * @c detail is @c kStatsTotalLabel, @c kStatsDirectLabel, or a subtopic name.
 */
struct StatsLine {
    StatsLineKind kind{StatsLineKind::MainTotal};
    std::string categoryName;
    std::string mainTopicName;
    std::string detail;
    std::int64_t seconds{0};
    int percent{0};
};

/**
 * @brief Totals for one statistics query.
 *
 * Hierarchical rows are not a flat partition: a main-topic total includes its
 * direct and subtopic rows.
 */
struct StatsReport {
    std::int64_t totalSeconds{0};
    std::vector<StatsLine> lines;
};

/**
 * @brief Saved appearance choice. @c System follows the operating system.
 */
enum class ThemePreference { System, Light, Dark };

/**
 * @brief The whole catalog, already sorted for display.
 */
struct CatalogSnapshot {
    std::vector<Category> categories;
    std::vector<MainTopic> mainTopics;
    std::vector<SubTopic> subTopics;
};

}  // namespace timetracker
