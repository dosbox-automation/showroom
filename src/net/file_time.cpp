// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/file_time.h"

namespace showroom {

std::filesystem::file_time_type fileTimeFromSys(
        std::chrono::system_clock::time_point instant)
{
#ifdef _WIN32
    // MSVC's file_clock offers only from_utc/to_utc, so clock_cast<file_clock>
    // routes through utc_clock, whose first use loads the timezone database.
    // That init throws std::system_error ("module could not be found") on
    // Windows hosts without the ICU backing library (aug-qerw). A file mtime
    // needs no leap-second correctness, so the two clock epochs are bridged by
    // their runtime offset, which touches no tzdb. Reading both now() back to
    // back measures the fixed offset; any skew stays below one file_clock tick.
    using file_clock = std::chrono::file_clock;
    using file_duration = file_clock::duration;
    const auto file_now = file_clock::now().time_since_epoch();
    const auto sys_now = std::chrono::system_clock::now().time_since_epoch();
    const file_duration epoch_offset =
            file_now - std::chrono::duration_cast<file_duration>(sys_now);
    return std::filesystem::file_time_type(
            epoch_offset
            + std::chrono::duration_cast<file_duration>(instant.time_since_epoch()));
#else
    // libstdc++ and libc++ implement file_clock::from_sys, a direct
    // epoch-offset conversion with no timezone database in the path.
    return std::chrono::file_clock::from_sys(instant);
#endif
}

} // namespace showroom
