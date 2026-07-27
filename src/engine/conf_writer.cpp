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

bool validate(const GameDefinition& game, const std::filesystem::path& cache_base,
              std::string& error)
{
    if (!cache_base.is_absolute()) {
        error = "cache base must be an absolute path";
        return false;
    }
    if (!pathFitsConfLine(cache_base)) {
        error = "cache base path cannot be carried by a conf line";
        return false;
    }
    if (!isSafeSlug(game.slug())) {
        error = "slug \"" + game.slug() + "\" is not a safe directory name";
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

} // namespace

std::optional<std::string> ConfWriter::renderConf(const GameDefinition& game,
                                                  const std::filesystem::path& cache_base,
                                                  std::string& error)
{
    error.clear();
    if (!validate(game, cache_base, error)) {
        return std::nullopt;
    }

    const auto& sound = game.dosbox().sound;
    std::ostringstream conf;

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
         << "webserver_token_file = false\n\n";

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
    std::error_code ec;
    if (!std::filesystem::is_directory(cache_base, ec)) {
        error = "cache base does not exist: " + cache_base.string();
        return std::nullopt;
    }

    std::random_device rd;
    const auto temp = cache_base / std::format("run.conf.{:08x}{:08x}.tmp", rd(), rd());
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "cannot create " + temp.string();
            return std::nullopt;
        }
        out << *conf;
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

    const auto target = cache_base / "run.conf";
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        error = "cannot move conf into place: " + ec.message();
        std::filesystem::remove(temp, ec);
        return std::nullopt;
    }
    return target;
}

} // namespace showroom
