// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/conf_writer.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <random>
#include <sstream>
#include <string_view>
#include <system_error>

namespace showroom {
namespace {

// Anything outside this set can forge conf lines or sections.
bool isSafeConfValue(std::string_view value)
{
    if (value.empty()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char c) {
        const auto uc = static_cast<unsigned char>(c);
        return std::islower(uc) != 0 || std::isdigit(uc) != 0 || c == '_' || c == '-';
    });
}

bool pathFitsConfLine(const std::filesystem::path& path)
{
    const auto text = path.string();
    return text.find('"') == std::string::npos && text.find('\n') == std::string::npos
        && text.find('\r') == std::string::npos;
}

std::string toDosPath(std::string_view posix_relative)
{
    std::string result(posix_relative);
    std::replace(result.begin(), result.end(), '/', '\\');
    return result;
}

bool wantsCdDrive(const GameDefinition& game)
{
    const auto& sources = game.sources();
    return !sources.empty() && sources.front().install_type == InstallType::IsoInstall;
}

bool validateConfDir(const std::filesystem::path& dir, std::string_view name,
                     std::string& error)
{
    if (!dir.is_absolute()) {
        error = std::string(name) + " must be an absolute path";
        return false;
    }
    if (!pathFitsConfLine(dir)) {
        error = std::string(name) + " path cannot be carried by a conf line";
        return false;
    }
    return true;
}

bool validateEngineSettings(const GameDefinition& game, std::string& error)
{
    if (!isSafeSlug(game.slug())) {
        error = "slug \"" + game.slug() + "\" is not a safe directory name";
        return false;
    }
    if (game.dosbox().cpu_cycles <= 0 || game.dosbox().cpu_cycles_protected <= 0) {
        error = "cpu cycle settings must be positive";
        return false;
    }
    if (!isSafeConfValue(game.dosbox().machine)) {
        error = "machine \"" + game.dosbox().machine + "\" is not a safe conf value";
        return false;
    }
    const auto& sound = game.dosbox().sound;
    for (const auto& [name, value] :
         {std::pair<std::string_view, const std::string&>{"sblaster_type",
                                                          sound.sblaster_type},
          {"mpu401", sound.mpu401},
          {"midi_device", sound.midi_device}}) {
        if (!value.empty() && !isSafeConfValue(value)) {
            error = std::string(name) + " \"" + value + "\" is not a safe conf value";
            return false;
        }
    }
    return true;
}

bool validate(const GameDefinition& game, const std::filesystem::path& cache_base,
              std::string& error)
{
    if (!validateConfDir(cache_base, "cache base", error)) {
        return false;
    }
    if (!validateEngineSettings(game, error)) {
        return false;
    }
    if (!game.isLaunchable()) {
        error = "game \"" + game.slug() + "\" has no launch executable";
        return false;
    }
    if (!isSafePathComponent(game.launch().executable)) {
        error = "executable \"" + game.launch().executable
              + "\" is not a plain file name";
        return false;
    }
    if (!isSafeRelativePath(game.launch().working_dir)) {
        error = "working_dir \"" + game.launch().working_dir
              + "\" escapes the install directory";
        return false;
    }
    return true;
}

void renderEngineSettings(const GameDefinition& game, std::ostringstream& conf)
{
    const auto& sound = game.dosbox().sound;
    conf << "[sdl]\n"
         << "output = texture\n\n";
    conf << "[dosbox]\n"
         << "machine = " << game.dosbox().machine << "\n\n";
    conf << "[cpu]\n"
         << "cpu_cycles = " << game.dosbox().cpu_cycles << "\n"
         << "cpu_cycles_protected = " << game.dosbox().cpu_cycles_protected << "\n\n";
    if (!sound.sblaster_type.empty()) {
        conf << "[sblaster]\n"
             << "sbtype = " << sound.sblaster_type << "\n\n";
    }
    if (!sound.midi_device.empty() || !sound.mpu401.empty()) {
        conf << "[midi]\n";
        if (!sound.midi_device.empty()) {
            conf << "mididevice = " << sound.midi_device << "\n";
        }
        if (!sound.mpu401.empty()) {
            conf << "mpu401 = " << sound.mpu401 << "\n";
        }
        conf << "\n";
    }
    conf << "[webserver]\n"
         << "webserver_enabled = true\n"
         << "webserver_token_file = false\n"
         << "webserver_port = " << kShowroomEnginePort << "\n\n";
}

bool isFloppyImageName(const std::filesystem::path& name)
{
    auto ext = name.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".ima" || ext == ".img";
}

std::optional<std::filesystem::path> firstFloppyImage(
        const std::filesystem::path& extracts_dir, std::string& error)
{
    std::error_code ec;
    std::vector<std::filesystem::path> images;
    for (const auto& entry : std::filesystem::directory_iterator(extracts_dir, ec)) {
        if (entry.is_regular_file(ec) && isFloppyImageName(entry.path().filename())) {
            images.push_back(entry.path());
        }
    }
    if (ec) {
        error = "cannot scan extracts dir: " + ec.message();
        return std::nullopt;
    }
    if (images.empty()) {
        error = "no floppy image in " + extracts_dir.string();
        return std::nullopt;
    }
    return *std::min_element(images.begin(), images.end());
}

// Atomic and owner-only; on failure nothing is left behind.
std::optional<std::filesystem::path> writeConfFile(const std::filesystem::path& dir,
                                                   const std::string& filename,
                                                   const std::string& contents,
                                                   std::string& error)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        error = "target directory does not exist: " + dir.string();
        return std::nullopt;
    }

    std::random_device rd;
    const auto temp = dir / std::format("{}.{:08x}{:08x}.tmp", filename, rd(), rd());
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "cannot create " + temp.string();
            return std::nullopt;
        }
        out << contents;
        out.flush();
        if (!out) {
            error = "write failed: " + temp.string();
            out.close();
            std::filesystem::remove(temp, ec);
            return std::nullopt;
        }
    }

    // The ofstream mode is umask-dependent; the conf must end up 0600.
    std::filesystem::permissions(temp,
                                 std::filesystem::perms::owner_read
                                         | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace,
                                 ec);
    if (ec) {
        error = "cannot set mode on " + temp.string() + ": " + ec.message();
        std::filesystem::remove(temp, ec);
        return std::nullopt;
    }

    const auto target = dir / filename;
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        error = "cannot move conf into place: " + ec.message();
        std::filesystem::remove(temp, ec);
        return std::nullopt;
    }
    return target;
}

} // namespace

std::optional<std::string> ConfWriter::renderConf(const GameDefinition& game,
                                                  const std::filesystem::path& cache_base,
                                                  std::string& error)
{
    error.clear();
    if (!validate(game, cache_base, error)) {
        return std::nullopt;
    }

    std::ostringstream conf;
    renderEngineSettings(game, conf);

    conf << "[autoexec]\n";
    conf << "mount c \"" << (cache_base / "installs").string() << "\"\n";
    if (wantsCdDrive(game)) {
        conf << "mount d \"" << (cache_base / "downloads" / game.slug()).string()
             << "\" -t cdrom\n";
    }
    conf << "c:\n";
    std::string dos_dir = "\\" + game.slug();
    if (!game.launch().working_dir.empty()) {
        dos_dir += "\\" + toDosPath(game.launch().working_dir);
    }
    conf << "cd " << dos_dir << "\n";
    conf << game.launch().executable << "\n";
    conf << "exit\n";

    return conf.str();
}

std::optional<std::filesystem::path> ConfWriter::writeConf(
        const GameDefinition& game, const std::filesystem::path& cache_base,
        std::string& error)
{
    const auto conf = renderConf(game, cache_base, error);
    if (!conf) {
        return std::nullopt;
    }
    return writeConfFile(cache_base, "run.conf", *conf, error);
}

std::optional<std::string> ConfWriter::renderInstallConf(
        const GameDefinition& game, const std::filesystem::path& extracts_dir,
        std::string& error)
{
    error.clear();
    if (!validateConfDir(extracts_dir, "extracts dir", error)) {
        return std::nullopt;
    }
    if (!validateEngineSettings(game, error)) {
        return std::nullopt;
    }
    if (game.sources().empty()) {
        error = "game \"" + game.slug() + "\" has no sources";
        return std::nullopt;
    }
    const auto install_type = game.sources().front().install_type;
    if (!install_type) {
        error = "primary source of \"" + game.slug() + "\" has no install type";
        return std::nullopt;
    }
    if (*install_type == InstallType::Unzip) {
        error = "install type of \"" + game.slug()
              + "\" is not driven through the engine";
        return std::nullopt;
    }

    std::ostringstream conf;
    renderEngineSettings(game, conf);

    conf << "[autoexec]\n";
    if (*install_type == InstallType::FloppyInstall) {
        const auto image = firstFloppyImage(extracts_dir, error);
        if (!image) {
            return std::nullopt;
        }
        conf << "mount a \"" << image->string() << "\" -t floppy\n";
    } else if (*install_type == InstallType::IsoInstall) {
        conf << "mount d \"" << extracts_dir.string() << "\" -t cdrom\n";
    } else {
        // Self-extractors and unzipped installers are DOS programs run
        // from D:, not media.
        conf << "mount d \"" << extracts_dir.string() << "\"\n";
    }
    conf << "mount c \"" << installStagingDir(extracts_dir).string() << "\"\n";

    return conf.str();
}

std::optional<std::filesystem::path> ConfWriter::writeInstallConf(
        const GameDefinition& game, const std::filesystem::path& extracts_dir,
        std::string& error)
{
    const auto conf = renderInstallConf(game, extracts_dir, error);
    if (!conf) {
        return std::nullopt;
    }
    return writeConfFile(extracts_dir, "install.conf", *conf, error);
}

} // namespace showroom
