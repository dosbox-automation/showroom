// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/install_check.h"

#include "model/game_definition.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace showroom {
namespace {

GameDefinition parseOrDie(const std::string& toml)
{
    std::string error;
    auto game = GameDefinition::fromTomlString(toml, error);
    if (!game) {
        ADD_FAILURE() << error;
        std::abort();
    }
    return *game;
}

std::string gameWithExpectedFiles(const std::string& files_toml)
{
    return "slug = \"probe\"\n"
           "title = \"Probe\"\n"
           "rank = 1\n"
           "license = \"shareware\"\n"
           "[sources.primary]\n"
           "url = \"https://example.invalid/probe.zip\"\n"
           "[dosbox]\n"
           "machine = \"svga_s3\"\n"
           "cpu_cycles = 3000\n"
           "cpu_cycles_protected = 3000\n"
           "[launch]\n"
           "executable = \"PROBE.EXE\"\n"
           "[install]\n"
           "max_runtime_seconds = 60\n"
           "[install.expected_files]\n"
         + files_toml;
}

void writeBytes(const std::filesystem::path& path, std::size_t count)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << std::string(count, 'x');
}

class InstallCheckDir : public testing::Test {
protected:
    void SetUp() override
    {
        const auto* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path()
             / ("showroom-check-"
                + std::to_string(testing::UnitTest::GetInstance()->random_seed()) + "-"
                + info->name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
        std::filesystem::permissions(dir_,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    std::filesystem::path dir_;
};

TEST_F(InstallCheckDir, an_intact_install_reports_no_damage)
{
    const auto game = parseOrDie(
            gameWithExpectedFiles("\"SUB/PROBE.EXE\" = { size = 10 }\n"
                                  "\"PROBE.CFG\" = {}\n"));
    writeBytes(dir_ / "SUB" / "PROBE.EXE", 10);
    writeBytes(dir_ / "PROBE.CFG", 3);

    EXPECT_TRUE(installDamage(game, dir_).empty());
}

TEST_F(InstallCheckDir, a_missing_file_is_named)
{
    const auto game = parseOrDie(
            gameWithExpectedFiles("\"SUB/PROBE.EXE\" = { size = 10 }\n"));

    const auto damage = installDamage(game, dir_);
    ASSERT_EQ(damage.size(), 1u);
    EXPECT_NE(damage.front().find("SUB/PROBE.EXE"), std::string::npos);
    EXPECT_NE(damage.front().find("missing"), std::string::npos);
}

TEST_F(InstallCheckDir, a_wrong_size_is_named_with_both_numbers)
{
    const auto game = parseOrDie(
            gameWithExpectedFiles("\"PROBE.EXE\" = { size = 10 }\n"));
    writeBytes(dir_ / "PROBE.EXE", 7);

    const auto damage = installDamage(game, dir_);
    ASSERT_EQ(damage.size(), 1u);
    EXPECT_NE(damage.front().find("PROBE.EXE"), std::string::npos);
    EXPECT_NE(damage.front().find("7"), std::string::npos);
    EXPECT_NE(damage.front().find("10"), std::string::npos);
}

TEST_F(InstallCheckDir, a_file_without_a_declared_size_only_needs_to_exist)
{
    const auto game = parseOrDie(gameWithExpectedFiles("\"PROBE.CFG\" = {}\n"));
    writeBytes(dir_ / "PROBE.CFG", 99);

    EXPECT_TRUE(installDamage(game, dir_).empty());
}

TEST_F(InstallCheckDir, every_problem_is_reported_not_just_the_first)
{
    const auto game = parseOrDie(
            gameWithExpectedFiles("\"A.DAT\" = { size = 5 }\n"
                                  "\"B.DAT\" = { size = 5 }\n"));
    writeBytes(dir_ / "B.DAT", 2);

    EXPECT_EQ(installDamage(game, dir_).size(), 2u);
}

TEST_F(InstallCheckDir, a_definition_with_no_expected_files_is_always_intact)
{
    const auto game = parseOrDie(
            "slug = \"probe\"\n"
            "title = \"Probe\"\n"
            "rank = 1\n"
            "license = \"shareware\"\n"
            "[sources.primary]\n"
            "url = \"https://example.invalid/probe.zip\"\n"
            "[dosbox]\n"
            "machine = \"svga_s3\"\n"
            "cpu_cycles = 3000\n"
            "cpu_cycles_protected = 3000\n"
            "[launch]\n"
            "executable = \"PROBE.EXE\"\n"
            "[install]\n"
            "max_runtime_seconds = 60\n");

    EXPECT_TRUE(installDamage(game, dir_).empty());
}

TEST_F(InstallCheckDir, a_directory_where_a_file_is_expected_is_damage)
{
    const auto game = parseOrDie(
            gameWithExpectedFiles("\"PROBE.EXE\" = { size = 10 }\n"));
    std::filesystem::create_directories(dir_ / "PROBE.EXE");

    EXPECT_EQ(installDamage(game, dir_).size(), 1u);
}

} // namespace
} // namespace showroom
