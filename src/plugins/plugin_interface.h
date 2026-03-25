#pragma once

#include <string>

namespace zibby::plugins {

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string name() const = 0;
    virtual bool onLoad() = 0;
    virtual void onUnload() = 0;
};

} // namespace zibby::plugins
