#pragma once

#include <chrono>

namespace timetracker {

/**
 * @brief Source of UTC instants.
 *
 * Not thread-safe. The desktop app calls it on the UI thread. Tests supply a
 * fake so a shutdown gap is just a jump of this clock.
 */
class IClock {
public:
    virtual ~IClock() = default;

    /**
     * @brief Current UTC instant.
     * @return A system-clock time point interpreted as UTC.
     */
    [[nodiscard]] virtual std::chrono::system_clock::time_point nowUtc() const = 0;
};

}  // namespace timetracker
