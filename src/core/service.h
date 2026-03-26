#pragma once

#include "core/config.h"

#include <boost/asio/io_context.hpp>

#include <mutex>
#include <string>

namespace zibby::core {

class Service {
public:
    explicit Service(Config config);

    int run(bool daemonMode);
    void requestStop();
    bool pingRunningInstance() const;
    std::string apiToken() const;
    std::string apiEndpoint() const;

private:
    Config config_;
    mutable std::mutex runMutex_;
    boost::asio::io_context* runningIo_ = nullptr;
};

} // namespace zibby::core
