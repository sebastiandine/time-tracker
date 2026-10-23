#include "SqlUtil.hpp"

namespace timetracker::detail {

std::string writeFailureMessage(sqlite3* db, std::string_view uniqueMessage,
                                std::string_view foreignKeyMessage, std::string_view checkMessage) {
    switch (classifyConstraint(db)) {
    case SqliteConstraint::Unique:
        return std::string(uniqueMessage);
    case SqliteConstraint::ForeignKey:
        return std::string(foreignKeyMessage);
    case SqliteConstraint::NotNull:
        return "A required field was empty.";
    case SqliteConstraint::Check:
        return std::string(checkMessage);
    case SqliteConstraint::None:
    case SqliteConstraint::Other:
        return kSaveFailed;
    }
    return kSaveFailed;
}

}  // namespace timetracker::detail
