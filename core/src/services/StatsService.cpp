#include "timetracker/services/StatsService.hpp"

#include "timetracker/domain/Records.hpp"
#include "timetracker/time/TimeFormat.hpp"

#include <algorithm>
#include <chrono>
#include <string_view>
#include <utility>
#include <vector>

namespace timetracker {
namespace {

struct SubAcc {
    std::int64_t id{0};
    std::string name;
    std::int64_t seconds{0};
};

struct MainAcc {
    std::int64_t categoryId{0};
    std::string categoryName;
    std::int64_t id{0};
    std::string name;
    std::int64_t direct{0};
    std::vector<SubAcc> subs;
};

[[nodiscard]] unsigned char lowerAscii(unsigned char character) {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<unsigned char>(character - 'A' + 'a');
    }
    return character;
}

[[nodiscard]] int asciiCompare(std::string_view left, std::string_view right) {
    const auto count = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < count; ++index) {
        const auto leftChar = lowerAscii(static_cast<unsigned char>(left[index]));
        const auto rightChar = lowerAscii(static_cast<unsigned char>(right[index]));
        if (leftChar < rightChar) {
            return -1;
        }
        if (rightChar < leftChar) {
            return 1;
        }
    }
    if (left.size() < right.size()) {
        return -1;
    }
    if (right.size() < left.size()) {
        return 1;
    }
    return 0;
}

[[nodiscard]] int percentOf(std::int64_t part, std::int64_t total) {
    if (total <= 0 || part <= 0) {
        return 0;
    }
    return static_cast<int>((part * 100 + total / 2) / total);
}

/**
 * @brief Seconds of one entry that fall inside the report window.
 *
 * An open entry has no end, so @p now is used. Both ends are then clipped to
 * the window so a timer that started earlier does not fill the whole report.
 */
[[nodiscard]] Result<std::int64_t> clippedSeconds(const TrackedInterval& row, const UtcRange& range,
                                                  std::chrono::system_clock::time_point now) {
    auto started = parseUtc(row.startedAt);
    if (!started) {
        return Result<std::int64_t>::err(started.error());
    }
    auto rangeStart = parseUtc(range.startUtc);
    if (!rangeStart) {
        return Result<std::int64_t>::err(rangeStart.error());
    }
    auto rangeEnd = parseUtc(range.endUtc);
    if (!rangeEnd) {
        return Result<std::int64_t>::err(rangeEnd.error());
    }
    auto ended = std::chrono::floor<std::chrono::seconds>(now);
    if (row.endedAt.has_value()) {
        auto parsedEnd = parseUtc(*row.endedAt);
        if (!parsedEnd) {
            return Result<std::int64_t>::err(parsedEnd.error());
        }
        ended = std::chrono::floor<std::chrono::seconds>(parsedEnd.value());
    }
    const auto clipStart = std::max(std::chrono::floor<std::chrono::seconds>(started.value()),
                                    std::chrono::floor<std::chrono::seconds>(rangeStart.value()));
    const auto clipEnd = std::min(ended, std::chrono::floor<std::chrono::seconds>(rangeEnd.value()));
    if (clipEnd <= clipStart) {
        return Result<std::int64_t>::ok(0);
    }
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clipEnd - clipStart).count();
    return Result<std::int64_t>::ok(seconds);
}

[[nodiscard]] std::size_t indexOfMain(std::vector<MainAcc>& mains, const TrackedInterval& row) {
    for (std::size_t index = 0; index < mains.size(); ++index) {
        if (mains[index].id == row.mainTopicId) {
            return index;
        }
    }
    MainAcc created;
    created.categoryId = row.categoryId;
    created.categoryName = row.categoryName;
    created.id = row.mainTopicId;
    created.name = row.mainTopicName;
    mains.push_back(std::move(created));
    return mains.size() - 1;
}

void addSeconds(MainAcc& main, const TrackedInterval& row, std::int64_t seconds) {
    if (!row.subTopicId.has_value()) {
        main.direct += seconds;
        return;
    }
    for (auto& sub : main.subs) {
        if (sub.id == *row.subTopicId) {
            sub.seconds += seconds;
            return;
        }
    }
    SubAcc created;
    created.id = *row.subTopicId;
    created.name = row.subTopicName;
    created.seconds = seconds;
    main.subs.push_back(std::move(created));
}

[[nodiscard]] bool mainBefore(const MainAcc& left, const MainAcc& right) {
    const int byCategory = asciiCompare(left.categoryName, right.categoryName);
    if (byCategory != 0) {
        return byCategory < 0;
    }
    if (left.categoryId != right.categoryId) {
        return left.categoryId < right.categoryId;
    }
    const int byName = asciiCompare(left.name, right.name);
    if (byName != 0) {
        return byName < 0;
    }
    return left.id < right.id;
}

[[nodiscard]] bool subBefore(const SubAcc& left, const SubAcc& right) {
    const int byName = asciiCompare(left.name, right.name);
    if (byName != 0) {
        return byName < 0;
    }
    return left.id < right.id;
}

[[nodiscard]] std::int64_t mainTotal(const MainAcc& main) {
    std::int64_t total = main.direct;
    for (const auto& sub : main.subs) {
        total += sub.seconds;
    }
    return total;
}

[[nodiscard]] StatsLine makeLine(StatsLineKind kind, const MainAcc& main, std::string detail,
                                 std::int64_t seconds, std::int64_t grand) {
    StatsLine line;
    line.kind = kind;
    line.categoryName = main.categoryName;
    line.mainTopicName = main.name;
    line.detail = std::move(detail);
    line.seconds = seconds;
    line.percent = percentOf(seconds, grand);
    return line;
}

void appendLines(StatsReport& report, MainAcc& main, std::int64_t grand) {
    const auto total = mainTotal(main);
    if (total <= 0) {
        return;
    }
    report.lines.push_back(
        makeLine(StatsLineKind::MainTotal, main, std::string(kStatsTotalLabel), total, grand));
    if (main.direct > 0) {
        report.lines.push_back(makeLine(StatsLineKind::Direct, main, std::string(kStatsDirectLabel),
                                         main.direct, grand));
    }
    std::sort(main.subs.begin(), main.subs.end(), subBefore);
    for (const auto& sub : main.subs) {
        if (sub.seconds <= 0) {
            continue;
        }
        report.lines.push_back(
            makeLine(StatsLineKind::SubTopic, main, sub.name, sub.seconds, grand));
    }
}

[[nodiscard]] Result<StatsReport> buildReport(const std::vector<TrackedInterval>& rows,
                                              std::chrono::system_clock::time_point now,
                                              const UtcRange& range) {
    std::vector<MainAcc> mains;
    for (const auto& row : rows) {
        auto seconds = clippedSeconds(row, range, now);
        if (!seconds) {
            return Result<StatsReport>::err(seconds.error());
        }
        if (seconds.value() <= 0) {
            continue;
        }
        const auto index = indexOfMain(mains, row);
        addSeconds(mains[index], row, seconds.value());
    }
    std::sort(mains.begin(), mains.end(), mainBefore);
    std::int64_t grand = 0;
    for (const auto& main : mains) {
        grand += mainTotal(main);
    }
    StatsReport report;
    report.totalSeconds = grand;
    for (auto& main : mains) {
        appendLines(report, main, grand);
    }
    return Result<StatsReport>::ok(std::move(report));
}

}  // namespace

StatsService::StatsService(ITimeEntryRepository& entries, IClock& clock)
    : entries_(entries), clock_(clock) {}

Result<StatsReport> StatsService::summarize(const UtcRange& range,
                                            std::optional<std::int64_t> categoryId) {
    auto rows = entries_.listOverlapping(range.startUtc, range.endUtc, categoryId);
    if (!rows) {
        return Result<StatsReport>::err(rows.error());
    }
    const auto now = std::chrono::floor<std::chrono::seconds>(clock_.nowUtc());
    return buildReport(rows.value(), now, range);
}

}  // namespace timetracker
