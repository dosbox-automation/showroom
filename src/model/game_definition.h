// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_MODEL_GAME_DEFINITION_H
#define SHOWROOM_MODEL_GAME_DEFINITION_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace showroom {

enum class License { Shareware, Freeware, Demo };

enum class InstallType { Unzip, UnzipInstall, ExeInstall, FloppyInstall, IsoInstall };

// A game with no install recipe yet still gets a tile; it just cannot be
// installed. See the tile states in the design document.
enum class RecipeStatus { Done, Todo };

struct GameSource {
    std::string role;
    std::optional<InstallType> install_type;
    std::string url;
    std::optional<std::string> filename;
    std::optional<std::string> sha256;
    // Archive size in bytes, for the pre-download free-space check.
    std::optional<std::uint64_t> size;
};

struct DosboxSound {
    std::string sblaster_type;
    std::string mpu401;
    std::string midi_device;
};

struct DosboxSettings {
    std::string machine;
    // Both rates matter: cpu_cycles covers real mode, cpu_cycles_protected
    // covers everything after a DOS extender switches modes.
    int cpu_cycles = 0;
    int cpu_cycles_protected = 0;
    DosboxSound sound;
};

struct LaunchSettings {
    std::string executable;
    std::string working_dir;
    std::string setup_exe;
};

struct Screenshots {
    std::string title;
    std::string gameplay;
};

struct ExpectedFile {
    std::string path;
    std::optional<std::uint64_t> size;
    std::optional<std::string> sha256;
};

struct InstallSettings {
    int max_runtime_seconds = 0;
    std::vector<ExpectedFile> expected_files;
};

// Everything here ends up in a path, a mount or an autoexec line, so the
// parse validates rather than trusting the file. A definition that fails
// validation is never half-constructed.
class GameDefinition {
public:
    static std::optional<GameDefinition> fromToml(const std::filesystem::path& path,
                                                  std::string& error);
    static std::optional<GameDefinition> fromTomlString(std::string_view toml,
                                                        std::string& error);

    const std::string& slug() const { return slug_; }
    const std::string& title() const { return title_; }
    int rank() const { return rank_; }
    const std::string& version() const { return version_; }
    License license() const { return license_; }
    RecipeStatus recipeStatus() const { return recipe_status_; }
    const std::vector<GameSource>& sources() const { return sources_; }
    const DosboxSettings& dosbox() const { return dosbox_; }
    const LaunchSettings& launch() const { return launch_; }
    const Screenshots& screenshots() const { return screenshots_; }
    const InstallSettings& install() const { return install_; }

    // A definition with no launch executable has no recipe yet: the tile
    // is shown, the Play button is not offered.
    bool isLaunchable() const { return !launch_.executable.empty(); }

private:
    GameDefinition() = default;

    std::string slug_;
    std::string title_;
    int rank_ = 0;
    std::string version_;
    License license_ = License::Shareware;
    RecipeStatus recipe_status_ = RecipeStatus::Todo;
    std::vector<GameSource> sources_;
    DosboxSettings dosbox_;
    LaunchSettings launch_;
    Screenshots screenshots_;
    InstallSettings install_;
};

// Matches what tools/csv_to_games.py enforces on the way out, and is
// deliberately stricter than a path component: the two are not
// interchangeable.
bool isSafeSlug(std::string_view value);

// Safe to append to a path: rejects empty, absolute, traversal,
// separators and anything outside a conservative character set.
bool isSafePathComponent(std::string_view value);

// Forces a name into that character set (underscores for the rest);
// empty when nothing safe remains.
std::string sanitizedPathComponent(std::string_view value);

// Same for a multi-segment path such as "GOLD/DOOM"; empty means the
// install root itself.
bool isSafeRelativePath(std::string_view value);

std::optional<License> licenseFromString(std::string_view value);
std::optional<InstallType> installTypeFromString(std::string_view value);

} // namespace showroom

#endif // SHOWROOM_MODEL_GAME_DEFINITION_H
