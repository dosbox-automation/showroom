// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_NET_FILE_TIME_H
#define SHOWROOM_NET_FILE_TIME_H

#include <chrono>
#include <filesystem>

namespace showroom {

// Convert a wall-clock instant to a filesystem timestamp for
// last_write_time, without std::chrono::clock_cast<file_clock>. On MSVC
// that cast routes through utc_clock and loads the timezone database,
// which throws on Windows hosts lacking the ICU backing library
// (aug-qerw). The implementation is tzdb-free on every platform.
std::filesystem::file_time_type fileTimeFromSys(
        std::chrono::system_clock::time_point instant);

} // namespace showroom

#endif // SHOWROOM_NET_FILE_TIME_H
