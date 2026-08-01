// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/archive_extractor.h"

#include <archive.h>
#include <archive_entry.h>

#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace showroom {
namespace {

namespace fs = std::filesystem;

// 1994-12-10T00:00:00Z. An old, even timestamp: zip's DOS time field
// has 2-second granularity, so odd seconds would round.
constexpr std::time_t kDoomEpoch = 787017600;

enum class Format { Zip, Tar, SevenZip };

struct FixtureEntry {
    std::string name;
    std::string content;
    std::time_t mtime = kDoomEpoch;
    bool symlink = false;
};

void writeArchive(const fs::path& out, Format format,
                  const std::vector<FixtureEntry>& entries)
{
    struct archive* writer = archive_write_new();
    ASSERT_NE(writer, nullptr);

    switch (format) {
    case Format::Zip: archive_write_set_format_zip(writer); break;
    case Format::Tar: archive_write_set_format_pax_restricted(writer); break;
    case Format::SevenZip: archive_write_set_format_7zip(writer); break;
    }
    ASSERT_EQ(archive_write_open_filename(writer, out.string().c_str()), ARCHIVE_OK);

    for (const auto& fixture : entries) {
        struct archive_entry* entry = archive_entry_new();
        archive_entry_set_pathname(entry, fixture.name.c_str());
        archive_entry_set_mtime(entry, fixture.mtime, 0);
        archive_entry_set_perm(entry, 0644);
        if (fixture.symlink) {
            archive_entry_set_filetype(entry, AE_IFLNK);
            archive_entry_set_symlink(entry, fixture.content.c_str());
        } else {
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_size(entry,
                                   static_cast<la_int64_t>(fixture.content.size()));
        }
        ASSERT_EQ(archive_write_header(writer, entry), ARCHIVE_OK);
        if (!fixture.symlink && !fixture.content.empty()) {
            archive_write_data(writer, fixture.content.data(), fixture.content.size());
        }
        archive_entry_free(entry);
    }

    archive_write_close(writer);
    archive_write_free(writer);
}

std::string slurp(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

class ArchiveExtractorDir : public testing::Test {
protected:
    void SetUp() override
    {
        const auto* info = testing::UnitTest::GetInstance()->current_test_info();
        // House rule: scratch lives in the project .workspace, not /tmp.
        dir_ = fs::path(SHOWROOM_TEST_WORKSPACE)
             / ("extract-"
                + std::to_string(testing::UnitTest::GetInstance()->random_seed()) + "-"
                + info->name());
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        fs::permissions(dir_, fs::perms::owner_all, fs::perm_options::replace);
    }

    void TearDown() override { fs::remove_all(dir_); }

    fs::path dir_;
    ArchiveExtractor extractor_;
};

TEST_F(ArchiveExtractorDir, a_zip_extracts_files_and_subdirs)
{
    const auto zip = dir_ / "game.zip";
    writeArchive(zip,
                 Format::Zip,
                 {{"disk1.ima", "first disk"}, {"docs/readme.txt", "read me"}});

    const auto result = extractor_.extract(zip, dir_ / "out");

    EXPECT_TRUE(result.ok) << result.error;
    EXPECT_EQ(slurp(dir_ / "out" / "disk1.ima"), "first disk");
    EXPECT_EQ(slurp(dir_ / "out" / "docs" / "readme.txt"), "read me");
}

TEST_F(ArchiveExtractorDir, a_tar_extracts)
{
    const auto tar = dir_ / "game.tar";
    writeArchive(tar, Format::Tar, {{"setup.exe", "installer"}});

    const auto result = extractor_.extract(tar, dir_ / "out");

    EXPECT_TRUE(result.ok) << result.error;
    EXPECT_EQ(slurp(dir_ / "out" / "setup.exe"), "installer");
}

TEST_F(ArchiveExtractorDir, a_7z_extracts)
{
    const auto sevenzip = dir_ / "game.7z";
    writeArchive(sevenzip, Format::SevenZip, {{"disk2.ima", "second disk"}});

    const auto result = extractor_.extract(sevenzip, dir_ / "out");

    EXPECT_TRUE(result.ok) << result.error;
    EXPECT_EQ(slurp(dir_ / "out" / "disk2.ima"), "second disk");
}

TEST_F(ArchiveExtractorDir, extraction_preserves_timestamps)
{
    const auto tar = dir_ / "game.tar";
    writeArchive(tar, Format::Tar, {{"disk1.ima", "payload", kDoomEpoch}});

    const auto result = extractor_.extract(tar, dir_ / "out");
    ASSERT_TRUE(result.ok) << result.error;

    const auto written = fs::last_write_time(dir_ / "out" / "disk1.ima");
    const auto expected = std::chrono::clock_cast<std::chrono::file_clock>(
            std::chrono::sys_seconds(std::chrono::seconds(kDoomEpoch)));
    EXPECT_EQ(written, expected);
}

TEST_F(ArchiveExtractorDir, a_missing_archive_fails)
{
    const auto result = extractor_.extract(dir_ / "nope.zip", dir_ / "out");

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("missing"), std::string::npos) << result.error;
}

TEST_F(ArchiveExtractorDir, a_corrupt_archive_fails)
{
    const auto junk = dir_ / "junk.zip";
    std::ofstream(junk, std::ios::binary) << "this is not an archive at all";

    const auto result = extractor_.extract(junk, dir_ / "out");

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
}

TEST_F(ArchiveExtractorDir, a_traversal_entry_is_rejected)
{
    const auto tar = dir_ / "hostile.tar";
    writeArchive(tar, Format::Tar, {{"../escape.txt", "outside"}});

    const auto result = extractor_.extract(tar, dir_ / "out");

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("unsafe entry name"), std::string::npos) << result.error;
    EXPECT_FALSE(fs::exists(dir_ / "escape.txt"));
}

TEST_F(ArchiveExtractorDir, an_absolute_entry_is_rejected)
{
    const auto tar = dir_ / "hostile.tar";
    writeArchive(tar, Format::Tar, {{"/abs/evil.txt", "outside"}});

    const auto result = extractor_.extract(tar, dir_ / "out");

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("unsafe entry name"), std::string::npos) << result.error;
}

TEST_F(ArchiveExtractorDir, a_symlink_entry_is_rejected)
{
    const auto tar = dir_ / "hostile.tar";
    FixtureEntry link;
    link.name = "innocent.txt";
    link.content = "/etc/passwd";
    link.symlink = true;
    writeArchive(tar, Format::Tar, {link});

    const auto result = extractor_.extract(tar, dir_ / "out");

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("link entry"), std::string::npos) << result.error;
    EXPECT_FALSE(fs::exists(dir_ / "out" / "innocent.txt"));
}

TEST_F(ArchiveExtractorDir, destination_is_created_if_missing)
{
    const auto zip = dir_ / "game.zip";
    writeArchive(zip, Format::Zip, {{"disk1.ima", "payload"}});

    const auto result = extractor_.extract(zip, dir_ / "deep" / "nested" / "out");

    EXPECT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(fs::exists(dir_ / "deep" / "nested" / "out" / "disk1.ima"));
}

} // namespace
} // namespace showroom
