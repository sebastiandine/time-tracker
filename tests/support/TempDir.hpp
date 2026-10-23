#pragma once

#include <atomic>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace timetracker::test {

/**
 * @brief A unique temporary directory removed when the object dies.
 */
struct TempDir {
    std::filesystem::path path;

    TempDir() {
        static std::atomic<int> sequence{0};
#if defined(_WIN32)
        const auto process = static_cast<unsigned long>(GetCurrentProcessId());
#else
        const auto process = static_cast<unsigned long>(::getpid());
#endif
        path = std::filesystem::temp_directory_path() /
               ("time-tracker-" + std::to_string(process) + "-" +
                std::to_string(sequence.fetch_add(1)));
        std::filesystem::create_directories(path);
    }

    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

}  // namespace timetracker::test
