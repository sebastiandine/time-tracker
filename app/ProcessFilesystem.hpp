#pragma once

#include "timetracker/ports/IFilesystem.hpp"

namespace timetracker {

/**
 * @brief Creates directories with the standard filesystem library.
 */
class ProcessFilesystem final : public IFilesystem {
public:
    [[nodiscard]] Result<void> ensureDirectory(const std::filesystem::path& directory) override;
};

}  // namespace timetracker
