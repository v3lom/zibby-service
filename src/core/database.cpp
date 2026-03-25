#include "core/database.h"

namespace zibby::core {

Database::~Database() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::initialize(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS messages(" 
        "id INTEGER PRIMARY KEY AUTOINCREMENT," 
        "peer TEXT NOT NULL," 
        "payload TEXT NOT NULL," 
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP" 
        ");";

    char* errorMessage = nullptr;
    const auto rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        sqlite3_free(errorMessage);
        return false;
    }

    return true;
}

} // namespace zibby::core
