// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

// The widget tests need a QApplication, which has to exist before any
// widget is constructed and outlive all of them. gtest_main cannot
// provide that, so this target brings its own entry point.
//
// The platform plugin is forced to offscreen here rather than left to
// the environment: a test run that quietly needs a display is a test
// run that passes on one machine and hangs on the build VM.

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
