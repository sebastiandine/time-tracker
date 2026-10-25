#pragma once

#include "timetracker/ports/IFilesystem.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace timetracker::test {

/**
 * @brief In-memory files for configuration tests.
 *
 * Keys are @c generic_string(). A scripted replace failure leaves the previous
 * bytes in place and does not create a temp entry.
 */
class MemoryFilesystem final : public IFilesystem {
public:
    std::map<std::string, std::string> files;
    std::vector<std::string> failPrefixes;
    int failReplaceRemaining{0};
    bool failEnsure{false};
    bool createRealDirectories{false};

    [[nodiscard]] Result<void> ensureDirectory(const std::filesystem::path& directory) override {
        if (failEnsure) {
            return Result<void>::err("cannot create directory");
        }
        if (createRealDirectories) {
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            if (error) {
                return Result<void>::err("cannot create directory");
            }
        }
        return Result<void>::ok();
    }

    [[nodiscard]] bool exists(const std::filesystem::path& path) const override {
        return files.contains(path.generic_string());
    }

    [[nodiscard]] Result<std::string> readText(const std::filesystem::path& path) override {
        const auto found = files.find(path.generic_string());
        if (found == files.end()) {
            return Result<std::string>::err("not found: " + path.generic_string());
        }
        return Result<std::string>::ok(found->second);
    }

    [[nodiscard]] Result<void> replaceText(const std::filesystem::path& destination,
                                           std::string_view contents) override {
        const auto key = destination.generic_string();
        if (failReplaceRemaining > 0) {
            --failReplaceRemaining;
            return Result<void>::err("replace failed: " + key);
        }
        for (const auto& prefix : failPrefixes) {
            if (key.starts_with(prefix)) {
                return Result<void>::err("replace failed: " + key);
            }
        }
        files[key] = std::string{contents};
        files.erase(key + ".tmp");
        return Result<void>::ok();
    }
};

}  // namespace timetracker::test
