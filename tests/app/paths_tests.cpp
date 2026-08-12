// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "app/paths.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace showroom {
namespace {

// The cache goes somewhere disposable: a test that writes into the
// developer's own cache changes the machine it runs on.
class PathsFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        saved_cache_ = savedValue("SHOWROOM_CACHE_DIR");
        saved_xdg_ = savedValue("XDG_CACHE_HOME");
        saved_engine_ = savedValue("SHOWROOM_ENGINE_BINARY");
        qputenv("SHOWROOM_CACHE_DIR", QByteArray(base().string().c_str()));
        qunsetenv("SHOWROOM_ENGINE_BINARY");
    }

    void TearDown() override
    {
        restore("SHOWROOM_CACHE_DIR", saved_cache_);
        restore("XDG_CACHE_HOME", saved_xdg_);
        restore("SHOWROOM_ENGINE_BINARY", saved_engine_);
    }

    std::filesystem::path base() const
    {
        return std::filesystem::path(dir_.path().toStdString()) / "cache";
    }

    static std::optional<std::string> savedValue(const char* name)
    {
        const char* value = std::getenv(name);
        if (value == nullptr) {
            return std::nullopt;
        }
        return std::string(value);
    }

    static void restore(const char* name, const std::optional<std::string>& value)
    {
        if (value.has_value()) {
            qputenv(name, QByteArray(value->c_str()));
        } else {
            qunsetenv(name);
        }
    }

    QTemporaryDir dir_;
    std::optional<std::string> saved_cache_;
    std::optional<std::string> saved_xdg_;
    std::optional<std::string> saved_engine_;
};

TEST_F(PathsFixture, the_cache_base_can_be_pointed_somewhere_else)
{
    EXPECT_EQ(Paths::cacheDir(), base());
    EXPECT_TRUE(std::filesystem::is_directory(base()));
}

// A derived path that leaves the base leaves the mount whitelist the
// base anchors.
TEST_F(PathsFixture, the_derived_directories_stay_below_the_cache_base)
{
    EXPECT_EQ(Paths::downloadsDir(), base() / "downloads");
    EXPECT_EQ(Paths::installsDir(), base() / "installs");
    EXPECT_TRUE(isWithin(base(), Paths::downloadsDir()));
    EXPECT_TRUE(isWithin(base(), Paths::installsDir()));
}

TEST_F(PathsFixture, the_derived_directories_are_created_on_first_use)
{
    EXPECT_TRUE(std::filesystem::is_directory(Paths::downloadsDir()));
    EXPECT_TRUE(std::filesystem::is_directory(Paths::installsDir()));
}

// The cache holds the run conf and whole game installs; a world-readable
// directory gives away both.
TEST_F(PathsFixture, created_directories_are_private_to_the_user)
{
    for (const std::filesystem::path& dir :
         {Paths::cacheDir(), Paths::downloadsDir(), Paths::installsDir()}) {
        EXPECT_EQ(std::filesystem::status(dir).permissions(),
                  std::filesystem::perms::owner_all)
                << dir.string();
    }
}

// The engine takes the last -conf file's directory as an allowed mount
// root, so moving this file silently stops the installs being mountable.
TEST_F(PathsFixture, the_run_conf_sits_directly_in_the_cache_base)
{
    EXPECT_EQ(Paths::runConfFile(), base() / "run.conf");
    EXPECT_EQ(Paths::runConfFile().parent_path(), Paths::cacheDir());
}

TEST_F(PathsFixture, the_engine_binary_override_wins_when_it_exists)
{
    const auto engine = std::filesystem::path(dir_.path().toStdString())
                      / "engine-binary";
    std::ofstream(engine) << "stub";
    qputenv("SHOWROOM_ENGINE_BINARY", QByteArray(engine.string().c_str()));

    EXPECT_EQ(Paths::engineBinary(), engine);
}

TEST_F(PathsFixture, a_missing_engine_binary_override_falls_back)
{
    const auto missing = std::filesystem::path(dir_.path().toStdString())
                       / "no-such-engine";
    qputenv("SHOWROOM_ENGINE_BINARY", QByteArray(missing.string().c_str()));

    EXPECT_NE(Paths::engineBinary(), missing);
}

TEST_F(PathsFixture, the_engine_binary_defaults_to_beside_the_executable)
{
#ifdef _WIN32
    const std::filesystem::path expected_name = "dosbox.exe";
#else
    const std::filesystem::path expected_name = "dosbox";
#endif
    const auto path = Paths::engineBinary();
    EXPECT_EQ(path.filename(), expected_name);
    EXPECT_EQ(path.parent_path(),
              std::filesystem::path(
                      QCoreApplication::applicationDirPath().toStdString()));
}

TEST_F(PathsFixture, a_safe_slug_lands_in_its_own_directory)
{
    const auto install = Paths::installDirFor("doom");
    ASSERT_TRUE(install.has_value());
    EXPECT_EQ(*install, base() / "installs" / "doom");

    const auto download = Paths::downloadDirFor("one-must-fall-2097");
    ASSERT_TRUE(download.has_value());
    EXPECT_EQ(*download, base() / "downloads" / "one-must-fall-2097");
}

// The slugs the parser already rejects, fed back in: today they can only
// arrive through it, and today is not a guarantee worth a traversal.
TEST_F(PathsFixture, a_slug_that_could_climb_out_of_the_cache_is_refused)
{
    const std::vector<std::string> rejected = {"",
                                               "..",
                                               "../..",
                                               "../../etc",
                                               "/etc",
                                               "a/b",
                                               "doom/../..",
                                               ".",
                                               "./doom",
                                               "-doom",
                                               "DOOM",
                                               "doom doom",
                                               "doom;id",
                                               "doom\\..",
                                               "d\"m",
                                               "doom\n",
                                               std::string("doom\0evil", 9)};

    for (const std::string& slug : rejected) {
        EXPECT_FALSE(Paths::installDirFor(slug).has_value()) << "install: " << slug;
        EXPECT_FALSE(Paths::downloadDirFor(slug).has_value()) << "download: " << slug;
    }
}

TEST_F(PathsFixture, the_cache_base_follows_xdg_cache_home_when_nothing_overrides_it)
{
    qunsetenv("SHOWROOM_CACHE_DIR");
    const std::filesystem::path xdg = std::filesystem::path(dir_.path().toStdString())
                                    / "xdg";
    qputenv("XDG_CACHE_HOME", QByteArray(xdg.string().c_str()));

    const std::filesystem::path cache = Paths::cacheDir();
    EXPECT_TRUE(isWithin(xdg, cache)) << cache.string() << " not below " << xdg.string();
}

// Short is not the same as obviously correct.
TEST(PathsWithin, a_path_outside_the_base_is_not_within_it)
{
    EXPECT_TRUE(isWithin("/a/b", "/a/b"));
    EXPECT_TRUE(isWithin("/a/b", "/a/b/c"));
    EXPECT_TRUE(isWithin("/a/b", "/a/b/c/../d"));
    EXPECT_FALSE(isWithin("/a/b", "/a/c"));
    EXPECT_FALSE(isWithin("/a/b", "/a/b/../c"));
    EXPECT_FALSE(isWithin("/a/b", "/a"));
    EXPECT_FALSE(isWithin("/a/b", "/a/bb"));
    EXPECT_FALSE(isWithin("/a/b", "relative/path"));
    EXPECT_FALSE(isWithin("", "/a/b"));
}

} // namespace
} // namespace showroom
