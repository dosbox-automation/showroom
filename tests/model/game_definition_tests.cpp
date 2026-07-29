// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/game_definition.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace {

using showroom::GameDefinition;
using showroom::InstallType;
using showroom::License;
using showroom::RecipeStatus;

const std::filesystem::path kDoomToml = "assets/games/doom/doom.toml";

// A minimal valid definition. Tests override one thing at a time by
// swapping a fragment, so each failure names exactly one cause.
std::string minimalToml(const std::string& extra = {})
{
    return R"(
slug = "doom"
title = "DOOM"
rank = 1
version = "1.666"
license = "shareware"
recipeStatus = "done"

[sources.primary]
role = "primary"
install_type = "floppyinstall"
url = "https://example.org/doom.7z"

[dosbox]
machine = "svga_s3"
cpu_cycles = 12000
cpu_cycles_protected = 12000

[dosbox.sound]
sblaster_type = "sb16"
mpu401 = "intelligent"
midi_device = "fluidsynth"

[launch]
executable = "DOOM.EXE"
working_dir = ""
setup_exe = ""

[screenshots]
title = "title.png"
gameplay = "gameplay.png"

[install]
max_runtime_seconds = 120
)" + extra;
}

// A raw backslash is a TOML escape introducer, so "sub\DOOM.EXE" dies in
// the parser and never reaches the validator under test.
std::string tomlEscaped(const std::string& value)
{
    std::string out;
    for (const char c : value) {
        if (c == '\\' || c == '"') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

// Replaces the first occurrence, so a test can bend exactly one field.
std::string withReplacement(const std::string& needle, const std::string& replacement)
{
    std::string toml = minimalToml();
    const auto pos = toml.find(needle);
    EXPECT_NE(pos, std::string::npos) << "fixture no longer contains " << needle;
    return toml.replace(pos, needle.size(), replacement);
}

TEST(GameDefinition, parses_the_real_doom_definition)
{
    ASSERT_TRUE(std::filesystem::exists(kDoomToml))
            << "run ctest from the source directory; looked for "
            << std::filesystem::absolute(kDoomToml);

    std::string error;
    const auto doom = GameDefinition::fromToml(kDoomToml, error);

    ASSERT_TRUE(doom.has_value()) << error;
    EXPECT_EQ(doom->slug(), "doom");
    EXPECT_EQ(doom->title(), "DOOM");
    EXPECT_EQ(doom->rank(), 1);
    EXPECT_EQ(doom->version(), "1.666");
    EXPECT_EQ(doom->license(), License::Shareware);
    EXPECT_EQ(doom->recipeStatus(), RecipeStatus::Done);
    EXPECT_EQ(doom->dosbox().machine, "svga_s3");
    EXPECT_EQ(doom->dosbox().cpu_cycles, 12000);
    EXPECT_EQ(doom->dosbox().cpu_cycles_protected, 12000);
    EXPECT_EQ(doom->dosbox().sound.sblaster_type, "sb16");
    EXPECT_EQ(doom->launch().executable, "DOOM.EXE");
    EXPECT_EQ(doom->launch().working_dir, "GOLD/DOOM");
    EXPECT_EQ(doom->screenshots().title, "title.png");
    EXPECT_TRUE(doom->isLaunchable());

    ASSERT_EQ(doom->sources().size(), 2u);
    EXPECT_EQ(doom->sources()[0].role, "primary");
    EXPECT_EQ(doom->sources()[0].install_type, InstallType::FloppyInstall);
    EXPECT_EQ(doom->sources()[1].role, "mirror");

    ASSERT_EQ(doom->install().expected_files.size(), 2u);
    EXPECT_EQ(doom->install().expected_files[0].path, "GOLD/DOOM/DOOM.EXE");
    EXPECT_EQ(doom->install().expected_files[0].size, 687001u);
}

TEST(GameDefinition, parses_a_minimal_definition)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(minimalToml(), error);

    ASSERT_TRUE(game.has_value()) << error;
    EXPECT_EQ(game->slug(), "doom");
    EXPECT_EQ(game->sources().size(), 1u);
}

TEST(GameDefinition, reads_an_optional_source_size)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(
            withReplacement("url = \"https://example.org/doom.7z\"",
                            "url = \"https://example.org/doom.7z\"\nsize = 2500000"),
            error);

    ASSERT_TRUE(game.has_value()) << error;
    EXPECT_EQ(game->sources()[0].size, 2500000u);

    const auto without = GameDefinition::fromTomlString(minimalToml(), error);
    ASSERT_TRUE(without.has_value()) << error;
    EXPECT_FALSE(without->sources()[0].size.has_value());
}

TEST(GameDefinition, rejects_a_negative_source_size)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(
            withReplacement("url = \"https://example.org/doom.7z\"",
                            "url = \"https://example.org/doom.7z\"\nsize = -5"),
            error);

    EXPECT_FALSE(game.has_value());
    EXPECT_NE(error.find("size"), std::string::npos);
}

TEST(GameDefinition, accepts_a_definition_without_expected_files)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(minimalToml(), error);

    ASSERT_TRUE(game.has_value()) << error;
    EXPECT_TRUE(game->install().expected_files.empty());
}

TEST(GameDefinition, reads_expected_files_with_optional_fields)
{
    const std::string extra = R"(
[install.expected_files]
"GOLD/DOOM/DOOM.EXE"  = { size = 687001 }
"GOLD/DOOM/DOOM1.WAD" = { size = 4234124, sha256 = "abc123" }
"GOLD/DOOM/DOOM.DOC"  = {}
)";
    std::string error;
    const auto game = GameDefinition::fromTomlString(minimalToml(extra), error);

    ASSERT_TRUE(game.has_value()) << error;
    ASSERT_EQ(game->install().expected_files.size(), 3u);
    // Order is by path so the check is reproducible run to run.
    const auto& files = game->install().expected_files;
    EXPECT_EQ(files[0].path, "GOLD/DOOM/DOOM.DOC");
    EXPECT_FALSE(files[0].size.has_value());
    EXPECT_EQ(files[1].path, "GOLD/DOOM/DOOM.EXE");
    EXPECT_EQ(files[1].size, 687001u);
    EXPECT_FALSE(files[1].sha256.has_value());
    EXPECT_EQ(files[2].sha256, "abc123");
}

TEST(GameDefinition, rejects_a_definition_with_no_sources)
{
    std::string toml = minimalToml();
    const auto start = toml.find("[sources.primary]");
    const auto end = toml.find("[dosbox]");
    ASSERT_NE(start, std::string::npos);
    toml.erase(start, end - start);

    std::string error;
    EXPECT_FALSE(GameDefinition::fromTomlString(toml, error).has_value());
    EXPECT_NE(error.find("source"), std::string::npos) << error;
}

TEST(GameDefinition, rejects_an_unknown_license)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(
            withReplacement(R"(license = "shareware")", R"(license = "warez")"),
            error);

    EXPECT_FALSE(game.has_value());
    EXPECT_NE(error.find("license"), std::string::npos) << error;
}

TEST(GameDefinition, rejects_an_unknown_install_type)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(
            withReplacement(R"(install_type = "floppyinstall")",
                            R"(install_type = "telepathy")"),
            error);

    EXPECT_FALSE(game.has_value());
    EXPECT_NE(error.find("install type"), std::string::npos) << error;
}

// The slug names a directory under the cache base and reaches the mount
// lines, so anything that could climb out of it must never parse.
class HostileSlug : public testing::TestWithParam<const char*> {};

TEST_P(HostileSlug, is_rejected)
{
    const std::string slug = GetParam();
    std::string error;
    const auto game = GameDefinition::fromTomlString(
            withReplacement(R"(slug = "doom")", "slug = \"" + tomlEscaped(slug) + "\""),
            error);

    EXPECT_FALSE(game.has_value()) << "accepted hostile slug: " << slug;
    EXPECT_NE(error.find("slug"), std::string::npos) << error;
}

INSTANTIATE_TEST_SUITE_P(Traversal, HostileSlug,
                         testing::Values("../etc", "..", "a/b", "a\\b", "/absolute",
                                         "C:doom", "", ".", "doom ", " doom", "doom.toml",
                                         "-rf"));

// The executable is appended to the install directory and typed into an
// autoexec line: a bare filename is the only safe shape.
class HostileExecutable : public testing::TestWithParam<const char*> {};

TEST_P(HostileExecutable, is_rejected)
{
    const std::string exe = GetParam();
    std::string error;
    const auto game = GameDefinition::fromTomlString(
            withReplacement(R"(executable = "DOOM.EXE")",
                            "executable = \"" + tomlEscaped(exe) + "\""),
            error);

    EXPECT_FALSE(game.has_value()) << "accepted hostile executable: " << exe;
    EXPECT_NE(error.find("executable"), std::string::npos) << error;
}

INSTANTIATE_TEST_SUITE_P(Traversal, HostileExecutable,
                         testing::Values("/bin/sh", "../../bin/sh", "sub/DOOM.EXE",
                                         "sub\\DOOM.EXE",
                                         "C:\\WINDOWS\\SYSTEM32\\CMD.EXE", ".."));

TEST(GameDefinition, accepts_an_empty_executable_as_no_recipe_yet)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(
            withReplacement(R"(executable = "DOOM.EXE")", R"(executable = "")"),
            error);

    ASSERT_TRUE(game.has_value()) << error;
    EXPECT_FALSE(game->isLaunchable());
}

class HostileWorkingDir : public testing::TestWithParam<const char*> {};

TEST_P(HostileWorkingDir, is_rejected)
{
    const std::string dir = GetParam();
    std::string error;
    const auto game = GameDefinition::fromTomlString(
            withReplacement(R"(working_dir = "")",
                            "working_dir = \"" + tomlEscaped(dir) + "\""),
            error);

    EXPECT_FALSE(game.has_value()) << "accepted hostile working_dir: " << dir;
    EXPECT_NE(error.find("working_dir"), std::string::npos) << error;
}

INSTANTIATE_TEST_SUITE_P(Traversal, HostileWorkingDir,
                         testing::Values("..", "../..", "GOLD/../../etc", "/etc", "C:\\",
                                         "GOLD/..", "..\\GOLD"));

TEST(GameDefinition, rejects_an_expected_file_that_escapes_the_install_directory)
{
    const std::string extra = R"(
[install.expected_files]
"../../../etc/passwd" = { size = 1 }
)";
    std::string error;
    const auto game = GameDefinition::fromTomlString(minimalToml(extra), error);

    EXPECT_FALSE(game.has_value());
    EXPECT_NE(error.find("expected file"), std::string::npos) << error;
}

TEST(GameDefinition, rejects_a_missing_required_section)
{
    std::string toml = minimalToml();
    const auto pos = toml.find("[launch]");
    ASSERT_NE(pos, std::string::npos);
    toml.erase(pos, toml.find("[screenshots]") - pos);

    std::string error;
    EXPECT_FALSE(GameDefinition::fromTomlString(toml, error).has_value());
    EXPECT_FALSE(error.empty());
}

TEST(GameDefinition, rejects_a_wrong_type_for_a_field)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(withReplacement("rank = 1",
                                                                     R"(rank = "first")"),
                                                     error);

    EXPECT_FALSE(game.has_value());
    EXPECT_NE(error.find("rank"), std::string::npos) << error;
}

TEST(GameDefinition, rejects_a_negative_cycle_count)
{
    std::string error;
    const auto game = GameDefinition::fromTomlString(withReplacement("cpu_cycles = 12000",
                                                                     "cpu_cycles = -1"),
                                                     error);

    EXPECT_FALSE(game.has_value());
    EXPECT_NE(error.find("cpu_cycles"), std::string::npos) << error;
}

TEST(GameDefinition, rejects_malformed_toml_without_crashing)
{
    std::string error;
    EXPECT_FALSE(GameDefinition::fromTomlString("slug = \"doom", error).has_value());
    EXPECT_FALSE(error.empty());

    EXPECT_FALSE(GameDefinition::fromTomlString("", error).has_value());
    EXPECT_FALSE(GameDefinition::fromTomlString("[[[[", error).has_value());
    EXPECT_FALSE(
            GameDefinition::fromTomlString(std::string(4096, '['), error).has_value());
}

TEST(GameDefinition, rejects_a_file_that_does_not_exist)
{
    std::string error;
    const auto game = GameDefinition::fromToml("assets/games/nosuchgame/nosuchgame.toml",
                                               error);

    EXPECT_FALSE(game.has_value());
    EXPECT_FALSE(error.empty());
}

TEST(GameDefinition, rejects_a_slug_that_disagrees_with_its_filename)
{
    // The file name is what the catalogue scans; a definition claiming a
    // different slug would resolve its cache directory somewhere else.
    std::string error;
    const auto game = GameDefinition::fromToml(kDoomToml, error);
    ASSERT_TRUE(game.has_value()) << error;
    EXPECT_EQ(game->slug(), kDoomToml.stem().string());
}

TEST(SafePathComponent, accepts_plain_names_and_rejects_everything_else)
{
    EXPECT_TRUE(showroom::isSafePathComponent("DOOM.EXE"));
    EXPECT_TRUE(showroom::isSafePathComponent("doom"));
    EXPECT_TRUE(showroom::isSafePathComponent("keen4-ep"));

    EXPECT_FALSE(showroom::isSafePathComponent(""));
    EXPECT_FALSE(showroom::isSafePathComponent("."));
    EXPECT_FALSE(showroom::isSafePathComponent(".."));
    EXPECT_FALSE(showroom::isSafePathComponent("a/b"));
    EXPECT_FALSE(showroom::isSafePathComponent("a\\b"));
    EXPECT_FALSE(showroom::isSafePathComponent(std::string_view("a\0b", 3)));
    EXPECT_FALSE(showroom::isSafePathComponent("a b"));
    EXPECT_FALSE(showroom::isSafePathComponent("a;b"));
}

TEST(SafeRelativePath, allows_empty_and_multiple_segments)
{
    EXPECT_TRUE(showroom::isSafeRelativePath(""));
    EXPECT_TRUE(showroom::isSafeRelativePath("GOLD"));
    EXPECT_TRUE(showroom::isSafeRelativePath("GOLD/DOOM"));

    EXPECT_FALSE(showroom::isSafeRelativePath("/GOLD"));
    EXPECT_FALSE(showroom::isSafeRelativePath("GOLD/"));
    EXPECT_FALSE(showroom::isSafeRelativePath("GOLD//DOOM"));
    EXPECT_FALSE(showroom::isSafeRelativePath("GOLD/../DOOM"));
    EXPECT_FALSE(showroom::isSafeRelativePath("GOLD\\DOOM"));
}

} // namespace
