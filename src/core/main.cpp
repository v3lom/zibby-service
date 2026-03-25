#include "core/config.h"
#include "core/service.h"

#include <boost/program_options.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    namespace po = boost::program_options;

    po::options_description options("Zibby Service options");
    options.add_options()
        ("help,h", "Show help")
        ("daemon,d", "Run as daemon/background service")
        ("cli,c", "Connect CLI to running service")
        ("version,v", "Show version");

    po::variables_map variables;
    try {
        po::store(po::parse_command_line(argc, argv, options), variables);
        po::notify(variables);
    } catch (const std::exception& ex) {
        std::cerr << "Argument error: " << ex.what() << '\n';
        std::cout << options << '\n';
        return 1;
    }

    if (variables.count("help") > 0) {
        std::cout << options << '\n';
        return 0;
    }

    if (variables.count("version") > 0) {
        std::cout << "zibby-service " << ZIBBY_VERSION << '\n';
        return 0;
    }

    zibby::core::ConfigManager configManager;
    const auto config = configManager.loadOrCreate();
    zibby::core::Service service(config);

    if (variables.count("cli") > 0) {
        if (service.pingRunningInstance()) {
            std::cout << "Connected: running instance is available" << '\n';
            return 0;
        }

        std::cout << "Service is not running" << '\n';
        return 2;
    }

    const bool daemonMode = variables.count("daemon") > 0;
    return service.run(daemonMode);
}
