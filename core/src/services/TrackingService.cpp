#include "timetracker/services/TrackingService.hpp"

#include "text/TextRules.hpp"
#include "timetracker/time/TimeFormat.hpp"

#include <chrono>
#include <utility>

namespace timetracker {
namespace {

[[nodiscard]] std::chrono::system_clock::time_point flooredNow(const IClock& clock) {
    return std::chrono::floor<std::chrono::seconds>(clock.nowUtc());
}

}  // namespace

TrackingService::TrackingService(ICatalogRepository& catalog, ITimeEntryRepository& entries,
                                 IClock& clock)
    : catalog_(catalog), entries_(entries), clock_(clock) {}

Result<void> TrackingService::targetExists(TimeTarget target) {
    if (target.kind == TimeTargetKind::MainTopic) {
        auto found = catalog_.findMainTopic(target.id);
        if (!found) {
            return Result<void>::err(found.error());
        }
        return Result<void>::ok();
    }
    auto found = catalog_.findSubTopic(target.id);
    if (!found) {
        return Result<void>::err(found.error());
    }
    return Result<void>::ok();
}

Result<TimeEntry> TrackingService::start(TimeTarget target, std::string notes) {
    auto validNotes = detail::validatedNotes(notes);
    if (!validNotes) {
        return Result<TimeEntry>::err(validNotes.error());
    }
    auto exists = targetExists(target);
    if (!exists) {
        return Result<TimeEntry>::err(exists.error());
    }
    auto running = entries_.findRunning();
    if (!running) {
        return Result<TimeEntry>::err(running.error());
    }
    if (running.value().has_value()) {
        return Result<TimeEntry>::err("A timer is already running.");
    }
    auto startedAt = formatUtc(flooredNow(clock_));
    if (!startedAt) {
        return Result<TimeEntry>::err(startedAt.error());
    }
    TimeEntryDraft draft;
    draft.target = target;
    draft.startedAt = std::move(startedAt.value());
    draft.notes = std::move(validNotes.value());
    return entries_.insertOpen(draft);
}

Result<TimeEntry> TrackingService::stop(std::string notes) {
    auto validNotes = detail::validatedNotes(notes);
    if (!validNotes) {
        return Result<TimeEntry>::err(validNotes.error());
    }
    auto running = entries_.findRunning();
    if (!running) {
        return Result<TimeEntry>::err(running.error());
    }
    if (!running.value().has_value()) {
        return Result<TimeEntry>::err("Nothing is running.");
    }
    auto endedAt = formatUtc(flooredNow(clock_));
    if (!endedAt) {
        return Result<TimeEntry>::err(endedAt.error());
    }
    auto seconds = secondsBetweenUtc(running.value()->startedAt, endedAt.value());
    if (!seconds) {
        return Result<TimeEntry>::err(seconds.error());
    }
    return entries_.closeEntry(running.value()->id, endedAt.value(), validNotes.value());
}

Result<TimeEntry> TrackingService::updateRunningNote(std::string notes) {
    auto validNotes = detail::validatedNotes(notes);
    if (!validNotes) {
        return Result<TimeEntry>::err(validNotes.error());
    }
    auto running = entries_.findRunning();
    if (!running) {
        return Result<TimeEntry>::err(running.error());
    }
    if (!running.value().has_value()) {
        return Result<TimeEntry>::err("Nothing is running.");
    }
    return entries_.updateNotes(running.value()->id, validNotes.value());
}

Result<TimeEntry> TrackingService::updateClosedNote(std::int64_t id, std::string notes) {
    auto validNotes = detail::validatedNotes(notes);
    if (!validNotes) {
        return Result<TimeEntry>::err(validNotes.error());
    }
    auto entry = entries_.findById(id);
    if (!entry) {
        return Result<TimeEntry>::err(entry.error());
    }
    if (entry.value().running) {
        return Result<TimeEntry>::err("That entry is still running.");
    }
    return entries_.updateNotes(id, validNotes.value());
}

Result<void> TrackingService::deleteEntry(std::int64_t id) {
    auto entry = entries_.findById(id);
    if (!entry) {
        return Result<void>::err(entry.error());
    }
    if (entry.value().running) {
        return Result<void>::err("That entry is still running.");
    }
    return entries_.deleteClosed(id);
}

Result<std::optional<TimeEntry>> TrackingService::current() { return entries_.findRunning(); }

Result<std::int64_t> TrackingService::elapsedSeconds() {
    auto running = entries_.findRunning();
    if (!running) {
        return Result<std::int64_t>::err(running.error());
    }
    if (!running.value().has_value()) {
        return Result<std::int64_t>::err("Nothing is running.");
    }
    auto endedAt = formatUtc(flooredNow(clock_));
    if (!endedAt) {
        return Result<std::int64_t>::err(endedAt.error());
    }
    auto seconds = secondsBetweenUtc(running.value()->startedAt, endedAt.value());
    if (!seconds) {
        if (seconds.error() == "The clock is earlier than the start time.") {
            return Result<std::int64_t>::ok(0);
        }
        return Result<std::int64_t>::err(seconds.error());
    }
    return seconds;
}

Result<std::vector<TimeEntry>> TrackingService::listRecent(
    TimeTarget target, int limit, const std::optional<RecentCursor>& cursor) {
    if (limit < 0) {
        limit = 0;
    }
    return entries_.listRecent(target, limit, cursor);
}

}  // namespace timetracker
