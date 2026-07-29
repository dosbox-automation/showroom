// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/file_verifier.h"

#include <QCryptographicHash>
#include <QFile>

#include <algorithm>
#include <cctype>
#include <system_error>

namespace showroom {
namespace {

// 64 KiB keeps memory flat on multi-hundred-MB archives.
constexpr qint64 kReadChunkBytes = 64 * 1024;

bool isSha256Hex(const std::string& hex)
{
    return hex.size() == 64 && std::all_of(hex.begin(), hex.end(), [](unsigned char c) {
               return std::isxdigit(c) != 0;
           });
}

std::string lowered(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

} // namespace

FileCheck FileVerifier::verify(const std::filesystem::path& file,
                               std::optional<std::uint64_t> expected_size,
                               const std::string& expected_sha256_hex)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) {
        return {FileVerdict::Missing, file.string() + ": missing"};
    }

    if (expected_size.has_value()) {
        const auto actual = std::filesystem::file_size(file, ec);
        if (ec) {
            return {FileVerdict::Unreadable, file.string() + ": " + ec.message()};
        }
        if (actual != *expected_size) {
            return {FileVerdict::WrongSize,
                    file.string() + ": " + std::to_string(actual) + " bytes, expected "
                            + std::to_string(*expected_size)};
        }
    }

    if (expected_sha256_hex.empty()) {
        return {};
    }
    // A malformed expectation must fail the file, not wave it through: a
    // typo in a definition would otherwise disable its verification.
    if (!isSha256Hex(expected_sha256_hex)) {
        return {FileVerdict::WrongHash, file.string() + ": malformed expected sha256"};
    }

    QFile input(QString::fromStdString(file.string()));
    if (!input.open(QIODevice::ReadOnly)) {
        return {FileVerdict::Unreadable,
                file.string() + ": " + input.errorString().toStdString()};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!input.atEnd()) {
        const QByteArray chunk = input.read(kReadChunkBytes);
        if (chunk.isEmpty() && input.error() != QFileDevice::NoError) {
            return {FileVerdict::Unreadable,
                    file.string() + ": " + input.errorString().toStdString()};
        }
        hash.addData(chunk);
    }

    const std::string actual_hex = hash.result().toHex().toStdString();
    if (actual_hex != lowered(expected_sha256_hex)) {
        return {FileVerdict::WrongHash,
                file.string() + ": sha256 " + actual_hex + ", expected "
                        + lowered(expected_sha256_hex)};
    }
    return {};
}

} // namespace showroom
