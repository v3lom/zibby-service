#include "core/database.h"

#include "core/message.h"

#include <algorithm>

namespace zibby::core {

namespace {

bool tableExists(sqlite3* db, const char* tableName) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1;",
            -1,
            &stmt,
            nullptr)
        != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, tableName, -1, SQLITE_TRANSIENT);
    const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

bool hasColumn(sqlite3* db, const char* tableName, const char* columnName) {
    sqlite3_stmt* stmt = nullptr;
    const std::string sql = std::string("PRAGMA table_info(") + tableName + ");";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* rawName = sqlite3_column_text(stmt, 1);
        if (rawName != nullptr && std::string(columnName) == std::string(reinterpret_cast<const char*>(rawName))) {
            found = true;
            break;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

} // namespace

Database::~Database() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        return false;
    }

    const char* chatsSql =
        "CREATE TABLE IF NOT EXISTS chats("
        "id TEXT PRIMARY KEY,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    const char* messagesSql =
        "CREATE TABLE IF NOT EXISTS messages("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "chat_id TEXT NOT NULL,"
        "sender TEXT NOT NULL,"
        "recipient TEXT NOT NULL,"
        "payload TEXT NOT NULL,"
        "is_read INTEGER NOT NULL DEFAULT 0,"
        "is_edited INTEGER NOT NULL DEFAULT 0,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "edited_at DATETIME,"
        "read_at DATETIME,"
        "FOREIGN KEY(chat_id) REFERENCES chats(id)"
        ");";

    const char* indexSql =
        "CREATE INDEX IF NOT EXISTS idx_messages_chat_created ON messages(chat_id, created_at);";

    const char* profilesSql =
        "CREATE TABLE IF NOT EXISTS profiles("
        "id TEXT PRIMARY KEY,"
        "display_name TEXT NOT NULL,"
        "avatar_path TEXT DEFAULT '',"
        "bio TEXT DEFAULT '',"
        "birth_date TEXT DEFAULT '',"
        "status TEXT NOT NULL DEFAULT 'active',"
        "public_key TEXT DEFAULT '',"
        "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    const char* peersSql =
        "CREATE TABLE IF NOT EXISTS peers("
        "peer_id TEXT PRIMARY KEY,"
        "display_name TEXT NOT NULL,"
        "host TEXT NOT NULL,"
        "port INTEGER NOT NULL,"
        "version TEXT DEFAULT '',"
        "last_seen DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    if (!executeSql(chatsSql)) {
        return false;
    }

    if (tableExists(db_, "messages") && !hasColumn(db_, "messages", "chat_id")) {
        if (!executeSql("ALTER TABLE messages RENAME TO messages_legacy;")) {
            return false;
        }

        if (!executeSql(messagesSql)) {
            return false;
        }

        if (!executeSql("INSERT OR IGNORE INTO chats(id) VALUES('legacy');")) {
            return false;
        }

        if (!executeSql(
                "INSERT INTO messages(chat_id, sender, recipient, payload, created_at) "
                "SELECT 'legacy', COALESCE(peer, ''), COALESCE(peer, ''), payload, COALESCE(created_at, CURRENT_TIMESTAMP) "
                "FROM messages_legacy;")) {
            return false;
        }

        if (!executeSql("DROP TABLE messages_legacy;")) {
            return false;
        }
    }

    return executeSql(messagesSql) && executeSql(indexSql) && executeSql(profilesSql) && executeSql(peersSql);
}

bool Database::upsertProfile(const UserProfile& profile) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_ == nullptr || profile.id.empty() || profile.displayName.empty()) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO profiles(id, display_name, avatar_path, bio, birth_date, status, public_key, updated_at) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, CURRENT_TIMESTAMP) "
        "ON CONFLICT(id) DO UPDATE SET "
        "display_name=excluded.display_name, avatar_path=excluded.avatar_path, bio=excluded.bio, "
        "birth_date=excluded.birth_date, status=excluded.status, public_key=excluded.public_key, "
        "updated_at=CURRENT_TIMESTAMP;";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, profile.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, profile.displayName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, profile.avatarPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, profile.bio.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, profile.birthDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, profile.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, profile.publicKey.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::optional<UserProfile> Database::getProfile(const std::string& id) const {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_ == nullptr || id.empty()) {
        return std::nullopt;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, display_name, avatar_path, bio, birth_date, status, public_key "
        "FROM profiles WHERE id = ?1 LIMIT 1;";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    UserProfile profile;
    profile.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    profile.displayName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    profile.avatarPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    profile.bio = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    profile.birthDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    profile.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    profile.publicKey = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    sqlite3_finalize(stmt);

    return profile;
}

bool Database::upsertPeer(const PeerInfo& peer) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_ == nullptr || peer.peerId.empty() || peer.host.empty() || peer.port <= 0) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO peers(peer_id, display_name, host, port, version, last_seen) "
        "VALUES(?1, ?2, ?3, ?4, ?5, CURRENT_TIMESTAMP) "
        "ON CONFLICT(peer_id) DO UPDATE SET "
        "display_name=excluded.display_name, host=excluded.host, port=excluded.port, "
        "version=excluded.version, last_seen=CURRENT_TIMESTAMP;";

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, peer.peerId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, peer.displayName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, peer.host.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, peer.port);
    sqlite3_bind_text(stmt, 5, peer.version.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<PeerInfo> Database::listPeers(std::size_t limit) const {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<PeerInfo> peers;
    if (db_ == nullptr) {
        return peers;
    }

    const int queryLimit = static_cast<int>(std::min<std::size_t>(limit == 0 ? 200 : limit, 2000));
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT peer_id, display_name, host, port, version, COALESCE(last_seen, '') "
        "FROM peers ORDER BY last_seen DESC LIMIT ?1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return peers;
    }

    sqlite3_bind_int(stmt, 1, queryLimit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PeerInfo peer;
        peer.peerId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        peer.displayName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        peer.host = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        peer.port = sqlite3_column_int(stmt, 3);
        peer.version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        peer.lastSeen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        peers.push_back(std::move(peer));
    }

    sqlite3_finalize(stmt);
    return peers;
}

bool Database::insertMessage(const Message& message, std::int64_t* messageId) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_ == nullptr || message.chatId.empty()) {
        return false;
    }

    sqlite3_stmt* chatStmt = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO chats(id) VALUES(?1);", -1, &chatStmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(chatStmt, 1, message.chatId.c_str(), -1, SQLITE_TRANSIENT);
    const int chatStep = sqlite3_step(chatStmt);
    sqlite3_finalize(chatStmt);
    if (chatStep != SQLITE_DONE) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "INSERT INTO messages(chat_id, sender, recipient, payload) VALUES(?1, ?2, ?3, ?4);",
            -1,
            &stmt,
            nullptr)
        != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, message.chatId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, message.from.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, message.to.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, message.payload.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return false;
    }

    if (messageId != nullptr) {
        *messageId = static_cast<std::int64_t>(sqlite3_last_insert_rowid(db_));
    }
    return true;
}

bool Database::updateMessagePayload(std::int64_t messageId, const std::string& encryptedPayload) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_ == nullptr || messageId <= 0 || encryptedPayload.empty()) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "UPDATE messages SET payload = ?1, is_edited = 1, edited_at = CURRENT_TIMESTAMP WHERE id = ?2;",
            -1,
            &stmt,
            nullptr)
        != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, encryptedPayload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(messageId));

    const int rc = sqlite3_step(stmt);
    const int changes = sqlite3_changes(db_);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE && changes > 0;
}

bool Database::markMessageRead(std::int64_t messageId) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_ == nullptr || messageId <= 0) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "UPDATE messages SET is_read = 1, read_at = CURRENT_TIMESTAMP WHERE id = ?1;",
            -1,
            &stmt,
            nullptr)
        != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(messageId));
    const int rc = sqlite3_step(stmt);
    const int changes = sqlite3_changes(db_);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE && changes > 0;
}

std::vector<Message> Database::getMessages(const std::string& chatId, std::size_t limit) const {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<Message> messages;
    if (db_ == nullptr || chatId.empty()) {
        return messages;
    }

    const int queryLimit = static_cast<int>(std::min<std::size_t>(limit == 0 ? 100 : limit, 1000));

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "SELECT id, chat_id, sender, recipient, payload, is_read, is_edited, "
            "COALESCE(created_at, ''), COALESCE(edited_at, ''), COALESCE(read_at, '') "
            "FROM messages WHERE chat_id = ?1 ORDER BY id ASC LIMIT ?2;",
            -1,
            &stmt,
            nullptr)
        != SQLITE_OK) {
        return messages;
    }

    sqlite3_bind_text(stmt, 1, chatId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, queryLimit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Message message;
        message.id = static_cast<std::int64_t>(sqlite3_column_int64(stmt, 0));
        message.chatId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        message.from = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        message.to = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        message.payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        message.read = sqlite3_column_int(stmt, 5) != 0;
        message.edited = sqlite3_column_int(stmt, 6) != 0;
        message.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        message.editedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        message.readAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        messages.push_back(std::move(message));
    }

    sqlite3_finalize(stmt);
    return messages;
}

bool Database::executeSql(const char* sql) const {
    char* errorMessage = nullptr;
    const auto rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        sqlite3_free(errorMessage);
        return false;
    }
    return true;
}

} // namespace zibby::core
