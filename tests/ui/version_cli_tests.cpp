// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "ui/version.h"

#include <gtest/gtest.h>

#include <QProcess>
#include <QProcessEnvironment>

namespace showroom {
namespace {

// The release checklist requires --version and --help to answer without
// a display server, so the probe runs the real binary with no platform
// plugin available at all.
QProcess::ProcessError runShowroom(const QString& argument, QString& out, int& exit_code)
{
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("DISPLAY"));
    env.remove(QStringLiteral("WAYLAND_DISPLAY"));
    env.remove(QStringLiteral("QT_QPA_PLATFORM"));
    process.setProcessEnvironment(env);

    process.start(QStringLiteral(SHOWROOM_BINARY_PATH), {argument});
    if (!process.waitForFinished(5000)) {
        process.kill();
        return QProcess::Timedout;
    }
    out = QString::fromUtf8(process.readAllStandardOutput());
    exit_code = process.exitCode();
    return process.error();
}

TEST(VersionCli, version_answers_without_a_display_and_names_the_build)
{
    QString out;
    int exit_code = -1;

    ASSERT_NE(runShowroom(QStringLiteral("--version"), out, exit_code),
              QProcess::Timedout)
            << "--version tried to start the GUI";

    EXPECT_EQ(exit_code, 0);
    EXPECT_TRUE(out.contains(QStringLiteral(SHOWROOM_VERSION))) << out.toStdString();
    EXPECT_TRUE(out.contains(QLatin1String(kShowroomGitHash))) << out.toStdString();
    EXPECT_TRUE(out.contains(QStringLiteral(SHOWROOM_ENGINE_VERSION)))
            << out.toStdString();
}

TEST(VersionCli, help_answers_without_a_display)
{
    QString out;
    int exit_code = -1;

    ASSERT_NE(runShowroom(QStringLiteral("--help"), out, exit_code),
              QProcess::Timedout)
            << "--help tried to start the GUI";

    EXPECT_EQ(exit_code, 0);
    EXPECT_TRUE(out.contains(QStringLiteral("--version"))) << out.toStdString();
}

} // namespace
} // namespace showroom
