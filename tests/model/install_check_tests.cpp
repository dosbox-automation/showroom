// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/install_check.h"

#include "imported/log.h"
#include "model/game_definition.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

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
        // House rule: scratch lives in the project .workspace, not /tmp.
        dir_ = std::filesystem::path(SHOWROOM_TEST_WORKSPACE)
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

std::string gameWithInstallType(const std::string& install_type)
{
    return "slug = \"probe\"\n"
           "title = \"Probe\"\n"
           "rank = 1\n"
           "license = \"shareware\"\n"
           "[sources.primary]\n"
           "install_type = \""
         + install_type
         + "\"\n"
           "url = \"https://example.invalid/probe.iso\"\n"
           "[dosbox]\n"
           "machine = \"svga_s3\"\n"
           "cpu_cycles = 3000\n"
           "cpu_cycles_protected = 3000\n"
           "[launch]\n"
           "executable = \"PROBE.EXE\"\n"
           "[install]\n"
           "max_runtime_seconds = 60\n";
}

TEST_F(InstallCheckDir, a_cd_game_with_its_iso_reports_no_media_damage)
{
    const auto game = parseOrDie(gameWithInstallType("isoinstall"));
    writeBytes(dir_ / "probe.iso", 4);
    EXPECT_TRUE(mediaDamage(game, dir_).empty());
}

TEST_F(InstallCheckDir, a_cd_game_without_its_iso_reports_media_damage)
{
    const auto game = parseOrDie(gameWithInstallType("isoinstall"));
    writeBytes(dir_ / "README.TXT", 4);
    const auto damage = mediaDamage(game, dir_);
    ASSERT_EQ(damage.size(), 1u);
    EXPECT_NE(damage.front().find("ISO"), std::string::npos);
}

TEST_F(InstallCheckDir, a_missing_downloads_dir_counts_as_missing_media)
{
    const auto game = parseOrDie(gameWithInstallType("isoinstall"));
    EXPECT_EQ(mediaDamage(game, dir_ / "never-created").size(), 1u);
}

TEST_F(InstallCheckDir, a_non_cd_game_needs_no_media)
{
    const auto game = parseOrDie(gameWithInstallType("floppyinstall"));
    EXPECT_TRUE(mediaDamage(game, dir_).empty());
}

class VerifyInstallDir : public InstallCheckDir {
protected:
    void SetUp() override
    {
        InstallCheckDir::SetUp();
        Logger::instance().add_sink([this](LogLevel level,
                                           const char* component,
                                           const std::string& message) {
            if (level == LogLevel::Warn && std::string(component) == "install_check") {
                warnings_.push_back(message);
            }
        });
    }

    void TearDown() override
    {
        Logger::instance().clear_sinks();
        InstallCheckDir::TearDown();
    }

    std::vector<std::string> warnings_;
};

TEST_F(VerifyInstallDir, an_intact_install_verifies_without_warnings)
{
    const auto game = parseOrDie(
            gameWithExpectedFiles("\"SUB/PROBE.EXE\" = { size = 10 }\n"));
    writeBytes(dir_ / "SUB" / "PROBE.EXE", 10);

    EXPECT_TRUE(verifyInstall(game, dir_).empty());
    EXPECT_TRUE(warnings_.empty());
}

TEST_F(VerifyInstallDir, verification_failures_carry_through_from_the_damage_check)
{
    const auto game = parseOrDie(
            gameWithExpectedFiles("\"A.DAT\" = { size = 5 }\n"
                                  "\"B.DAT\" = { size = 5 }\n"));
    writeBytes(dir_ / "B.DAT", 2);

    const auto failures = verifyInstall(game, dir_);
    EXPECT_EQ(failures.size(), 2u);
}

TEST_F(VerifyInstallDir, an_empty_expectation_list_passes_but_says_so_out_loud)
{
    // A silent pass here would let a definition with no expected_files
    // claim every install succeeded.
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

    EXPECT_TRUE(verifyInstall(game, dir_).empty());
    ASSERT_EQ(warnings_.size(), 1u);
    EXPECT_NE(warnings_.front().find("probe"), std::string::npos);
}

TEST_F(VerifyInstallDir, a_missing_install_dir_fails_verification_rather_than_warning)
{
    const auto game = parseOrDie(
            gameWithExpectedFiles("\"PROBE.EXE\" = { size = 10 }\n"));

    const auto failures = verifyInstall(game, dir_ / "never-created");
    EXPECT_EQ(failures.size(), 1u);
    EXPECT_TRUE(warnings_.empty());
}

} // namespace
} // namespace showroom
