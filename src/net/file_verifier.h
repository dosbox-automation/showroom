// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_NET_FILE_VERIFIER_H
#define SHOWROOM_NET_FILE_VERIFIER_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace showroom {

enum class FileVerdict {
    Ok,
    Missing,
    WrongSize,
    WrongHash,
    // Exists and is the right size, but could not be read to the end.
    Unreadable,
};

struct FileCheck {
    FileVerdict verdict = FileVerdict::Ok;
    std::string detail;
};

class FileVerifier {
public:
    // Streams the hash in chunks; a download never fits in memory as a
    // rule. An empty expected hash skips hashing, a null size skips the
    // size check; existence is always required.
    static FileCheck verify(const std::filesystem::path& file,
                            std::optional<std::uint64_t> expected_size,
                            const std::string& expected_sha256_hex);
};

} // namespace showroom

#endif // SHOWROOM_NET_FILE_VERIFIER_H
