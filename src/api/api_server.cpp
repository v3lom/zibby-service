#include "api/api_server.h"

namespace zibby::api {

bool ApiServer::start(const std::string&) {
    running_ = true;
    return true;
}

void ApiServer::stop() {
    running_ = false;
}

} // namespace zibby::api
