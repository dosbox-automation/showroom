// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "app/paths.h"

#include "app/logging.h"
#include "model/game_definition.h"

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

// Only a directory this call created gets its mode narrowed; rewriting
// permissions on one the user made is not ours to do.
std::filesystem::path ensureDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    const bool created = std::filesystem::create_directories(path, error);
    if (error) {
        log_error(kLogComponent,
                  "cannot create directory %s: %s",
                  path.string().c_str(),
                  error.message().c_str());
        return path;
    }
    if (created) {
        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     error);
        if (error) {
            log_warn(kLogComponent,
                     "cannot restrict permissions on %s: %s",
                     path.string().c_str(),
                     error.message().c_str());
        }
    }
    return path;
}

// Two layers on purpose: the parser's slug rule, then the composed path
// against its base. Neither change alone can open a traversal.
std::optional<std::filesystem::path> gameDir(const std::filesystem::path& parent,
                                             std::string_view slug)
{
    if (!isSafeSlug(slug)) {
        log_error(kLogComponent,
                  "refusing a game directory for an unsafe slug of %zu bytes",
                  slug.size());
        return std::nullopt;
    }

    const std::filesystem::path candidate = parent / std::filesystem::path(slug);
    if (!isWithin(parent, candidate)) {
        log_error(kLogComponent,
                  "refusing %s: outside %s",
                  candidate.string().c_str(),
                  parent.string().c_str());
        return std::nullopt;
    }
    return ensureDirectory(candidate);
}

} // namespace

bool isWithin(const std::filesystem::path& base, const std::filesystem::path& candidate)
{
    if (base.empty() || !base.is_absolute() || !candidate.is_absolute()) {
        return false;
    }

    const std::filesystem::path normal_base = base.lexically_normal();
    const std::filesystem::path normal_candidate = candidate.lexically_normal();

    // Element-wise rather than a string prefix: "/a/bb" starts with
    // "/a/b" as text and is a different directory.
    auto base_it = normal_base.begin();
    auto candidate_it = normal_candidate.begin();
    for (; base_it != normal_base.end(); ++base_it, ++candidate_it) {
        if (candidate_it == normal_candidate.end() || *candidate_it != *base_it) {
            return false;
        }
    }
    return true;
}

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
    if (const char* override_dir = std::getenv("SHOWROOM_CACHE_DIR")) {
        if (*override_dir != '\0') {
            return ensureDirectory(std::filesystem::path(override_dir));
        }
    }

#ifdef Q_OS_WIN
    // Portable by design: no registry, no AppData, everything the
    // showroom writes stays inside the folder it was unpacked into.
    const std::filesystem::path base = fromQString(QCoreApplication::applicationDirPath())
                                     / "data" / "cache";
#else
    const std::filesystem::path base = fromQString(
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
#endif

    return ensureDirectory(base);
}

std::filesystem::path Paths::downloadsDir()
{
    return ensureDirectory(cacheDir() / "downloads");
}

std::filesystem::path Paths::installsDir()
{
    return ensureDirectory(cacheDir() / "installs");
}

std::optional<std::filesystem::path> Paths::downloadDirFor(std::string_view slug)
{
    return gameDir(downloadsDir(), slug);
}

std::optional<std::filesystem::path> Paths::installDirFor(std::string_view slug)
{
    return gameDir(installsDir(), slug);
}

} // namespace showroom
