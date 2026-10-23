#include "SystemClock.hpp"

namespace timetracker {

std::chrono::system_clock::time_point SystemClock::nowUtc() const {
    return std::chrono::system_clock::now();
}

}  // namespace timetracker
