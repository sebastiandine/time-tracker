#include "ProcessEnvironment.hpp"

#include <cstdlib>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
extern char** environ;
#endif

namespace timetracker {
namespace {

#if defined(_WIN32)

std::string narrow(const wchar_t* text) {
    if (text == nullptr || text[0] == L'\0') {
        return {};
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        return {};
    }
    std::string value(static_cast<std::size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, value.data(), bytes, nullptr, nullptr);
    return value;
}

struct EnvironmentBlock {
    wchar_t* block{nullptr};
    ~EnvironmentBlock() {
        if (block != nullptr) {
            FreeEnvironmentStringsW(block);
        }
    }
    EnvironmentBlock() = default;
    EnvironmentBlock(const EnvironmentBlock&) = delete;
    EnvironmentBlock& operator=(const EnvironmentBlock&) = delete;
};

bool appendEnvironmentRow(std::vector<std::pair<std::string, std::string>>& listed,
                          const std::wstring& row) {
    if (row.empty() || row[0] == L'=') {
        return false;
    }
    const auto split = row.find(L'=');
    if (split == std::wstring::npos) {
        return false;
    }
    auto name = narrow(row.substr(0, split).c_str());
    auto value = narrow(row.substr(split + 1).c_str());
    if (name.empty() || value.empty()) {
        return false;
    }
    listed.emplace_back(std::move(name), std::move(value));
    return true;
}

std::vector<std::pair<std::string, std::string>> listEnvironment() {
    std::vector<std::pair<std::string, std::string>> listed;
    EnvironmentBlock owned;
    owned.block = GetEnvironmentStringsW();
    if (owned.block == nullptr) {
        return listed;
    }
    for (const wchar_t* cursor = owned.block; *cursor != L'\0';) {
        const std::wstring row(cursor);
        cursor += row.size() + 1;
        if (!appendEnvironmentRow(listed, row)) {
            continue;
        }
    }
    return listed;
}

#else

std::vector<std::pair<std::string, std::string>> listEnvironment() {
    std::vector<std::pair<std::string, std::string>> listed;
    if (environ == nullptr) {
        return listed;
    }
    for (char** cursor = environ; *cursor != nullptr; ++cursor) {
        const std::string row(*cursor);
        const auto split = row.find('=');
        if (split == std::string::npos || split == 0) {
            continue;
        }
        auto value = row.substr(split + 1);
        if (value.empty()) {
            continue;
        }
        listed.emplace_back(row.substr(0, split), std::move(value));
    }
    return listed;
}

#endif

}  // namespace

std::optional<std::string> ProcessEnvironment::get(std::string_view name) const {
    const std::string key{name};
    const char* value = std::getenv(key.c_str());
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string{value};
}

std::vector<std::pair<std::string, std::string>> ProcessEnvironment::entries() const {
    return listEnvironment();
}

}  // namespace timetracker
