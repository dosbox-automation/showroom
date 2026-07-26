// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "app/paths.h"

#include "app/logging.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QString>

#include <cstdlib>
#include <system_error>

namespace showroom {
namespace {

constexpr const char* kLogComponent = "paths";

std::filesystem::path fromQString(const QString& value)
{
    return std::filesystem::path(value.toStdString());
}

bool isReadableDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    return !path.empty() && std::filesystem::is_directory(path, error);
}

} // namespace

std::filesystem::path Paths::assetsDir()
{
    if (const char* override_dir = std::getenv("SHOWROOM_ASSETS_DIR")) {
        const std::filesystem::path candidate(override_dir);
        if (isReadableDirectory(candidate)) {
            return candidate;
        }
        log_warn(kLogComponent,
                 "SHOWROOM_ASSETS_DIR is not a directory: %s",
                 override_dir);
    }

    const std::filesystem::path beside_binary =
            fromQString(QCoreApplication::applicationDirPath()) / "assets";
    if (isReadableDirectory(beside_binary)) {
        return beside_binary;
    }

    const std::filesystem::path in_source_tree(SHOWROOM_SOURCE_ASSETS_DIR);
    if (isReadableDirectory(in_source_tree)) {
        return in_source_tree;
    }

    // Returned anyway: the catalogue reports an unreadable directory as
    // a load error the user can see, which beats an empty grid with no
    // explanation.
    log_error(kLogComponent,
              "no assets directory found, falling back to %s",
              beside_binary.string().c_str());
    return beside_binary;
}

std::filesystem::path Paths::cacheDir()
{
#ifdef Q_OS_WIN
    // Portable by design: no registry, no AppData, everything the
    // showroom writes stays inside the folder it was unpacked into.
    const std::filesystem::path base = fromQString(QCoreApplication::applicationDirPath())
                                     / "data" / "cache";
#else
    const std::filesystem::path base = fromQString(
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
#endif

    std::error_code error;
    std::filesystem::create_directories(base, error);
    if (error) {
        log_error(kLogComponent,
                  "cannot create cache directory %s: %s",
                  base.string().c_str(),
                  error.message().c_str());
    }
    return base;
}

} // namespace showroom
