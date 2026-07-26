// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include <gtest/gtest.h>

// Proves the test harness itself builds, links and runs before any real
// test depends on it.
TEST(BuildSkeleton, gtest_harness_runs_and_reports)
{
    EXPECT_EQ(2 + 2, 4);
}
