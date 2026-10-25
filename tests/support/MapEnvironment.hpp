#pragma once

#include "timetracker/ports/IEnvironment.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace timetracker::test {

/**
 * @brief Environment map for tests.
 *
 * Blank values are missing, matching @c ProcessEnvironment.
 */
class MapEnvironment final : public IEnvironment {
public:
    std::map<std::string, std::string> values;

    [[nodiscard]] std::optional<std::string> get(std::string_view name) const override {
        const auto found = values.find(std::string{name});
        if (found == values.end() || found->second.empty()) {
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> entries() const override {
        std::vector<std::pair<std::string, std::string>> listed;
        for (const auto& [key, value] : values) {
            if (!value.empty()) {
                listed.emplace_back(key, value);
            }
        }
        return listed;
    }
};

}  // namespace timetracker::test
