#include "ProcessFilesystem.hpp"

#include "timetracker/infra/HostTextFile.hpp"

#include <system_error>

namespace timetracker {

Result<void> ProcessFilesystem::ensureDirectory(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return Result<void>::err("Could not create the data directory: " +
                                 directory.generic_string());
    }
    return Result<void>::ok();
}

bool ProcessFilesystem::exists(const std::filesystem::path& path) const {
    return hostPathExists(path);
}

Result<std::string> ProcessFilesystem::readText(const std::filesystem::path& path) {
    return readHostText(path);
}

Result<void> ProcessFilesystem::replaceText(const std::filesystem::path& destination,
                                            std::string_view contents) {
    return replaceHostText(destination, contents);
}

}  // namespace timetracker
