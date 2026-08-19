// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/file_time.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>

namespace showroom {
namespace {

std::chrono::system_clock::time_point sysFromUnix(long long secs)
{
    return std::chrono::system_clock::time_point(std::chrono::seconds(secs));
}

// The current instant must land near file_clock's own now(): the two
// clocks differ only by a fixed epoch offset, so the mapping of "now"
// cannot drift far. A wide tolerance keeps the anchor free of scheduler
// jitter while still catching an offset computed the wrong way round.
TEST(FileTimeFromSys, maps_now_near_file_clock_now)
{
    const auto mapped = fileTimeFromSys(std::chrono::system_clock::now());
    const auto reference = std::filesystem::file_time_type::clock::now();
    const auto gap = std::chrono::abs(
            std::chrono::duration_cast<std::chrono::seconds>(mapped - reference));
    EXPECT_LE(gap.count(), 2);
}

// A later instant maps to a later timestamp, and the gap is preserved
// exactly: the epoch offset cancels, so this holds regardless of when
// the test runs. The download path relies on both properties.
TEST(FileTimeFromSys, preserves_ordering_and_interval)
{
    const auto earlier = fileTimeFromSys(sysFromUnix(1000000000));
    const auto later = fileTimeFromSys(sysFromUnix(1000003600));
    EXPECT_LT(earlier, later);
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(later - earlier).count(),
              3600);
}

// The epoch itself and instants before it convert without trapping on
// the duration arithmetic, and keep their order.
TEST(FileTimeFromSys, handles_epoch_and_before)
{
    EXPECT_NO_THROW({ (void)fileTimeFromSys(sysFromUnix(0)); });
    EXPECT_NO_THROW({ (void)fileTimeFromSys(sysFromUnix(-86400)); });
    EXPECT_LT(fileTimeFromSys(sysFromUnix(-86400)), fileTimeFromSys(sysFromUnix(0)));
}

} // namespace
} // namespace showroom
