// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/download_plan.h"

#include "model/game_definition.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace showroom {
namespace {

GameDefinition gameWithSources(const std::string& sources_toml)
{
    const std::string
            toml = "slug = \"probe\"\n"
                   "title = \"Probe\"\n"
                   "rank = 1\n"
                   "license = \"shareware\"\n"
                 + sources_toml
                 + "[dosbox]\n"
                   "machine = \"svga_s3\"\n"
                   "cpu_cycles = 3000\n"
                   "cpu_cycles_protected = 3000\n"
                   "[launch]\n"
                   "executable = \"PROBE.EXE\"\n"
                   "[install]\n"
                   "max_runtime_seconds = 60\n";
    std::string error;
    auto game = GameDefinition::fromTomlString(toml, error);
    if (!game) {
        ADD_FAILURE() << error;
        std::abort();
    }
    return *game;
}

TEST(DownloadPlan, uses_the_primary_source_with_its_declared_fields)
{
    const auto game = gameWithSources(
            "[sources.mirror]\n"
            "role = \"mirror\"\n"
            "url = \"https://mirror.invalid/other.zip\"\n"
            "[sources.primary]\n"
            "role = \"primary\"\n"
            "url = \"https://example.invalid/dir/probe.zip\"\n"
            "filename = \"renamed.zip\"\n"
            "size = 12345\n");

    const auto plan = downloadPlanFor(game);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(plan->url, "https://example.invalid/dir/probe.zip");
    EXPECT_EQ(plan->filename, "renamed.zip");
    EXPECT_EQ(plan->size, 12345u);
}

TEST(DownloadPlan, falls_back_to_the_sanitized_url_basename)
{
    // Space and query survive as underscores and absence: the cache name
    // only has to be safe and stable, not identical to the server's.
    const auto game = gameWithSources(
            "[sources.primary]\n"
            "url = \"https://example.invalid/dir/Probe%20v1.zip?token=abc\"\n");

    const auto plan = downloadPlanFor(game);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(plan->filename, "Probe_v1.zip");
    EXPECT_FALSE(plan->size.has_value());
}

TEST(DownloadPlan, a_url_yielding_no_safe_filename_produces_no_plan)
{
    // A path ending in a slash has no basename; a plan built anyway would
    // write to the downloads directory itself.
    const auto game = gameWithSources(
            "[sources.primary]\n"
            "url = \"https://example.invalid/dir/\"\n");

    EXPECT_FALSE(downloadPlanFor(game).has_value());
}

TEST(DownloadPlan, a_hostile_basename_produces_no_plan)
{
    const auto game = gameWithSources(
            "[sources.primary]\n"
            "url = \"https://example.invalid/%2E%2E/archive.zip/..\"\n");

    EXPECT_FALSE(downloadPlanFor(game).has_value());
}

TEST(DownloadPlan, every_source_yields_a_plan_in_role_order_with_its_own_install_type)
{
    // classicdosgames.com served a wrong certificate for a day and three
    // games with perfectly healthy mirrors stayed dead (aug-ctpt): the
    // mirror entries must reach the downloader.
    const auto game = gameWithSources(
            "[sources.mirror]\n"
            "role = \"mirror\"\n"
            "install_type = \"unzip\"\n"
            "url = \"https://mirror.invalid/DOSBOX_PROBE.ZIP\"\n"
            "[sources.primary]\n"
            "role = \"primary\"\n"
            "install_type = \"unzipinstall\"\n"
            "url = \"https://example.invalid/dir/probe.zip\"\n");

    const auto plans = downloadPlansFor(game);
    ASSERT_EQ(plans.size(), 2u);
    EXPECT_EQ(plans[0].url, "https://example.invalid/dir/probe.zip");
    EXPECT_EQ(plans[0].install_type, InstallType::UnzipInstall);
    EXPECT_EQ(plans[1].url, "https://mirror.invalid/DOSBOX_PROBE.ZIP");
    EXPECT_EQ(plans[1].filename, "DOSBOX_PROBE.ZIP");
    EXPECT_EQ(plans[1].install_type, InstallType::Unzip);
}

TEST(DownloadPlan, an_unusable_source_is_skipped_rather_than_fatal)
{
    const auto game = gameWithSources(
            "[sources.primary]\n"
            "role = \"primary\"\n"
            "url = \"https://example.invalid/dir/\"\n"
            "[sources.mirror]\n"
            "role = \"mirror\"\n"
            "url = \"https://mirror.invalid/probe.zip\"\n");

    const auto plans = downloadPlansFor(game);
    ASSERT_EQ(plans.size(), 1u);
    EXPECT_EQ(plans[0].url, "https://mirror.invalid/probe.zip");
}

TEST(DownloadPlan, the_archive_on_disk_names_the_plan_that_installs_it)
{
    // The installer must act on whichever source's file actually landed:
    // the mirror's filename and install type differ from the primary's.
    const auto game = gameWithSources(
            "[sources.primary]\n"
            "role = \"primary\"\n"
            "install_type = \"unzipinstall\"\n"
            "url = \"https://example.invalid/probe.zip\"\n"
            "[sources.mirror]\n"
            "role = \"mirror\"\n"
            "install_type = \"unzip\"\n"
            "url = \"https://mirror.invalid/DOSBOX_PROBE.ZIP\"\n");

    const std::filesystem::path workspace(SHOWROOM_TEST_WORKSPACE);
    const std::filesystem::path downloads = workspace / "plan-on-disk-probe";
    std::filesystem::remove_all(downloads);
    std::filesystem::create_directories(downloads);

    EXPECT_FALSE(archivePlanOnDisk(game, downloads).has_value());

    std::ofstream(downloads / "DOSBOX_PROBE.ZIP") << "zip";
    const auto mirror_plan = archivePlanOnDisk(game, downloads);
    ASSERT_TRUE(mirror_plan.has_value());
    EXPECT_EQ(mirror_plan->install_type, InstallType::Unzip);

    // With both on disk the primary outranks the mirror.
    std::ofstream(downloads / "probe.zip") << "zip";
    const auto primary_plan = archivePlanOnDisk(game, downloads);
    ASSERT_TRUE(primary_plan.has_value());
    EXPECT_EQ(primary_plan->install_type, InstallType::UnzipInstall);

    std::filesystem::remove_all(downloads);
}

} // namespace
} // namespace showroom
