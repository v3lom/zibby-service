#include "plugins/plugin_interface.h"

namespace {

class SamplePlugin final : public zibby::plugins::IPlugin {
public:
    std::string name() const override {
        return "sample";
    }

    bool onLoad() override {
        return true;
    }

    void onUnload() override {
    }
};

} // namespace
