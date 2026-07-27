// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

// Stand-in child for the EngineProcess tests.
//
// FAKE_ENGINE_REPORT         file to write the token and argv report into
// FAKE_ENGINE_MODE           "exit" (default), "run", or "stubborn"
// FAKE_ENGINE_SHUTDOWN_FILE  in run modes, exit 0 once this file appears
// FAKE_ENGINE_READY_FILE     created once signal handling is in place, so
//                            a test never signals a child that is not yet
//                            ignoring SIGTERM

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#ifndef _WIN32
#include <csignal>
#endif

int main(int argc, char* argv[])
{
    const char* report_path = std::getenv("FAKE_ENGINE_REPORT");
    if (report_path != nullptr) {
        std::ofstream report(report_path, std::ios::trunc);
        const char* token = std::getenv("DOSBOX_API_TOKEN");
        report << "token=" << (token != nullptr ? token : "<unset>") << "\n";
        for (int i = 1; i < argc; ++i) {
            report << "arg=" << argv[i] << "\n";
        }
    }

    const char* mode_env = std::getenv("FAKE_ENGINE_MODE");
    const std::string mode = mode_env != nullptr ? mode_env : "exit";
    if (mode == "exit") {
        // Distinctive code so a forwarded exit status is provably the child's.
        return 7;
    }

#ifndef _WIN32
    if (mode == "stubborn") {
        std::signal(SIGTERM, SIG_IGN);
    }
#endif

    const char* ready_path = std::getenv("FAKE_ENGINE_READY_FILE");
    if (ready_path != nullptr) {
        std::ofstream(ready_path) << "ready";
    }

    const char* shutdown_path = std::getenv("FAKE_ENGINE_SHUTDOWN_FILE");
    for (;;) {
        if (shutdown_path != nullptr && std::filesystem::exists(shutdown_path)) {
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
