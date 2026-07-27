// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

// QProcess signal delivery and QSignalSpy::wait need a Qt event loop,
// which gtest_main cannot arrange. Core only: no widgets in this tier.

#include <QCoreApplication>

#include <gtest/gtest.h>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
