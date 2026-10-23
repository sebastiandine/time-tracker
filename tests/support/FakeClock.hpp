#pragma once

#include "timetracker/ports/IClock.hpp"

#include <chrono>

namespace timetracker::test {

/**
 * @brief Clock whose instant changes only when a test moves it.
 */
class FakeClock final : public IClock {
public:
    /**
     * @brief Start the clock at @p start.
     * @param start Initial UTC instant.
     */
    explicit FakeClock(std::chrono::system_clock::time_point start) : now_(start) {}

    [[nodiscard]] std::chrono::system_clock::time_point nowUtc() const override { return now_; }

    /**
     * @brief Move the clock to an absolute instant.
     * @param instant New UTC instant.
     */
    void set(std::chrono::system_clock::time_point instant) { now_ = instant; }

private:
    std::chrono::system_clock::time_point now_;
};

}  // namespace timetracker::test
