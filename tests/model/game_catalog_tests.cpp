// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/game_catalog.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using showroom::GameCatalog;

// The bundled assets, relative to the working directory ctest runs in.
const std::filesystem::path kAssetGames = "assets/games";

constexpr std::size_t kBundledGameCount = 16;

std::string definition_toml(const std::string& slug, const std::string& title)
{
    return "slug = \"" + slug + "\"\n"
           "title = \"" + title + "\"\n"
           "rank = 1\n"
           "license = \"freeware\"\n"
           "\n[sources.primary]\n"
           "url = \"https://example.org/game.zip\"\n"
           "\n[dosbox]\n"
           "machine = \"svga_s3\"\n"
           "cpu_cycles = 12000\n"
           "cpu_cycles_protected = 12000\n"
           "\n[launch]\n"
           "executable = \"GAME.EXE\"\n"
           "\n[install]\n"
           "max_runtime_seconds = 60\n";
}

void write_game(const std::filesystem::path& games_dir, const std::string& slug,
                const std::string& title)
{
    std::filesystem::create_directories(games_dir / slug);
    std::ofstream out(games_dir / slug / (slug + ".toml"));
    out << definition_toml(slug, title);
}

void write_raw(const std::filesystem::path& games_dir, const std::string& slug,
               const std::string& contents)
{
    std::filesystem::create_directories(games_dir / slug);
    std::ofstream out(games_dir / slug / (slug + ".toml"));
    out << contents;
}

// A fresh directory per test, never the source tree.
class CatalogDir : public testing::Test {
protected:
    void SetUp() override
    {
        dir_ = std::filesystem::temp_directory_path()
               / ("showroom-catalog-" + std::to_string(::testing::UnitTest::GetInstance()
                                                           ->random_seed())
                  + "-" + test_name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    static std::string test_name()
    {
        return ::testing::UnitTest::GetInstance()->current_test_info()->name();
    }

    std::filesystem::path dir_;
};

TEST(GameCatalogAssets, loads_every_bundled_game)
{
    ASSERT_TRUE(std::filesystem::is_directory(kAssetGames))
        << "run ctest from the source directory; looked for "
        << std::filesystem::absolute(kAssetGames);

    const auto catalog = GameCatalog::loadFromDirectory(kAssetGames);

    for (const auto& error : catalog.errors()) {
        ADD_FAILURE() << "bundled definition rejected: " << error.path << ": "
                      << error.message;
    }
    EXPECT_EQ(catalog.size(), kBundledGameCount);
    EXPECT_NE(catalog.find("doom"), nullptr);
}

TEST(GameCatalogAssets, orders_bundled_games_by_title)
{
    const auto catalog = GameCatalog::loadFromDirectory(kAssetGames);
    ASSERT_FALSE(catalog.empty());

    std::vector<std::string> titles;
    for (const auto& game : catalog) {
        titles.push_back(game.title());
    }

    // The contract restated independently of the implementation's
    // comparator: case-insensitive, ascending.
    auto sorted = titles;
    std::sort(sorted.begin(), sorted.end(), [](std::string a, std::string b) {
        std::transform(a.begin(), a.end(), a.begin(), ::tolower);
        std::transform(b.begin(), b.end(), b.begin(), ::tolower);
        return a < b;
    });
    EXPECT_EQ(titles, sorted);
}

TEST(GameCatalogAssets, every_bundled_game_has_its_screenshots_on_disk)
{
    const auto catalog = GameCatalog::loadFromDirectory(kAssetGames);
    ASSERT_FALSE(catalog.empty());

    for (const auto& game : catalog) {
        const auto dir = kAssetGames / game.slug();
        EXPECT_FALSE(game.screenshots().title.empty()) << game.slug();
        EXPECT_TRUE(std::filesystem::exists(dir / game.screenshots().title))
            << game.slug() << " claims a title screenshot that is not there";
        EXPECT_TRUE(std::filesystem::exists(dir / game.screenshots().gameplay))
            << game.slug() << " claims a gameplay screenshot that is not there";
    }
}

TEST_F(CatalogDir, empty_directory_yields_an_empty_catalog)
{
    const auto catalog = GameCatalog::loadFromDirectory(dir_);

    EXPECT_TRUE(catalog.empty());
    EXPECT_TRUE(catalog.errors().empty());
}

TEST_F(CatalogDir, missing_directory_reports_an_error_instead_of_throwing)
{
    const auto catalog = GameCatalog::loadFromDirectory(dir_ / "nosuchdir");

    EXPECT_TRUE(catalog.empty());
    EXPECT_EQ(catalog.errors().size(), 1u);
}

TEST_F(CatalogDir, one_corrupt_definition_does_not_blank_the_rest)
{
    write_game(dir_, "doom", "DOOM");
    write_raw(dir_, "broken", "slug = \"broken\"\nthis is not toml [[[");
    write_game(dir_, "tyrian", "Tyrian");

    const auto catalog = GameCatalog::loadFromDirectory(dir_);

    EXPECT_EQ(catalog.size(), 2u);
    EXPECT_NE(catalog.find("doom"), nullptr);
    EXPECT_NE(catalog.find("tyrian"), nullptr);
    EXPECT_EQ(catalog.find("broken"), nullptr);
    ASSERT_EQ(catalog.errors().size(), 1u);
    EXPECT_NE(catalog.errors()[0].message.find("malformed"), std::string::npos)
        << catalog.errors()[0].message;
}

TEST_F(CatalogDir, a_definition_whose_slug_fights_its_directory_is_rejected)
{
    // The catalogue resolves cache directories from the slug; letting a
    // file in doom/ claim to be another game would cross those paths.
    write_raw(dir_, "doom", definition_toml("tyrian", "Tyrian"));

    const auto catalog = GameCatalog::loadFromDirectory(dir_);

    EXPECT_TRUE(catalog.empty());
    ASSERT_EQ(catalog.errors().size(), 1u);
    EXPECT_NE(catalog.errors()[0].message.find("does not match file name"),
              std::string::npos)
        << catalog.errors()[0].message;
}

TEST_F(CatalogDir, sorts_titles_ignoring_case)
{
    write_game(dir_, "cherry", "cherry");
    write_game(dir_, "apple", "Apple");
    write_game(dir_, "banana", "BANANA");

    const auto catalog = GameCatalog::loadFromDirectory(dir_);

    ASSERT_EQ(catalog.size(), 3u);
    EXPECT_EQ(catalog.at(0).title(), "Apple");
    EXPECT_EQ(catalog.at(1).title(), "BANANA");
    EXPECT_EQ(catalog.at(2).title(), "cherry");
}

TEST_F(CatalogDir, a_directory_without_a_definition_is_skipped_quietly)
{
    write_game(dir_, "doom", "DOOM");
    std::filesystem::create_directories(dir_ / "screenshots-only");

    const auto catalog = GameCatalog::loadFromDirectory(dir_);

    EXPECT_EQ(catalog.size(), 1u);
    EXPECT_TRUE(catalog.errors().empty());
}

TEST_F(CatalogDir, loose_files_beside_the_game_directories_are_ignored)
{
    write_game(dir_, "doom", "DOOM");
    std::ofstream(dir_ / "index.json") << "[]";

    const auto catalog = GameCatalog::loadFromDirectory(dir_);

    EXPECT_EQ(catalog.size(), 1u);
    EXPECT_TRUE(catalog.errors().empty());
}

TEST_F(CatalogDir, at_rejects_an_out_of_range_index)
{
    write_game(dir_, "doom", "DOOM");
    const auto catalog = GameCatalog::loadFromDirectory(dir_);

    EXPECT_THROW((void)catalog.at(1), std::out_of_range);
}

}  // namespace
