#pragma once

#include "timetracker/ports/IFilesystem.hpp"

namespace timetracker {

/**
 * @brief Host filesystem adapter for the composition root.
 *
 * Directories and text files use the operating-system filesystem. Not thread-safe.
 */
class ProcessFilesystem final : public IFilesystem {
public:
    [[nodiscard]] Result<void> ensureDirectory(const std::filesystem::path& directory) override;
    [[nodiscard]] bool exists(const std::filesystem::path& path) const override;
    [[nodiscard]] Result<std::string> readText(const std::filesystem::path& path) override;
    [[nodiscard]] Result<void> replaceText(const std::filesystem::path& destination,
                                           std::string_view contents) override;
};

}  // namespace timetracker
