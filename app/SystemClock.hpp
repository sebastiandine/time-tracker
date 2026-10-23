#pragma once

#include "timetracker/ports/IClock.hpp"

namespace timetracker {

/**
 * @brief UTC clock backed by the operating system.
 */
class SystemClock final : public IClock {
public:
    [[nodiscard]] std::chrono::system_clock::time_point nowUtc() const override;
};

}  // namespace timetracker
