// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "app/settings.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include <fstream>

namespace showroom {
namespace {

class SettingsFixture : public ::testing::Test {
protected:
    std::filesystem::path file() const
    {
        return std::filesystem::path(dir_.path().toStdString()) / "showroom.ini";
    }

    QTemporaryDir dir_;
};

TEST_F(SettingsFixture, the_port_notice_defaults_to_shown)
{
    Settings settings(file());

    EXPECT_TRUE(settings.showPortNotice());
}

TEST_F(SettingsFixture, suppressing_the_port_notice_survives_a_restart)
{
    {
        Settings settings(file());
        settings.setShowPortNotice(false);
    }

    Settings reopened(file());
    EXPECT_FALSE(reopened.showPortNotice());
}

TEST_F(SettingsFixture, a_corrupt_value_fails_toward_showing_the_notice)
{
    {
        std::ofstream out(file());
        out << "[launch]\nshow_port_notice=maybe\n";
    }

    Settings settings(file());
    EXPECT_TRUE(settings.showPortNotice());
}

} // namespace
} // namespace showroom
