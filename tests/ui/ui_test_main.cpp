// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

// A QApplication must exist before any widget and outlive all of them,
// which gtest_main cannot arrange. Offscreen is forced here rather than
// left to the environment: a run that quietly needs a display passes
// here and hangs on the build VM.

#include <QApplication>

#include <gtest/gtest.h>

#include <cstdlib>

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
