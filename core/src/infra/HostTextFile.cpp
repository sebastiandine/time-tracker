#include "timetracker/infra/HostTextFile.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace timetracker {
namespace {

namespace fs = std::filesystem;

Result<void> ioErr(const fs::path& path, std::string_view what, std::string_view detail) {
    return Result<void>::err(std::string(what) + ": " + path.generic_string() + ": " +
                             std::string(detail));
}

#if defined(_WIN32)

struct WinHandle {
    HANDLE handle{INVALID_HANDLE_VALUE};
    ~WinHandle() {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
    WinHandle() = default;
    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;
};

Result<void> writeSynced(const fs::path& temp, std::string_view contents) {
    WinHandle file;
    file.handle = CreateFileW(temp.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file.handle == INVALID_HANDLE_VALUE) {
        return ioErr(temp, "open temp failed",
                     std::string("win32 ") + std::to_string(GetLastError()));
    }
    const char* cursor = contents.data();
    std::size_t left = contents.size();
    while (left > 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(left, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(file.handle, cursor, chunk, &written, nullptr) || written == 0) {
            return ioErr(temp, "write temp failed",
                         std::string("win32 ") + std::to_string(GetLastError()));
        }
        cursor += written;
        left -= written;
    }
    if (!FlushFileBuffers(file.handle)) {
        return ioErr(temp, "fsync temp failed",
                     std::string("win32 ") + std::to_string(GetLastError()));
    }
    return Result<void>::ok();
}

Result<void> replaceSynced(const fs::path& temp, const fs::path& destination) {
    if (!MoveFileExW(temp.wstring().c_str(), destination.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return ioErr(destination, "replace failed",
                     std::string("win32 ") + std::to_string(GetLastError()));
    }
    return Result<void>::ok();
}

#else

struct PosixFd {
    int fd{-1};
    ~PosixFd() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
    PosixFd() = default;
    PosixFd(const PosixFd&) = delete;
    PosixFd& operator=(const PosixFd&) = delete;
};

Result<void> writeSynced(const fs::path& temp, std::string_view contents) {
    PosixFd file;
    file.fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (file.fd < 0) {
        return ioErr(temp, "open temp failed", std::generic_category().message(errno));
    }
    const char* cursor = contents.data();
    std::size_t left = contents.size();
    while (left > 0) {
        const ssize_t wrote = ::write(file.fd, cursor, left);
        if (wrote < 0) {
            if (errno == EINTR) {
                continue;
            }
            return ioErr(temp, "write temp failed", std::generic_category().message(errno));
        }
        cursor += wrote;
        left -= static_cast<std::size_t>(wrote);
    }
    while (::fsync(file.fd) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return ioErr(temp, "fsync temp failed", std::generic_category().message(errno));
    }
    return Result<void>::ok();
}

Result<void> replaceSynced(const fs::path& temp, const fs::path& destination) {
    if (::rename(temp.c_str(), destination.c_str()) != 0) {
        return ioErr(destination, "replace failed", std::generic_category().message(errno));
    }
    const fs::path parent =
        destination.parent_path().empty() ? fs::path(".") : destination.parent_path();
    PosixFd directory;
    directory.fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (directory.fd < 0) {
        return ioErr(destination, "directory open failed after replace",
                     std::generic_category().message(errno));
    }
    while (::fsync(directory.fd) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return ioErr(destination, "directory fsync failed after replace",
                     std::generic_category().message(errno));
    }
    return Result<void>::ok();
}

#endif

Result<void> removeTemp(const fs::path& temp) {
    std::error_code error;
    fs::remove(temp, error);
    if (fs::exists(temp)) {
        return ioErr(temp, "remove leftover temp failed",
                     error ? error.message() : std::string("still present"));
    }
    return Result<void>::ok();
}

}  // namespace

bool hostPathExists(const fs::path& path) {
    std::error_code error;
    return fs::exists(path, error);
}

Result<std::string> readHostText(const fs::path& path) {
    if (!hostPathExists(path)) {
        return Result<std::string>::err("not found: " + path.generic_string());
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<std::string>::err("read failed: " + path.generic_string());
    }
    std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad()) {
        return Result<std::string>::err("read failed: " + path.generic_string());
    }
    return Result<std::string>::ok(std::move(data));
}

Result<void> replaceHostText(const fs::path& destination, std::string_view contents) {
    std::error_code error;
    if (!destination.parent_path().empty()) {
        fs::create_directories(destination.parent_path(), error);
        if (error) {
            return ioErr(destination, "create_directories failed", error.message());
        }
    }

    fs::path temp = destination;
    temp.concat(".tmp");
    if (auto removed = removeTemp(temp); !removed) {
        return removed;
    }

    if (auto wrote = writeSynced(temp, contents); !wrote) {
        std::error_code ignore;
        fs::remove(temp, ignore);
        return wrote;
    }
    if (auto replaced = replaceSynced(temp, destination); !replaced) {
        std::error_code ignore;
        fs::remove(temp, ignore);
        return replaced;
    }
    return Result<void>::ok();
}

}  // namespace timetracker
