// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_NET_ARCHIVE_EXTRACTOR_H
#define SHOWROOM_NET_ARCHIVE_EXTRACTOR_H

#include <filesystem>
#include <string>

namespace showroom {

struct ExtractResult {
    bool ok = false;
    std::string error;
};

// Unpacks a downloaded archive (7z, zip, tar) into a destination
// directory. Entry names are validated before extraction: absolute
// paths, traversal components, and link entries all fail the whole
// archive. On failure, whatever was already extracted stays on disk;
// the caller owns the directory and its cleanup.
class ArchiveExtractor {
public:
    virtual ~ArchiveExtractor() = default;

    virtual ExtractResult extract(const std::filesystem::path& archive,
                                  const std::filesystem::path& destination) const;
};

} // namespace showroom

#endif // SHOWROOM_NET_ARCHIVE_EXTRACTOR_H
