#pragma once

#include <string>

namespace zibby::core {

struct Message {
    std::string from;
    std::string to;
    std::string payload;
};

} // namespace zibby::core
