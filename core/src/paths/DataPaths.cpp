#include "timetracker/paths/DataPaths.hpp"

#include <string>
#include <system_error>

namespace timetracker {
namespace {

[[nodiscard]] bool prefixMatch(const std::filesystem::path& root,
                               const std::filesystem::path& candidate) {
    const auto base = root.generic_string();
    const auto joined = candidate.generic_string();
    if (joined == base) {
        return true;
    }
    const auto prefix = !base.empty() && base.back() == '/' ? base : base + '/';
    return joined.size() > prefix.size() && joined.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

bool isConfinedPath(const std::filesystem::path& root, const std::filesystem::path& relative) {
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    for (const auto& part : relative) {
        const auto text = part.generic_string();
        if (text.empty() || text == "." || text == "..") {
            return false;
        }
        if (text.find_first_of("/\\") != std::string::npos) {
            return false;
        }
    }
    const auto base = root.lexically_normal();
    const auto joined = (base / relative).lexically_normal();
    std::error_code error;
    const auto canonBase = std::filesystem::weakly_canonical(base, error);
    if (error) {
        return false;
    }
    const auto canonJoined = std::filesystem::weakly_canonical(joined, error);
    if (error) {
        return false;
    }
    return prefixMatch(canonBase, canonJoined);
}

Result<std::filesystem::path> userDataDirectory(const IEnvironment& environment) {
#if defined(_WIN32)
    auto base = environment.get("LOCALAPPDATA");
    if (!base.has_value() || base->empty()) {
        return Result<std::filesystem::path>::err("LOCALAPPDATA is not set");
    }
    const std::filesystem::path root{*base};
#else
    std::filesystem::path root;
    auto xdg = environment.get("XDG_DATA_HOME");
    if (xdg.has_value() && !xdg->empty()) {
        root = *xdg;
    } else {
        auto home = environment.get("HOME");
        if (!home.has_value() || home->empty()) {
            return Result<std::filesystem::path>::err("HOME is not set");
        }
        root = std::filesystem::path{*home} / ".local" / "share";
    }
#endif
    const std::filesystem::path relative{std::string{kDataDirectoryName}};
    if (!isConfinedPath(root, relative)) {
        return Result<std::filesystem::path>::err("The data directory escapes its root.");
    }
    return Result<std::filesystem::path>::ok(root / relative);
}

Result<std::filesystem::path> databasePath(const std::filesystem::path& dataDirectory) {
    const std::filesystem::path relative{std::string{kDatabaseFileName}};
    if (!isConfinedPath(dataDirectory, relative)) {
        return Result<std::filesystem::path>::err("The data directory escapes its root.");
    }
    return Result<std::filesystem::path>::ok(dataDirectory / relative);
}

Result<void> openUserDatabase(const IEnvironment& environment, IFilesystem& files,
                              IDatabaseSession& session) {
    auto directory = userDataDirectory(environment);
    if (!directory) {
        return Result<void>::err(directory.error());
    }
    auto ensured = files.ensureDirectory(directory.value());
    if (!ensured) {
        return ensured;
    }
    auto file = databasePath(directory.value());
    if (!file) {
        return Result<void>::err(file.error());
    }
    return session.open(file.value());
}

Result<void> openDatabaseAt(const std::filesystem::path& directory, IFilesystem& files,
                            IDatabaseSession& session) {
    auto ensured = files.ensureDirectory(directory);
    if (!ensured) {
        return ensured;
    }
    auto file = databasePath(directory);
    if (!file) {
        return Result<void>::err(file.error());
    }
    return session.open(file.value());
}

}  // namespace timetracker
