#include "timetracker/services/DataLocationService.hpp"

#include "timetracker/paths/DataPaths.hpp"

namespace timetracker {
namespace {

[[nodiscard]] std::string normalized(const std::filesystem::path& path) {
    return path.lexically_normal().generic_string();
}

}  // namespace

DataLocationService::DataLocationService(ConfigService& config, IFilesystem& files,
                                         IDatabaseSession& session)
    : config_(config), files_(files), session_(session) {}

const std::filesystem::path& DataLocationService::directory() const {
    return config_.dataDirectory();
}

Result<DataLocationChoice> DataLocationService::keepPrevious(const std::filesystem::path& previousFile,
                                                             const std::string& reason) {
    // open() closes the current connection first, so a failed switch has to
    // reopen the previous file or the window is left with no database.
    auto restored = session_.open(previousFile);
    if (!restored) {
        return Result<DataLocationChoice>::err(reason + " " + restored.error());
    }
    return Result<DataLocationChoice>::err(reason);
}

Result<DataLocationChoice> DataLocationService::selectDirectory(
    const std::filesystem::path& directory) {
    if (!directory.is_absolute()) {
        return Result<DataLocationChoice>::err("dataDirectory must be an absolute path");
    }
    const auto chosen = directory.lexically_normal();
    if (normalized(chosen) == normalized(config_.dataDirectory())) {
        return Result<DataLocationChoice>::ok(DataLocationChoice{});
    }

    ConfigLayer winning = ConfigLayer::Default;
    const auto effective = config_.effectiveIfStored(chosen, winning);
    if (normalized(effective) != normalized(chosen)) {
        auto stored = config_.storeDataDirectory(files_, chosen);
        if (!stored) {
            return Result<DataLocationChoice>::err(stored.error());
        }
        DataLocationChoice choice;
        choice.overridden = true;
        choice.notice = std::string("dataDirectory is set by layer ") + configLayerName(winning) +
                        ", so the open database stays at " + config_.dataDirectory().generic_string() +
                        ".";
        return Result<DataLocationChoice>::ok(std::move(choice));
    }

    auto previous = databasePath(config_.dataDirectory());
    if (!previous) {
        return Result<DataLocationChoice>::err(previous.error());
    }
    auto opened = openDatabaseAt(chosen, files_, session_);
    if (!opened) {
        return keepPrevious(previous.value(), opened.error());
    }
    auto stored = config_.storeDataDirectory(files_, chosen);
    if (!stored) {
        return keepPrevious(previous.value(), stored.error());
    }
    DataLocationChoice choice;
    choice.switched = true;
    return Result<DataLocationChoice>::ok(std::move(choice));
}

}  // namespace timetracker
