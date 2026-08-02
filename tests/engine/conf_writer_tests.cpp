// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/conf_writer.h"

#include "model/game_definition.h"

#include <gtest/gtest.h>

#include <algorithm>
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
        // House rule: scratch lives in the project .workspace, not /tmp.
        dir_ = std::filesystem::path(SHOWROOM_TEST_WORKSPACE)
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

    void addDownloadedFile(const std::string& name)
    {
        std::filesystem::create_directories(dir_ / "downloads" / "doom");
        std::ofstream out(dir_ / "downloads" / "doom" / name, std::ios::binary);
        out << "image bytes";
    }

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

TEST_F(ConfWriterDir, autoexec_mounts_the_games_own_install_dir_and_runs_the_game)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    const auto install = (dir_ / "installs" / "doom").string();
    EXPECT_TRUE(hasLine(*conf, "[autoexec]"));
    EXPECT_TRUE(hasLine(*conf, "mount c \"" + install + "\""));
    EXPECT_TRUE(hasLine(*conf, "c:"));
    EXPECT_TRUE(hasLine(*conf, "cd \\GOLD\\DOOM"));
    EXPECT_TRUE(hasLine(*conf, "DOOM.EXE"));
    EXPECT_TRUE(hasLine(*conf, "exit"));
}

TEST_F(ConfWriterDir, empty_working_dir_stays_at_the_drive_root)
{
    auto toml = doomLikeToml();
    const auto pos = toml.find("working_dir = \"GOLD/DOOM\"");
    ASSERT_NE(pos, std::string::npos);
    toml.replace(pos, 25, "working_dir = \"\"");
    const auto game = parseOrDie(toml);

    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_EQ(conf->find("cd \\"), std::string::npos);
}

TEST_F(ConfWriterDir, cd_title_mounts_the_downloaded_iso_image_as_cdrom)
{
    // The ISO never leaves downloads/; the run conf's anchor (the cache
    // base) is what makes that path mountable.
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    addDownloadedFile("game.iso");
    std::string error;
    const auto conf = ConfWriter::renderConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    const auto image = (dir_ / "downloads" / "doom" / "game.iso").string();
    EXPECT_TRUE(hasLine(*conf, "mount d \"" + image + "\" -t cdrom"));
}

TEST_F(ConfWriterDir, cd_title_without_a_downloaded_iso_is_refused)
{
    // A silent directory mount would boot the game with an empty D:.
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    std::string error;
    EXPECT_FALSE(ConfWriter::renderConf(game, dir_, error));
    EXPECT_NE(error.find("ISO"), std::string::npos) << error;
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
    addDownloadedFile("game.iso");
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
    addDownloadedFile("game.iso");
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

// Floppy and exe install confs anchor in the extracts dir: the engine
// treats the conf file's directory as an image root, which is what lets
// the recipe's bare-name drive_swap("A", "disk2.ima") pass mount
// policy. Iso install confs anchor at the cache base instead, so the
// ISO can mount from downloads/ without ever being copied.
class InstallConfDir : public ConfWriterDir {
protected:
    void SetUp() override
    {
        ConfWriterDir::SetUp();
        extracts_ = dir_ / "extracts" / "doom";
        std::filesystem::create_directories(extracts_);
    }

    void addExtractedFile(const std::string& name)
    {
        std::ofstream out(extracts_ / name, std::ios::binary);
        out << "image bytes";
    }

    std::filesystem::path extracts_;
};

TEST_F(InstallConfDir, install_conf_renders_the_same_engine_settings_as_play)
{
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("disk1.ima");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    EXPECT_TRUE(hasLine(*conf, "[sdl]"));
    EXPECT_TRUE(hasLine(*conf, "output = texture"));
    EXPECT_TRUE(hasLine(*conf, "machine = svga_s3"));
    EXPECT_TRUE(hasLine(*conf, "cpu_cycles = 12000"));
    EXPECT_TRUE(hasLine(*conf, "cpu_cycles_protected = 12000"));
    EXPECT_TRUE(hasLine(*conf, "sbtype = sb16"));
    EXPECT_TRUE(hasLine(*conf, "mididevice = fluidsynth"));
    EXPECT_TRUE(hasLine(*conf, "webserver_enabled = true"));
    EXPECT_TRUE(hasLine(*conf, "webserver_token_file = false"));
    EXPECT_TRUE(hasLine(*conf, "webserver_port = 8686"));
}

TEST_F(InstallConfDir, floppy_install_mounts_the_first_image_and_the_staging_dir)
{
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("disk1.ima");
    addExtractedFile("disk2.ima");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    const auto image = (extracts_ / "disk1.ima").string();
    // C: stages under the conf anchor; without a primary config the
    // anchor is the engine's only allowed directory-mount root.
    const auto installs = (extracts_ / "installs").string();
    EXPECT_TRUE(hasLine(*conf, "[autoexec]"));
    EXPECT_TRUE(hasLine(*conf, "mount a \"" + image + "\" -t floppy"));
    EXPECT_TRUE(hasLine(*conf, "mount c \"" + installs + "\""));
    EXPECT_EQ(conf->find("mount d"), std::string::npos);
}

TEST_F(InstallConfDir, floppy_images_are_picked_in_name_order_not_directory_order)
{
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("disk2.ima");
    addExtractedFile("disk1.ima");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_NE(conf->find("disk1.ima"), std::string::npos);
    EXPECT_EQ(conf->find("disk2.ima"), std::string::npos);
}

TEST_F(InstallConfDir, floppy_images_inside_a_subdirectory_are_found)
{
    // Disk sets often extract into a subdirectory named after the
    // archive (fotaq's six-disk 7z does).
    const auto game = parseOrDie(doomLikeToml());
    std::filesystem::create_directories(extracts_ / "diskset");
    std::ofstream(extracts_ / "diskset" / "disk1.img", std::ios::binary) << "image";
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_NE(conf->find((extracts_ / "diskset" / "disk1.img").string()),
              std::string::npos);
}

TEST_F(InstallConfDir, floppy_image_extension_matching_ignores_case)
{
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("GAME.IMG");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_NE(conf->find("GAME.IMG"), std::string::npos);
}

TEST_F(InstallConfDir, non_image_files_are_not_mounted_as_floppies)
{
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("README.TXT");
    addExtractedFile("zzz.ima");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_NE(conf->find("zzz.ima"), std::string::npos);
    EXPECT_EQ(conf->find("README.TXT"), std::string::npos);
}

TEST_F(InstallConfDir, floppy_install_without_any_floppy_image_is_refused)
{
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("README.TXT");
    std::string error;
    EXPECT_FALSE(ConfWriter::renderInstallConf(game, dir_, error));
    EXPECT_NE(error.find("floppy"), std::string::npos) << error;
}

TEST_F(InstallConfDir, cd_install_mounts_the_downloaded_iso_image_and_the_staging_dir)
{
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    addDownloadedFile("game.iso");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    const auto image = (dir_ / "downloads" / "doom" / "game.iso").string();
    EXPECT_TRUE(hasLine(*conf, "mount d \"" + image + "\" -t cdrom"));
    EXPECT_TRUE(hasLine(*conf, "mount c \"" + (extracts_ / "installs").string() + "\""));
    EXPECT_EQ(conf->find("mount a"), std::string::npos);
}

TEST_F(InstallConfDir, cd_install_without_a_downloaded_iso_is_refused)
{
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    addDownloadedFile("README.TXT");
    std::string error;
    EXPECT_FALSE(ConfWriter::renderInstallConf(game, dir_, error));
    EXPECT_NE(error.find("ISO"), std::string::npos) << error;
}

TEST_F(InstallConfDir, iso_images_are_picked_in_name_order_not_directory_order)
{
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    addDownloadedFile("disc2.iso");
    addDownloadedFile("disc1.iso");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_NE(conf->find("disc1.iso"), std::string::npos);
    EXPECT_EQ(conf->find("disc2.iso"), std::string::npos);
}

TEST_F(InstallConfDir, iso_extension_matching_ignores_case_and_other_files)
{
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    addDownloadedFile("GAME.ISO");
    addDownloadedFile("cover.jpg");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_NE(conf->find("GAME.ISO"), std::string::npos);
    EXPECT_EQ(conf->find("cover.jpg"), std::string::npos);
}

TEST_F(InstallConfDir, install_conf_neither_launches_the_game_nor_exits)
{
    // The recipe types the installer invocation itself and controls
    // shutdown; an autoexec launch would double-fire it.
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("disk1.ima");
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;
    EXPECT_EQ(conf->find("DOOM.EXE"), std::string::npos);
    EXPECT_EQ(conf->find("SETUP.EXE"), std::string::npos);
    EXPECT_FALSE(hasLine(*conf, "exit"));
    EXPECT_FALSE(hasLine(*conf, "c:"));
}

TEST_F(InstallConfDir, exe_install_mounts_the_extracts_dir_plain_and_the_staging_dir)
{
    // The self-extractor runs from D: as a DOS program, so the extracts
    // dir mounts plain, not as cdrom media.
    const auto game = parseOrDie(doomLikeToml("svga_s3", "exeinstall"));
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    EXPECT_TRUE(hasLine(*conf, "mount d \"" + extracts_.string() + "\""));
    EXPECT_TRUE(hasLine(*conf, "mount c \"" + (extracts_ / "installs").string() + "\""));
    EXPECT_EQ(conf->find("-t cdrom"), std::string::npos);
    EXPECT_EQ(conf->find("mount a"), std::string::npos);
}

TEST_F(InstallConfDir, unzipinstall_mounts_like_an_exe_install)
{
    const auto game = parseOrDie(doomLikeToml("svga_s3", "unzipinstall"));
    std::string error;
    const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(conf) << error;

    EXPECT_TRUE(hasLine(*conf, "mount d \"" + extracts_.string() + "\""));
    EXPECT_TRUE(hasLine(*conf, "mount c \"" + (extracts_ / "installs").string() + "\""));
    EXPECT_EQ(conf->find("-t cdrom"), std::string::npos);
}

TEST_F(InstallConfDir, an_install_type_the_engine_does_not_drive_is_refused)
{
    const auto game = parseOrDie(doomLikeToml("svga_s3", "unzip"));
    std::string error;
    EXPECT_FALSE(ConfWriter::renderInstallConf(game, dir_, error));
    EXPECT_FALSE(error.empty());
}

TEST_F(InstallConfDir, a_primary_source_without_an_install_type_is_refused)
{
    auto toml = doomLikeToml();
    const auto pos = toml.find("install_type = \"floppyinstall\"\n");
    ASSERT_NE(pos, std::string::npos);
    toml.erase(pos, 31);
    const auto game = parseOrDie(toml);

    std::string error;
    EXPECT_FALSE(ConfWriter::renderInstallConf(game, dir_, error));
    EXPECT_NE(error.find("install type"), std::string::npos) << error;
}

TEST_F(InstallConfDir, refuses_a_relative_cache_base)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    EXPECT_FALSE(ConfWriter::renderInstallConf(game, "cache", error));
    EXPECT_FALSE(error.empty());
}

TEST_F(InstallConfDir, refuses_a_cache_base_a_conf_line_cannot_carry)
{
    const auto game = parseOrDie(doomLikeToml());
    for (const auto* bad : {"with\"quote", "with\nnewline"}) {
        std::string error;
        EXPECT_FALSE(ConfWriter::renderInstallConf(game, dir_ / bad, error)) << bad;
        EXPECT_FALSE(error.empty());
    }
}

TEST_F(InstallConfDir, every_install_mount_path_stays_under_its_conf_anchor)
{
    // Whatever a conf mounts must sit under that conf's anchor, or the
    // mount is dead on arrival at runtime: the extracts dir for floppy
    // installs, the cache base for iso installs.
    struct Case {
        const char* type;
        std::filesystem::path anchor;
    };
    for (const auto& [type, anchor] :
         {Case{"floppyinstall", extracts_}, Case{"isoinstall", dir_}}) {
        const auto game = parseOrDie(doomLikeToml("svga_s3", type));
        addExtractedFile("disk1.ima");
        addDownloadedFile("game.iso");
        std::string error;
        const auto conf = ConfWriter::renderInstallConf(game, dir_, error);
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
            auto rel = mounted.lexically_relative(anchor);
            EXPECT_FALSE(rel.empty()) << line;
            EXPECT_NE(rel.begin()->string(), "..") << line;
        }
    }
}

TEST_F(InstallConfDir, write_lands_the_iso_install_conf_at_the_cache_base)
{
    // The anchor must cover the ISO in downloads/, which the extracts
    // dir never does.
    const auto game = parseOrDie(doomLikeToml("svga_s3", "isoinstall"));
    addDownloadedFile("game.iso");
    std::string error;
    const auto written = ConfWriter::writeInstallConf(game, dir_, error);
    ASSERT_TRUE(written) << error;
    EXPECT_EQ(*written, dir_ / "install.conf");
}

TEST_F(InstallConfDir, write_lands_the_install_conf_inside_the_extracts_dir)
{
    // If this path ever moves out of the extracts dir, multi-disk
    // installs break: drive_swap's bare image names stop resolving.
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("disk1.ima");
    std::string error;
    const auto written = ConfWriter::writeInstallConf(game, dir_, error);
    ASSERT_TRUE(written) << error;
    EXPECT_EQ(*written, extracts_ / "install.conf");
    ASSERT_TRUE(std::filesystem::exists(*written));

    std::ifstream in(*written, std::ios::binary);
    std::string on_disk((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    const auto rendered = ConfWriter::renderInstallConf(game, dir_, error);
    ASSERT_TRUE(rendered) << error;
    EXPECT_EQ(on_disk, *rendered);
}

TEST_F(InstallConfDir, written_install_conf_is_private_and_leaves_no_temp_files)
{
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("disk1.ima");
    std::string error;
    const auto written = ConfWriter::writeInstallConf(game, dir_, error);
    ASSERT_TRUE(written) << error;

    const auto mode = std::filesystem::status(*written).permissions();
    using std::filesystem::perms;
    EXPECT_EQ(mode & (perms::group_all | perms::others_all), perms::none);
    EXPECT_NE(mode & perms::owner_read, perms::none);
    EXPECT_NE(mode & perms::owner_write, perms::none);

    std::vector<std::string> entries;
    for (const auto& entry : std::filesystem::directory_iterator(extracts_)) {
        entries.push_back(entry.path().filename().string());
    }
    std::sort(entries.begin(), entries.end());
    EXPECT_EQ(entries, (std::vector<std::string>{"disk1.ima", "install.conf"}));
}

TEST_F(InstallConfDir, write_replaces_an_existing_install_conf)
{
    const auto game = parseOrDie(doomLikeToml());
    addExtractedFile("disk1.ima");
    {
        std::ofstream stale(extracts_ / "install.conf");
        stale << "stale contents from a previous install\n";
    }
    std::string error;
    const auto written = ConfWriter::writeInstallConf(game, dir_, error);
    ASSERT_TRUE(written) << error;

    std::ifstream in(*written, std::ios::binary);
    std::string on_disk((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ(on_disk.find("stale"), std::string::npos);
    EXPECT_NE(on_disk.find("[autoexec]"), std::string::npos);
}

TEST_F(InstallConfDir, write_refuses_a_missing_cache_base)
{
    const auto game = parseOrDie(doomLikeToml());
    std::string error;
    EXPECT_FALSE(ConfWriter::writeInstallConf(game, dir_ / "missing", error));
    EXPECT_FALSE(error.empty());
}

} // namespace
