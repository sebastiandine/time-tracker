#pragma once

#include "timetracker/util/Result.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace timetracker::detail {

/**
 * @brief Maximum display-name length in Unicode code points.
 */
inline constexpr std::size_t kMaxNameLength = 200;

/**
 * @brief Maximum note length in Unicode code points.
 */
inline constexpr std::size_t kMaxNotesLength = 20000;

/**
 * @brief Trim ends and reject an empty or oversized name.
 * @param name Raw name from the user.
 * @return The trimmed name, or a validation error.
 */
[[nodiscard]] Result<std::string> validatedName(std::string_view name);

/**
 * @brief Trim ends and reject an oversized note. An empty note is allowed.
 * @param notes Raw note from the user.
 * @return The trimmed note, or a validation error.
 */
[[nodiscard]] Result<std::string> validatedNotes(std::string_view notes);

/**
 * @brief A name and note that have both passed validation.
 */
struct NameAndNotes {
    std::string name;
    std::string notes;
};

/**
 * @brief Validate a name and a note together.
 * @param name Raw name.
 * @param notes Raw note.
 * @return Both trimmed values, or the first validation error.
 */
[[nodiscard]] Result<NameAndNotes> validatedNameAndNotes(std::string_view name,
                                                         std::string_view notes);

}  // namespace timetracker::detail
