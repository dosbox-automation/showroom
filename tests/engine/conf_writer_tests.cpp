// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/conf_writer.h"

#include "model/game_definition.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

using showroom::ConfWriter;
using showroom::GameDefinition;

// Shaped like the real doom.toml; the nested working_dir exercises the
// separator conversion.
std::string doomLikeToml(const std::string& machine = "svga_s3",
                         const std::string& primary_install_type = "floppyinstall")
{
    return std::format(R"(slug = "doom"
title = "DOOM"
rank = 1
license = "shareware"
recipe_status = "done"

[sources.primary]
role = "primary"
install_type = "{}"
url = "https://example.org/doom.7z"

[dosbox]
machine = "{}"
cpu_cycles = 12000
cpu_cycles_protected = 12000

[dosbox.sound]
sblaster_type = "sb16"
mpu401 = "intelligent"
midi_device = "fluidsynth"

[launch]
executable = "DOOM.EXE"
working_dir = "GOLD/DOOM"
setup_exe = "SETUP.EXE"

[install]
max_runtime_seconds = 120
)",
                       primary_install_type,
                       machine);
}

GameDefinition parseOrDie(const std::string& toml)
{
    std::string error;
    auto game = GameDefinition::fromTomlString(toml, error);
    EXPECT_TRUE(game) << error;
    return *game;
}

std::vector<std::string> lines(const std::string& text)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        result.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return result;
}

bool hasLine(const std::string& text, const std::string& wanted)
{
    const auto all = lines(text);
    return std::find(all.begin(), all.end(), wanted) != all.end();
}

class ConfWriterDir : public testing::Test {
protected:
    void SetUp() override
    {
        const auto* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path()
             / ("showroom-conf-"
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

TEST_F(ConfWriterDir, renders_engine_settings_from_the_definition)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    EXPECT_TRUE(hasLine(*conf, "[sdl]"));
    EXPECT_TRUE(hasLine(*conf, "output = texture"));
    EXPECT_TRUE(hasLine(*conf, "[dosbox]"));
    EXPECT_TRUE(hasLine(*conf, "machine = svga_s3"));
    EXPECT_TRUE(hasLine(*conf, "[cpu]"));
    EXPECT_TRUE(hasLine(*conf, "cpu_cycles = 12000"));
    EXPECT_TRUE(hasLine(*conf, "cpu_cycles_protected = 12000"));
    EXPECT_TRUE(hasLine(*conf, "[sblaster]"));
    EXPECT_TRUE(hasLine(*conf, "sbtype = sb16"));
    EXPECT_TRUE(hasLine(*conf, "[midi]"));
    EXPECT_TRUE(hasLine(*conf, "mididevice = fluidsynth"));
    EXPECT_TRUE(hasLine(*conf, "mpu401 = intelligent"));
    EXPECT_TRUE(hasLine(*conf, "[webserver]"));
    EXPECT_TRUE(hasLine(*conf, "webserver_enabled = true"));
    EXPECT_TRUE(hasLine(*conf, "webserver_token_file = false"));
    // Off the engine default 8386, which a developer instance on the same
    // box usually owns.
    EXPECT_TRUE(hasLine(*conf, "webserver_port = 8686"));
}

TEST_F(ConfWriterDir, autoexec_mounts_installs_under_the_cache_base_and_runs_the_game)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    const auto installs = (dir_ / "installs").string();
    EXPECT_TRUE(hasLine(*conf, "[autoexec]"));
    EXPECT_TRUE(hasLine(*conf, "mount c \"" + installs + "\""));
    EXPECT_TRUE(hasLine(*conf, "c:"));
    EXPECT_TRUE(hasLine(*conf, "cd \\doom\\GOLD\\DOOM"));
    EXPECT_TRUE(hasLine(*conf, "DOOM.EXE"));
    EXPECT_TRUE(hasLine(*conf, "exit"));
}

TEST_F(ConfWriterDir, empty_working_dir_changes_into_the_slug_directory_alone)
{
    auto toml = doomLikeToml();
    const auto pos = toml.find("working_dir = \"GOLD/DOOM\"");
    ASSERT_NE(pos, std::string::npos);
    toml.replace(pos, 25, "working_dir = \"\"");
    const auto game = parseOrDie(toml);

    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_TRUE(hasLine(*conf, "cd \\doom"));
}

TEST_F(ConfWriterDir, cd_title_mounts_its_download_directory_as_cdrom)
{
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    const auto downloads = (dir_ / "downloads" / "doom").string();
    EXPECT_TRUE(hasLine(*conf, "mount d \"" + downloads + "\" -t cdrom"));
}

TEST_F(ConfWriterDir, floppy_title_gets_no_cd_mount)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_EQ(conf->find("mount d"), std::string::npos);
}

TEST_F(ConfWriterDir, conf_carries_no_mount_whitelist_keys)
{
    // The whitelist keys are primary-config-only; writing them here
    // would be a misleading no-op.
    for (const auto* type : {"floppyinstall", "isoinstall"}) {
        const auto game = parseOrDie(doomLikeToml("svga_s3", type));
        std::string error;
        const auto conf = ConfWriter::renderConf(game, dir_, error);
        ASSERT_TRUE(conf) << error;
        EXPECT_EQ(conf->find("mount_allowed_bases"), std::string::npos);
        EXPECT_EQ(conf->find("mount_allowed_image_roots"), std::string::npos);
    }
}

TEST_F(ConfWriterDir, every_autoexec_mount_path_stays_under_the_cache_base)
{
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    for (const auto& line : lines(*conf)) {
        if (!line.starts_with("mount ")) {
            continue;
        }
        const auto open = line.find('"');
        const auto close = line.rfind('"');
        ASSERT_NE(open, std::string::npos) << line;
        ASSERT_GT(close, open) << line;
        const std::filesystem::path mounted = line.substr(open + 1, close - open - 1);
        auto rel = mounted.lexically_relative(dir_);
        EXPECT_FALSE(rel.empty()) << line;
        EXPECT_NE(rel.begin()->string(), "..") << line;
    }
}

TEST_F(ConfWriterDir, refuses_a_definition_without_an_executable)
{
    auto toml = doomLikeToml();
    const auto pos = toml.find("executable = \"DOOM.EXE\"");
    ASSERT_NE(pos, std::string::npos);
    toml.replace(pos, 23, "executable = \"\"");
    const auto game = parseOrDie(toml);

    std::string error;
    EXPECT_FALSE(ConfWriter::renderConf(game, dir_, error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(ConfWriter::writeConf(game, dir_, error));
    EXPECT_FALSE(std::filesystem::exists(dir_ / "run.conf"));
}

TEST_F(ConfWriterDir, refuses_a_machine_string_that_breaks_out_of_its_line)
{
    // A newline in a conf value forges keys or whole sections; TOML basic
    // strings can smuggle one in as an escape.
    const auto game = parseOrDie(doomLikeToml("svga_s3\\n[autoexec]"));
    std::string error;
    EXPECT_FALSE(ConfWriter::renderConf(game, dir_, error));
    EXPECT_NE(error.find("machine"), std::string::npos) << error;
}

TEST_F(ConfWriterDir, refuses_sound_values_outside_the_conservative_charset)
{
    auto toml = doomLikeToml();
    const auto pos = toml.find("midi_device = \"fluidsynth\"");
    ASSERT_NE(pos, std::string::npos);
    toml.replace(pos, 26, "midi_device = \"fluid synth\"");
    const auto game = parseOrDie(toml);

    std::string error;
    EXPECT_FALSE(ConfWriter::renderConf(game, dir_, error));
    EXPECT_NE(error.find("midi_device"), std::string::npos) << error;
}

TEST_F(ConfWriterDir, refuses_a_relative_cache_base)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    EXPECT_FALSE(ConfWriter::renderConf(game, "relative/cache", error));
    EXPECT_FALSE(error.empty());
}

TEST_F(ConfWriterDir, refuses_a_cache_base_a_conf_line_cannot_carry)
{
    const auto game = parseOrDie(doomLikeToml());
    for (const auto* bad : {"with\"quote", "with\nnewline"}) {
        std::string error;
        EXPECT_FALSE(ConfWriter::renderConf(game, dir_ / bad, error)) << bad;
        EXPECT_FALSE(error.empty());
    }
}

TEST_F(ConfWriterDir, a_working_dir_escaping_the_install_directory_never_parses)
{
    // The parser is the reachable line of defence; ConfWriter re-checks
    // behind it.
    auto toml = doomLikeToml();
    const auto pos = toml.find("working_dir = \"GOLD/DOOM\"");
    ASSERT_NE(pos, std::string::npos);
    toml.replace(pos, 25, "working_dir = \"../outside\"");

    std::string error;
    EXPECT_FALSE(GameDefinition::fromTomlString(toml, error));
    EXPECT_NE(error.find("working_dir"), std::string::npos) << error;
}

TEST_F(ConfWriterDir, write_lands_exactly_at_the_cache_base_run_conf)
{
    // The conf's directory is the mount anchor; if this path moves, the
    // policy silently stops covering the cache.
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    const auto written = ConfWriter::writeConf(game, dir_, error);
    ASSERT_TRUE(written) << error;
    EXPECT_EQ(*written, dir_ / "run.conf");
    ASSERT_TRUE(std::filesystem::exists(*written));

    std::ifstream in(*written, std::ios::binary);
    std::string on_disk((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    const auto rendered = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(rendered) << error;
    EXPECT_EQ(on_disk, *rendered);
}

TEST_F(ConfWriterDir, written_conf_is_private_to_the_user)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    const auto written = ConfWriter::writeConf(game, dir_, error);
    ASSERT_TRUE(written) << error;

    const auto mode = std::filesystem::status(*written).permissions();
    using std::filesystem::perms;
    EXPECT_EQ(mode & (perms::group_all | perms::others_all), perms::none);
    EXPECT_NE(mode & perms::owner_read, perms::none);
    EXPECT_NE(mode & perms::owner_write, perms::none);
}

TEST_F(ConfWriterDir, write_leaves_no_temporary_files_behind)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    ASSERT_TRUE(ConfWriter::writeConf(game, dir_, error)) << error;

    std::vector<std::string> entries;
    for (const auto& entry : std::filesystem::directory_iterator(dir_)) {
        entries.push_back(entry.path().filename().string());
    }
    EXPECT_EQ(entries, std::vector<std::string>{"run.conf"});
}

TEST_F(ConfWriterDir, write_replaces_an_existing_run_conf)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    {
        std::ofstream stale(dir_ / "run.conf");
        stale << "stale contents from a previous run\n";
    }
    const auto written = ConfWriter::writeConf(game, dir_, error);
    ASSERT_TRUE(written) << error;

    std::ifstream in(*written, std::ios::binary);
    std::string on_disk((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ(on_disk.find("stale"), std::string::npos);
    EXPECT_NE(on_disk.find("[autoexec]"), std::string::npos);
}

TEST_F(ConfWriterDir, write_refuses_a_missing_cache_base)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    EXPECT_FALSE(ConfWriter::writeConf(game, dir_ / "does-not-exist", error));
    EXPECT_FALSE(error.empty());
}

} // namespace
