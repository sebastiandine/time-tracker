#include "ProcessFilesystem.hpp"

#include <system_error>

namespace timetracker {

Result<void> ProcessFilesystem::ensureDirectory(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return Result<void>::err("Could not create the data directory: " + directory.generic_string());
    }
    return Result<void>::ok();
}

}  // namespace timetracker
