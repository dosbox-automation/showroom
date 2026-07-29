// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/file_verifier.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace showroom {
namespace {

// sha256 of the literal bytes "showroom".
constexpr const char* kShowroomHash =
        "634e92fa7130dd801e0355d682155b86c9e6be7f6c0085ce0902ff4b6e13e8b9";

// sha256 of 200000 'a' bytes, several read chunks worth.
constexpr const char* kLongHash =
        "2287d207f24a941ff3b56c04c8a25ad56b63e3023207b3bb5b4ac0c9869d74be";

class FileVerifierDir : public testing::Test {
protected:
    void SetUp() override
    {
        const auto* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path()
             / ("showroom-verify-"
                + std::to_string(testing::UnitTest::GetInstance()->random_seed()) + "-"
                + info->name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
        std::filesystem::permissions(dir_,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    }

    void TearDown() override { std::filesystem::remove_all(dir_); }

    std::filesystem::path write(const std::string& name, const std::string& content)
    {
        const auto path = dir_ / name;
        std::ofstream out(path, std::ios::binary);
        out << content;
        return path;
    }

    std::filesystem::path dir_;
};

TEST_F(FileVerifierDir, an_intact_file_verifies)
{
    const auto file = write("archive.zip", "showroom");

    const FileCheck check = FileVerifier::verify(file, 8, kShowroomHash);

    EXPECT_EQ(check.verdict, FileVerdict::Ok);
    EXPECT_TRUE(check.detail.empty());
}

TEST_F(FileVerifierDir, a_size_match_with_a_hash_mismatch_fails)
{
    const auto file = write("archive.zip", "shOwroom");

    const FileCheck check = FileVerifier::verify(file, 8, kShowroomHash);

    EXPECT_EQ(check.verdict, FileVerdict::WrongHash);
    EXPECT_FALSE(check.detail.empty());
}

TEST_F(FileVerifierDir, a_missing_file_fails)
{
    const FileCheck check = FileVerifier::verify(dir_ / "nowhere.zip", 8, kShowroomHash);

    EXPECT_EQ(check.verdict, FileVerdict::Missing);
}

TEST_F(FileVerifierDir, a_wrong_size_names_both_numbers)
{
    const auto file = write("archive.zip", "showroom");

    const FileCheck check = FileVerifier::verify(file, 9, kShowroomHash);

    EXPECT_EQ(check.verdict, FileVerdict::WrongSize);
    EXPECT_NE(check.detail.find("8"), std::string::npos);
    EXPECT_NE(check.detail.find("9"), std::string::npos);
}

TEST_F(FileVerifierDir, hash_comparison_ignores_case)
{
    const auto file = write("archive.zip", "showroom");
    std::string upper = kShowroomHash;
    for (char& c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    EXPECT_EQ(FileVerifier::verify(file, 8, upper).verdict, FileVerdict::Ok);
}

TEST_F(FileVerifierDir, a_file_spanning_many_chunks_hashes_correctly)
{
    const auto file = write("big.bin", std::string(200000, 'a'));

    EXPECT_EQ(FileVerifier::verify(file, 200000, kLongHash).verdict, FileVerdict::Ok);
}

TEST_F(FileVerifierDir, an_empty_expected_hash_checks_size_only)
{
    const auto file = write("archive.zip", "showroom");

    EXPECT_EQ(FileVerifier::verify(file, 8, "").verdict, FileVerdict::Ok);
    EXPECT_EQ(FileVerifier::verify(file, 9, "").verdict, FileVerdict::WrongSize);
}

TEST_F(FileVerifierDir, no_expected_size_checks_existence_and_hash_only)
{
    const auto file = write("archive.zip", "showroom");

    EXPECT_EQ(FileVerifier::verify(file, std::nullopt, kShowroomHash).verdict,
              FileVerdict::Ok);
}

TEST_F(FileVerifierDir, a_directory_where_a_file_is_expected_is_missing)
{
    std::filesystem::create_directories(dir_ / "archive.zip");

    EXPECT_EQ(FileVerifier::verify(dir_ / "archive.zip", 8, kShowroomHash).verdict,
              FileVerdict::Missing);
}

TEST_F(FileVerifierDir, a_malformed_expected_hash_never_verifies)
{
    const auto file = write("archive.zip", "showroom");

    EXPECT_EQ(FileVerifier::verify(file, 8, "not-hex").verdict, FileVerdict::WrongHash);
    EXPECT_EQ(FileVerifier::verify(file, 8, std::string(63, 'a')).verdict,
              FileVerdict::WrongHash);
}

} // namespace
} // namespace showroom
