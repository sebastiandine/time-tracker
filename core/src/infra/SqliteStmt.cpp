#include "timetracker/infra/SqliteStmt.hpp"

#include <sqlite3.h>

namespace timetracker {

void SqliteStmtDeleter::operator()(sqlite3_stmt* statement) const noexcept {
    if (statement != nullptr) {
        sqlite3_finalize(statement);
    }
}

SqliteStmt prepareStmt(sqlite3* db, const char* sql, int& rc) {
    sqlite3_stmt* raw = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(raw);
        return {};
    }
    return SqliteStmt{raw};
}

SqliteConstraint classifyConstraint(sqlite3* db) noexcept {
    switch (sqlite3_extended_errcode(db)) {
    case SQLITE_CONSTRAINT_UNIQUE:
    case SQLITE_CONSTRAINT_PRIMARYKEY:
        return SqliteConstraint::Unique;
    case SQLITE_CONSTRAINT_FOREIGNKEY:
        return SqliteConstraint::ForeignKey;
    case SQLITE_CONSTRAINT_NOTNULL:
        return SqliteConstraint::NotNull;
    case SQLITE_CONSTRAINT_CHECK:
        return SqliteConstraint::Check;
    default:
        return SqliteConstraint::Other;
    }
}

}  // namespace timetracker
