#pragma once

#include "core/message.h"

#include <sqlite3.h>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace zibby::core {

class Database {
public:
    Database() = default;
    ~Database();

    bool initialize(const std::string& dbPath);
    bool insertMessage(const Message& message, std::int64_t* messageId = nullptr);
    bool updateMessagePayload(std::int64_t messageId, const std::string& encryptedPayload);
    bool markMessageRead(std::int64_t messageId);
    std::vector<Message> getMessages(const std::string& chatId, std::size_t limit) const;

private:
    bool executeSql(const char* sql) const;

    sqlite3* db_ = nullptr;
    mutable std::mutex dbMutex_;
};

} // namespace zibby::core
