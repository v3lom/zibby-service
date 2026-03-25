#include "core/database.h"
#include "core/peer.h"
#include "core/profile.h"

#include <boost/filesystem.hpp>

#include <cstdlib>
#include <iostream>

int main() {
    namespace fs = boost::filesystem;
    const fs::path baseDir = fs::temp_directory_path() / fs::unique_path("zibby-profile-peer-test-%%%%-%%%%");
    fs::create_directories(baseDir);
    const fs::path dbPath = baseDir / "profile.sqlite3";

    {
        zibby::core::Database database;
        if (!database.initialize(dbPath.string())) {
            std::cerr << "database.initialize failed" << std::endl;
            return EXIT_FAILURE;
        }

        zibby::core::ProfileService profileService(database);
        auto profile = profileService.ensureLocalProfile("tester");
        profile.displayName = "tester_01";
        profile.status = "hidden";
        profile.bio = "bio";
        if (!profileService.save(profile)) {
            std::cerr << "profile save failed" << std::endl;
            return EXIT_FAILURE;
        }

        const auto loaded = profileService.get();
        if (!loaded.has_value() || loaded->displayName != "tester_01" || loaded->status != "hidden") {
            std::cerr << "profile read mismatch" << std::endl;
            return EXIT_FAILURE;
        }

        zibby::core::PeerInfo peer;
        peer.peerId = "127.0.0.1:9876";
        peer.displayName = "local-peer";
        peer.host = "127.0.0.1";
        peer.port = 9876;
        peer.version = "0.1.0";

        if (!database.upsertPeer(peer)) {
            std::cerr << "peer upsert failed" << std::endl;
            return EXIT_FAILURE;
        }

        const auto peers = database.listPeers(10);
        if (peers.empty() || peers.front().peerId != peer.peerId) {
            std::cerr << "peer list mismatch" << std::endl;
            return EXIT_FAILURE;
        }
    }

    fs::remove_all(baseDir);
    return EXIT_SUCCESS;
}
