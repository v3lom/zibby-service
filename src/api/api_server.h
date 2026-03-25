#pragma once

#include <string>

namespace zibby::api {

class ApiServer {
public:
    bool start(const std::string& endpoint);
    void stop();

private:
    bool running_ = false;
};

} // namespace zibby::api
