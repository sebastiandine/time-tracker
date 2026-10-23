#include "ProcessEnvironment.hpp"

#include <cstdlib>

namespace timetracker {

std::optional<std::string> ProcessEnvironment::get(std::string_view name) const {
    const std::string key{name};
    const char* value = std::getenv(key.c_str());
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string{value};
}

}  // namespace timetracker
