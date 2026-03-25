#include "core/profile.h"

#include "core/database.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace zibby::core {

ProfileService::ProfileService(Database& database)
    : database_(database) {}

UserProfile ProfileService::ensureLocalProfile(const std::string& defaultName) {
    const auto existing = database_.getProfile("local");
    if (existing.has_value()) {
        return *existing;
    }

    UserProfile profile;
    profile.id = "local";
    profile.displayName = isValidDisplayName(defaultName) ? defaultName : "zibby-user";
    profile.status = "active";

    database_.upsertProfile(profile);
    return profile;
}

std::optional<UserProfile> ProfileService::get(const std::string& id) const {
    return database_.getProfile(id);
}

bool ProfileService::save(const UserProfile& profile) const {
    if (!isValidDisplayName(profile.displayName)) {
        return false;
    }

    static const std::regex statusRegex("^(active|hidden|inactive)$");
    if (!std::regex_match(profile.status, statusRegex)) {
        return false;
    }

    return database_.upsertProfile(profile);
}

bool ProfileService::isValidDisplayName(const std::string& name) {
    if (name.empty() || name.size() > 64) {
        return false;
    }

    if (std::isspace(static_cast<unsigned char>(name.front())) != 0 || std::isspace(static_cast<unsigned char>(name.back())) != 0) {
        return false;
    }

    static const std::regex allowed(R"(^[A-Za-z0-9._\-а-яА-ЯёЁ]+(?:[ ]?[A-Za-z0-9._\-а-яА-ЯёЁ]+)*$)");
    return std::regex_match(name, allowed);
}

} // namespace zibby::core
