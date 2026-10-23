#pragma once

#include "timetracker/ports/IEnvironment.hpp"

namespace timetracker {

/**
 * @brief Environment variables from the process.
 *
 * A blank value is treated as missing. Call from the UI thread at startup.
 */
class ProcessEnvironment final : public IEnvironment {
public:
    [[nodiscard]] std::optional<std::string> get(std::string_view name) const override;
};

}  // namespace timetracker
