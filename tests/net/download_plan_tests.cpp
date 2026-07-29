// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/download_plan.h"

#include "model/game_definition.h"

#include <gtest/gtest.h>

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

} // namespace
} // namespace showroom
