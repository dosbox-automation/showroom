// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/archive_extractor.h"

#include <archive.h>
#include <archive_entry.h>

#include <memory>
#include <system_error>

namespace showroom {
namespace {

constexpr std::size_t kReadBlockBytes = 64 * 1024;

// ARCHIVE_EXTRACT_TIME preserves modification times (house rule).
// No ARCHIVE_EXTRACT_PERM: archive-supplied permissions from the wild
// are not restored, the process umask decides.
// SECURE_NOABSOLUTEPATHS and SECURE_NODOTDOT are unusable here: entry
// pathnames are rewritten to absolute destination paths before writing,
// so those flags would judge our own path, not the archive's.
// entryNameIsSafe covers what they would have, on the raw entry name.
constexpr int kExtractFlags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_SECURE_SYMLINKS;

struct ArchiveCloser {
    void operator()(struct archive* a) const { archive_read_free(a); }
};

using ArchivePtr = std::unique_ptr<struct archive, ArchiveCloser>;

std::string archiveError(struct archive* a, const std::string& fallback)
{
    const char* msg = archive_error_string(a);
    return msg != nullptr ? msg : fallback;
}

// The SECURE_* flags guard the write side; this guards before any write
// happens, so a hostile name fails the archive instead of being partially
// processed.
bool entryNameIsSafe(const std::filesystem::path& name)
{
    if (name.empty() || name.is_absolute()) {
        return false;
    }
    for (const auto& component : name) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

} // namespace

ExtractResult ArchiveExtractor::extract(const std::filesystem::path& archive,
                                        const std::filesystem::path& destination) const
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(archive, ec)) {
        return {false, archive.string() + ": missing"};
    }

    std::filesystem::create_directories(destination, ec);
    if (ec) {
        return {false, destination.string() + ": " + ec.message()};
    }

    ArchivePtr reader(archive_read_new());
    if (!reader) {
        return {false, "libarchive: allocation failed"};
    }

    // The three formats game archives ship in. Filters cover compressed
    // tar variants (tar.gz and friends).
    archive_read_support_format_7zip(reader.get());
    archive_read_support_format_zip(reader.get());
    archive_read_support_format_tar(reader.get());
    archive_read_support_filter_all(reader.get());

    if (archive_read_open_filename(reader.get(), archive.string().c_str(), kReadBlockBytes)
        != ARCHIVE_OK) {
        return {false,
                archive.string() + ": " + archiveError(reader.get(), "unreadable")};
    }

    for (;;) {
        struct archive_entry* entry = nullptr;
        const int header = archive_read_next_header(reader.get(), &entry);
        if (header == ARCHIVE_EOF) {
            break;
        }
        if (header < ARCHIVE_WARN) {
            return {false,
                    archive.string() + ": " + archiveError(reader.get(), "corrupt")};
        }

        const char* raw_name = archive_entry_pathname(entry);
        const std::filesystem::path name = raw_name != nullptr ? raw_name : "";
        if (!entryNameIsSafe(name)) {
            return {false,
                    archive.string() + ": unsafe entry name '" + name.string() + "'"};
        }

        // Link entries carry targets that point wherever they like; game
        // archives have no legitimate use for them.
        const auto type = archive_entry_filetype(entry);
        if (type == AE_IFLNK || archive_entry_hardlink(entry) != nullptr) {
            return {false,
                    archive.string() + ": link entry '" + name.string() + "' refused"};
        }

        const auto target = destination / name;
        archive_entry_set_pathname(entry, target.string().c_str());

        // A warn here means the file was skipped or written incomplete;
        // that must fail the archive, not pass as a lesser success.
        const int extracted = archive_read_extract(reader.get(), entry, kExtractFlags);
        if (extracted != ARCHIVE_OK) {
            return {false,
                    name.string() + ": " + archiveError(reader.get(), "extract failed")};
        }
    }

    return {true, {}};
}

} // namespace showroom
