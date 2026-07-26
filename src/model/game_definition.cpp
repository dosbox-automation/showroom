// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/game_definition.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <utility>

namespace showroom {
namespace {

// DOSBox tops out well below this; the cap only exists so a definition
// cannot ask for a nonsense machine.
constexpr int kMaxCpuCycles = 1'000'000;
constexpr int kMaxInstallSeconds = 3600;
constexpr std::size_t kMaxExpectedFiles = 4096;

bool is_allowed_component_char(char c)
{
    const auto uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) != 0 || c == '.' || c == '-' || c == '_';
}

// Sources are consumed in order and the primary is what gets fetched
// first, so the order must not depend on how the TOML table is keyed.
int role_order(const std::string& role)
{
    if (role == "primary") {
        return 0;
    }
    if (role == "mirror") {
        return 1;
    }
    return 2;
}

// A URL the network layer hands to QNetworkAccessManager. Anything but
// plain HTTP(S) has no business in a game definition.
bool is_http_url(std::string_view url)
{
    return url.starts_with("https://") || url.starts_with("http://");
}

struct Parser {
    std::string error;

    bool fail(std::string message)
    {
        error = std::move(message);
        return false;
    }

    // Every lookup names its field in the error, so a bad definition says
    // which line to go and look at.
    bool require_string(const toml::table& table, std::string_view key, std::string& out,
                        std::string_view context)
    {
        const auto* node = table.get(key);
        if (node == nullptr) {
            return fail(std::string(context) + ": missing " + std::string(key));
        }
        const auto value = node->value<std::string>();
        if (!value) {
            return fail(std::string(context) + ": " + std::string(key)
                        + " must be a string");
        }
        out = *value;
        return true;
    }

    bool require_int(const toml::table& table, std::string_view key, int& out,
                     std::string_view context, int min, int max)
    {
        const auto* node = table.get(key);
        if (node == nullptr) {
            return fail(std::string(context) + ": missing " + std::string(key));
        }
        const auto value = node->value<std::int64_t>();
        if (!value) {
            return fail(std::string(context) + ": " + std::string(key)
                        + " must be an integer");
        }
        if (*value < min || *value > max) {
            return fail(std::string(context) + ": " + std::string(key) + " out of range");
        }
        out = static_cast<int>(*value);
        return true;
    }

    const toml::table* require_table(const toml::table& parent, std::string_view key)
    {
        const auto* node = parent.get(key);
        if (node == nullptr || !node->is_table()) {
            fail("missing [" + std::string(key) + "] section");
            return nullptr;
        }
        return node->as_table();
    }

    std::optional<std::string> optional_string(const toml::table& table,
                                               std::string_view key) const
    {
        const auto* node = table.get(key);
        if (node == nullptr) {
            return std::nullopt;
        }
        return node->value<std::string>();
    }
};

}  // namespace

bool is_safe_slug(std::string_view value)
{
    if (value.empty()) {
        return false;
    }
    const auto first = static_cast<unsigned char>(value.front());
    if (std::islower(first) == 0 && std::isdigit(first) == 0) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char c) {
        const auto uc = static_cast<unsigned char>(c);
        return std::islower(uc) != 0 || std::isdigit(uc) != 0 || c == '-' || c == '_';
    });
}

bool is_safe_path_component(std::string_view value)
{
    if (value.empty() || value == "." || value == "..") {
        return false;
    }
    return std::all_of(value.begin(), value.end(), is_allowed_component_char);
}

bool is_safe_relative_path(std::string_view value)
{
    if (value.empty()) {
        return true;  // the install root itself
    }
    std::size_t start = 0;
    while (true) {
        const auto end = value.find('/', start);
        const auto length =
            end == std::string_view::npos ? value.size() - start : end - start;
        if (!is_safe_path_component(value.substr(start, length))) {
            return false;
        }
        if (end == std::string_view::npos) {
            return true;
        }
        start = end + 1;
    }
}

std::optional<License> license_from_string(std::string_view value)
{
    if (value == "shareware") {
        return License::Shareware;
    }
    if (value == "freeware") {
        return License::Freeware;
    }
    if (value == "demo") {
        return License::Demo;
    }
    return std::nullopt;
}

std::optional<InstallType> install_type_from_string(std::string_view value)
{
    static constexpr std::array<std::pair<std::string_view, InstallType>, 5> kTypes{{
        {"unzip", InstallType::Unzip},
        {"unzipinstall", InstallType::UnzipInstall},
        {"exeinstall", InstallType::ExeInstall},
        {"floppyinstall", InstallType::FloppyInstall},
        {"isoinstall", InstallType::IsoInstall},
    }};
    for (const auto& [name, type] : kTypes) {
        if (name == value) {
            return type;
        }
    }
    return std::nullopt;
}

std::optional<GameDefinition> GameDefinition::fromToml(const std::filesystem::path& path,
                                                       std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path.string();
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();

    auto game = fromTomlString(buffer.str(), error);
    if (!game) {
        error = path.string() + ": " + error;
        return std::nullopt;
    }
    // The catalogue finds a definition by its file name and later resolves
    // the game's cache directory from the slug. If the two disagree, a game
    // would install into one directory and launch from another.
    const auto stem = path.stem().string();
    if (game->slug() != stem) {
        error = path.string() + ": slug \"" + game->slug() + "\" does not match file name";
        return std::nullopt;
    }
    return game;
}

std::optional<GameDefinition> GameDefinition::fromTomlString(std::string_view text,
                                                             std::string& error)
{
    error.clear();
    Parser parser;

    toml::table root;
    try {
        root = toml::parse(text);
    } catch (const toml::parse_error& e) {
        error = std::string("malformed TOML: ") + std::string(e.description());
        return std::nullopt;
    }

    GameDefinition game;

    if (!parser.require_string(root, "slug", game.slug_, "definition")) {
        error = parser.error;
        return std::nullopt;
    }
    if (!is_safe_slug(game.slug_)) {
        error = "definition: slug \"" + game.slug_ + "\" is not a safe directory name";
        return std::nullopt;
    }
    if (!parser.require_string(root, "title", game.title_, "definition")
        || !parser.require_int(root, "rank", game.rank_, "definition", 1, 9999)) {
        error = parser.error;
        return std::nullopt;
    }
    game.version_ = parser.optional_string(root, "version").value_or("");

    std::string license_name;
    if (!parser.require_string(root, "license", license_name, "definition")) {
        error = parser.error;
        return std::nullopt;
    }
    const auto license = license_from_string(license_name);
    if (!license) {
        error = "definition: unknown license \"" + license_name + "\"";
        return std::nullopt;
    }
    game.license_ = *license;

    const auto status = parser.optional_string(root, "recipe_status").value_or("todo");
    if (status != "done" && status != "todo") {
        error = "definition: unknown recipe_status \"" + status + "\"";
        return std::nullopt;
    }
    game.recipe_status_ = status == "done" ? RecipeStatus::Done : RecipeStatus::Todo;

    const auto* sources = parser.require_table(root, "sources");
    if (sources == nullptr) {
        error = parser.error;
        return std::nullopt;
    }
    for (const auto& [key, node] : *sources) {
        if (!node.is_table()) {
            error = "sources: " + std::string(key.str()) + " must be a table";
            return std::nullopt;
        }
        const auto& entry = *node.as_table();
        GameSource source;
        source.role = parser.optional_string(entry, "role").value_or(std::string(key.str()));
        if (!parser.require_string(entry, "url", source.url, "source " + source.role)) {
            error = parser.error;
            return std::nullopt;
        }
        if (!is_http_url(source.url)) {
            error = "source " + source.role + ": url must be http or https";
            return std::nullopt;
        }
        if (const auto type_name = parser.optional_string(entry, "install_type")) {
            const auto type = install_type_from_string(*type_name);
            if (!type) {
                error = "source " + source.role + ": unknown install type \"" + *type_name
                        + "\"";
                return std::nullopt;
            }
            source.install_type = type;
        }
        source.filename = parser.optional_string(entry, "filename");
        source.sha256 = parser.optional_string(entry, "sha256");
        game.sources_.push_back(std::move(source));
    }
    if (game.sources_.empty()) {
        error = "definition: at least one source is required";
        return std::nullopt;
    }
    std::stable_sort(game.sources_.begin(), game.sources_.end(),
                     [](const GameSource& a, const GameSource& b) {
                         return role_order(a.role) < role_order(b.role);
                     });

    const auto* dosbox = parser.require_table(root, "dosbox");
    if (dosbox == nullptr) {
        error = parser.error;
        return std::nullopt;
    }
    if (!parser.require_string(*dosbox, "machine", game.dosbox_.machine, "dosbox")
        || !parser.require_int(*dosbox, "cpu_cycles", game.dosbox_.cpu_cycles, "dosbox", 1,
                               kMaxCpuCycles)
        || !parser.require_int(*dosbox, "cpu_cycles_protected",
                               game.dosbox_.cpu_cycles_protected, "dosbox", 1,
                               kMaxCpuCycles)) {
        error = parser.error;
        return std::nullopt;
    }
    if (const auto* sound = dosbox->get("sound"); sound != nullptr && sound->is_table()) {
        const auto& table = *sound->as_table();
        game.dosbox_.sound.sblaster_type =
            parser.optional_string(table, "sblaster_type").value_or("");
        game.dosbox_.sound.mpu401 = parser.optional_string(table, "mpu401").value_or("");
        game.dosbox_.sound.midi_device =
            parser.optional_string(table, "midi_device").value_or("");
    }

    const auto* launch = parser.require_table(root, "launch");
    if (launch == nullptr) {
        error = parser.error;
        return std::nullopt;
    }
    game.launch_.executable = parser.optional_string(*launch, "executable").value_or("");
    game.launch_.working_dir = parser.optional_string(*launch, "working_dir").value_or("");
    game.launch_.setup_exe = parser.optional_string(*launch, "setup_exe").value_or("");
    // Empty means "no recipe yet" and stays valid. Anything present has to
    // be a bare file name: it is appended to the install directory and
    // typed into an autoexec line.
    if (!game.launch_.executable.empty()
        && !is_safe_path_component(game.launch_.executable)) {
        error = "launch: executable \"" + game.launch_.executable
                + "\" must be a plain file name";
        return std::nullopt;
    }
    if (!game.launch_.setup_exe.empty()
        && !is_safe_path_component(game.launch_.setup_exe)) {
        error = "launch: setup_exe \"" + game.launch_.setup_exe
                + "\" must be a plain file name";
        return std::nullopt;
    }
    if (!is_safe_relative_path(game.launch_.working_dir)) {
        error = "launch: working_dir \"" + game.launch_.working_dir
                + "\" must stay inside the install directory";
        return std::nullopt;
    }

    if (const auto* shots = root.get("screenshots");
        shots != nullptr && shots->is_table()) {
        const auto& table = *shots->as_table();
        game.screenshots_.title = parser.optional_string(table, "title").value_or("");
        game.screenshots_.gameplay =
            parser.optional_string(table, "gameplay").value_or("");
    }
    for (const auto* name : {&game.screenshots_.title, &game.screenshots_.gameplay}) {
        if (!name->empty() && !is_safe_path_component(*name)) {
            error = "screenshots: \"" + *name + "\" must be a plain file name";
            return std::nullopt;
        }
    }

    const auto* install = parser.require_table(root, "install");
    if (install == nullptr) {
        error = parser.error;
        return std::nullopt;
    }
    if (!parser.require_int(*install, "max_runtime_seconds",
                            game.install_.max_runtime_seconds, "install", 1,
                            kMaxInstallSeconds)) {
        error = parser.error;
        return std::nullopt;
    }
    if (const auto* files = install->get("expected_files");
        files != nullptr && files->is_table()) {
        const auto& table = *files->as_table();
        if (table.size() > kMaxExpectedFiles) {
            error = "install: too many expected files";
            return std::nullopt;
        }
        for (const auto& [key, node] : table) {
            ExpectedFile file;
            file.path = std::string(key.str());
            if (file.path.empty() || !is_safe_relative_path(file.path)) {
                error = "install: expected file \"" + file.path
                        + "\" must stay inside the install directory";
                return std::nullopt;
            }
            if (!node.is_table()) {
                error = "install: expected file \"" + file.path + "\" must be a table";
                return std::nullopt;
            }
            const auto& entry = *node.as_table();
            if (const auto* size = entry.get("size"); size != nullptr) {
                const auto value = size->value<std::int64_t>();
                if (!value || *value < 0) {
                    error = "install: expected file \"" + file.path
                            + "\" has an invalid size";
                    return std::nullopt;
                }
                file.size = static_cast<std::uint64_t>(*value);
            }
            file.sha256 = parser.optional_string(entry, "sha256");
            game.install_.expected_files.push_back(std::move(file));
        }
        // toml++ keys its tables in an unspecified order; sorting keeps the
        // integrity check reproducible and its failures readable.
        std::sort(game.install_.expected_files.begin(), game.install_.expected_files.end(),
                  [](const ExpectedFile& a, const ExpectedFile& b) {
                      return a.path < b.path;
                  });
    }

    return game;
}

}  // namespace showroom
