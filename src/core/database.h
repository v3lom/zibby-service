#pragma once

#include <sqlite3.h>
#include <string>

namespace zibby::core {

class Database {
public:
    Database() = default;
    ~Database();

    bool initialize(const std::string& dbPath);

private:
    sqlite3* db_ = nullptr;
};

} // namespace zibby::core
