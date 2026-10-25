#include "timetracker/config/ConfigService.hpp"

#include "timetracker/config/ConfigFile.hpp"
#include "timetracker/json/DocumentIo.hpp"
#include "timetracker/paths/DataPaths.hpp"

#include <map>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

namespace timetracker {
namespace {

constexpr std::string_view kEnvPrefix = "TIME_TRACKER_";
constexpr std::string_view kDataDirectoryEnv = "TIME_TRACKER_DATA_DIRECTORY";
constexpr const char* kAbsolutePathError = "dataDirectory must be an absolute path";

struct CompiledDefault {
    bool present{false};
    std::string generic;
    std::string error;
};

struct Overlay {
    std::optional<std::string> value;
    ConfigLayer layer{ConfigLayer::Default};
    bool sawFile{false};
};

struct DotAssignment {
    std::string key;
    std::string value;
};

[[nodiscard]] std::filesystem::path configJson(const std::filesystem::path& directory) {
    return directory / "config.json";
}

[[nodiscard]] std::filesystem::path dotenvPath(const std::filesystem::path& directory) {
    return directory / ".env";
}

[[nodiscard]] CompiledDefault compiledDefault(const IEnvironment& environment) {
    auto directory = userDataDirectory(environment);
    if (!directory) {
        return CompiledDefault{false, {}, directory.error()};
    }
    return CompiledDefault{true, directory.value().generic_string(), {}};
}

[[nodiscard]] Result<void> requireAbsolute(const std::filesystem::path& path) {
    if (!path.is_absolute()) {
        return Result<void>::err(kAbsolutePathError);
    }
    return Result<void>::ok();
}

[[nodiscard]] Result<ConfigLayer> writeDocument(IFilesystem& files, const ConfigLocations& locations,
                                               const std::string& dataDirectory) {
    ConfigFile document;
    document.schemaVersion = kConfigSchema;
    document.dataDirectory = dataDirectory;
    auto executable = storeJson(files, configJson(locations.executable), document);
    if (executable) {
        return Result<ConfigLayer>::ok(ConfigLayer::ExeFile);
    }
    auto user = storeJson(files, configJson(locations.user), document);
    if (!user) {
        return Result<ConfigLayer>::err(user.error().message);
    }
    return Result<ConfigLayer>::ok(ConfigLayer::UserFile);
}

[[nodiscard]] std::string_view trimView(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] std::string stripComment(std::string_view line) {
    bool quoted = false;
    char quote = '\0';
    std::string out;
    for (const char ch : line) {
        if (quoted) {
            out.push_back(ch);
            if (ch == quote) {
                quoted = false;
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quoted = true;
            quote = ch;
            out.push_back(ch);
            continue;
        }
        if (ch == '#') {
            break;
        }
        out.push_back(ch);
    }
    return out;
}

[[nodiscard]] std::string unquote(std::string_view value) {
    if (value.size() < 2) {
        return std::string{value};
    }
    const char quote = value.front();
    if ((quote != '"' && quote != '\'') || value.back() != quote) {
        return std::string{value};
    }
    return std::string{value.substr(1, value.size() - 2)};
}

[[nodiscard]] Result<std::optional<DotAssignment>> parseDotLine(std::string_view raw) {
    auto text = trimView(stripComment(raw));
    if (text.empty()) {
        return Result<std::optional<DotAssignment>>::ok(std::nullopt);
    }
    if (text.starts_with("export ")) {
        text.remove_prefix(7);
        text = trimView(text);
    }
    if (text.empty()) {
        return Result<std::optional<DotAssignment>>::ok(std::nullopt);
    }
    const auto split = text.find('=');
    if (split == std::string_view::npos) {
        return Result<std::optional<DotAssignment>>::err("malformed .env line");
    }
    const auto key = trimView(text.substr(0, split));
    if (key.empty()) {
        return Result<std::optional<DotAssignment>>::err("malformed .env line");
    }
    DotAssignment assignment;
    assignment.key = std::string{key};
    assignment.value = unquote(trimView(text.substr(split + 1)));
    return Result<std::optional<DotAssignment>>::ok(std::move(assignment));
}

[[nodiscard]] Result<std::map<std::string, std::string>> parseDotEnv(const std::string& text) {
    std::map<std::string, std::string> values;
    std::size_t cursor = 0;
    while (cursor <= text.size()) {
        const auto end = text.find('\n', cursor);
        const auto count = end == std::string::npos ? std::string::npos : end - cursor;
        const auto line = text.substr(cursor, count);
        auto parsed = parseDotLine(line);
        if (!parsed) {
            return Result<std::map<std::string, std::string>>::err(parsed.error());
        }
        if (parsed.value().has_value() && !parsed.value()->value.empty()) {
            values[parsed.value()->key] = parsed.value()->value;
        }
        if (end == std::string::npos) {
            break;
        }
        cursor = end + 1;
    }
    return Result<std::map<std::string, std::string>>::ok(std::move(values));
}

[[nodiscard]] Result<void> rejectUnknownDotenv(const std::map<std::string, std::string>& values,
                                               ConfigLayer layer) {
    for (const auto& [key, value] : values) {
        (void)value;
        if (key != kDataDirectoryEnv) {
            return Result<void>::err("unknown key '" + key + "' in layer " + configLayerName(layer));
        }
    }
    return Result<void>::ok();
}

[[nodiscard]] Result<void> overlayConfigFile(IFilesystem& files, const std::filesystem::path& path,
                                             ConfigLayer layer, Overlay& fileLayer, Overlay& effective) {
    if (!files.exists(path)) {
        return Result<void>::ok();
    }
    effective.sawFile = true;
    auto loaded = loadJson<ConfigFile>(files, path);
    if (!loaded) {
        return Result<void>::err(loaded.error().message + " in layer " + configLayerName(layer));
    }
    if (!loaded.value().dataDirectory.has_value()) {
        return Result<void>::ok();
    }
    fileLayer.value = loaded.value().dataDirectory;
    fileLayer.layer = layer;
    effective.value = fileLayer.value;
    effective.layer = layer;
    return Result<void>::ok();
}

[[nodiscard]] Result<void> overlayDotFile(IFilesystem& files, const std::filesystem::path& path,
                                          ConfigLayer layer, std::optional<std::string>& slot,
                                          Overlay& effective) {
    if (!files.exists(path)) {
        return Result<void>::ok();
    }
    auto text = files.readText(path);
    if (!text) {
        return Result<void>::err(path.generic_string() + ": " + text.error() + " in layer " +
                                 configLayerName(layer));
    }
    auto parsed = parseDotEnv(text.value());
    if (!parsed) {
        return Result<void>::err(parsed.error() + " in layer " + configLayerName(layer));
    }
    auto known = rejectUnknownDotenv(parsed.value(), layer);
    if (!known) {
        return known;
    }
    const auto found = parsed.value().find(std::string{kDataDirectoryEnv});
    if (found == parsed.value().end()) {
        return Result<void>::ok();
    }
    slot = found->second;
    effective.value = slot;
    effective.layer = layer;
    return Result<void>::ok();
}

[[nodiscard]] Result<std::optional<std::string>> readProcessDirectory(const IEnvironment& environment) {
    std::optional<std::string> found;
    for (const auto& [key, value] : environment.entries()) {
        if (value.empty() || !key.starts_with(kEnvPrefix)) {
            continue;
        }
        if (key != kDataDirectoryEnv) {
            return Result<std::optional<std::string>>::err("unknown key '" + key +
                                                           "' in layer process-env");
        }
        found = value;
    }
    return Result<std::optional<std::string>>::ok(std::move(found));
}

[[nodiscard]] bool isIgnoredFlag(const std::string& argument) {
    return argument == "--help" || argument == "--version";
}

[[nodiscard]] Result<std::optional<std::string>> parseDataDirectoryFlag(
    const std::vector<std::string>& arguments) {
    std::optional<std::string> found;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (isIgnoredFlag(argument)) {
            continue;
        }
        if (argument == "--data-directory") {
            if (index + 1 >= arguments.size()) {
                return Result<std::optional<std::string>>::err("missing value for --data-directory");
            }
            found = arguments[++index];
            continue;
        }
        constexpr std::string_view kEquals = "--data-directory=";
        if (argument.starts_with(kEquals)) {
            found = argument.substr(kEquals.size());
            continue;
        }
        if (argument.starts_with("--")) {
            return Result<std::optional<std::string>>::err("unknown flag '" + argument +
                                                           "' in layer cli");
        }
    }
    return Result<std::optional<std::string>>::ok(std::move(found));
}

void applySlot(std::filesystem::path& directory, ConfigLayer& layer,
               const std::optional<std::string>& value, ConfigLayer slotLayer) {
    if (!value.has_value()) {
        return;
    }
    directory = std::filesystem::path{*value};
    layer = slotLayer;
}

[[nodiscard]] Result<void> applyConfigTrio(IFilesystem& files, const ConfigLocations& locations,
                                          Overlay& fileLayer, Overlay& effective) {
    if (auto applied = overlayConfigFile(files, configJson(locations.system), ConfigLayer::SystemFile,
                                         fileLayer, effective);
        !applied) {
        return applied;
    }
    if (auto applied = overlayConfigFile(files, configJson(locations.user), ConfigLayer::UserFile,
                                         fileLayer, effective);
        !applied) {
        return applied;
    }
    return overlayConfigFile(files, configJson(locations.executable), ConfigLayer::ExeFile, fileLayer,
                             effective);
}

[[nodiscard]] Result<void> applyDotTrio(IFilesystem& files, const ConfigLocations& locations,
                                       std::optional<std::string>& systemSlot,
                                       std::optional<std::string>& userSlot,
                                       std::optional<std::string>& exeSlot, Overlay& effective) {
    if (auto applied = overlayDotFile(files, dotenvPath(locations.system), ConfigLayer::SystemDotenv,
                                      systemSlot, effective);
        !applied) {
        return applied;
    }
    if (auto applied =
            overlayDotFile(files, dotenvPath(locations.user), ConfigLayer::UserDotenv, userSlot, effective);
        !applied) {
        return applied;
    }
    return overlayDotFile(files, dotenvPath(locations.executable), ConfigLayer::ExeDotenv, exeSlot,
                          effective);
}

struct RuntimeOverlay {
    std::optional<std::string> process;
    std::optional<std::string> command;
};

[[nodiscard]] Result<RuntimeOverlay> readRuntimeOverlay(const IEnvironment& environment,
                                                       const std::vector<std::string>& arguments) {
    auto process = readProcessDirectory(environment);
    if (!process) {
        return Result<RuntimeOverlay>::err(process.error());
    }
    auto flag = parseDataDirectoryFlag(arguments);
    if (!flag) {
        return Result<RuntimeOverlay>::err(flag.error());
    }
    return Result<RuntimeOverlay>::ok(RuntimeOverlay{std::move(process.value()), std::move(flag.value())});
}

void applyRuntime(const RuntimeOverlay& runtime, Overlay& effective) {
    if (runtime.process.has_value()) {
        effective.value = runtime.process;
        effective.layer = ConfigLayer::ProcessEnv;
    }
    if (runtime.command.has_value()) {
        effective.value = runtime.command;
        effective.layer = ConfigLayer::Cli;
    }
}

[[nodiscard]] Result<std::filesystem::path> requireEffective(const Overlay& effective,
                                                             const CompiledDefault& compiled) {
    if (!effective.value.has_value()) {
        if (!compiled.present) {
            return Result<std::filesystem::path>::err(compiled.error);
        }
        return Result<std::filesystem::path>::err("dataDirectory is not set");
    }
    const std::filesystem::path chosen{*effective.value};
    if (auto absolute = requireAbsolute(chosen); !absolute) {
        return Result<std::filesystem::path>::err(absolute.error());
    }
    return Result<std::filesystem::path>::ok(chosen);
}

[[nodiscard]] Result<void> rememberFileLayer(const CompiledDefault& compiled, const Overlay& fileLayer,
                                             bool sawFile, std::string& stored, ConfigLayer& storedLayer,
                                             IFilesystem& files, const ConfigLocations& locations) {
    if (!sawFile) {
        if (!compiled.present) {
            return Result<void>::ok();
        }
        auto wrote = writeDocument(files, locations, compiled.generic);
        if (!wrote) {
            return Result<void>::err(wrote.error());
        }
        stored = compiled.generic;
        storedLayer = wrote.value();
        return Result<void>::ok();
    }
    if (fileLayer.value.has_value()) {
        stored = *fileLayer.value;
        storedLayer = fileLayer.layer;
        return Result<void>::ok();
    }
    if (compiled.present) {
        stored = compiled.generic;
        storedLayer = ConfigLayer::Default;
    }
    return Result<void>::ok();
}

}  // namespace

const char* configLayerName(ConfigLayer layer) {
    switch (layer) {
    case ConfigLayer::Default:
        return "default";
    case ConfigLayer::SystemFile:
        return "system-file";
    case ConfigLayer::UserFile:
        return "user-file";
    case ConfigLayer::ExeFile:
        return "exe-file";
    case ConfigLayer::SystemDotenv:
        return "system-dotenv";
    case ConfigLayer::UserDotenv:
        return "user-dotenv";
    case ConfigLayer::ExeDotenv:
        return "exe-dotenv";
    case ConfigLayer::ProcessEnv:
        return "process-env";
    case ConfigLayer::Cli:
        return "cli";
    }
    return "default";
}

Result<ConfigLocations> configLocations(const IEnvironment& environment,
                                        const std::filesystem::path& executableDirectory) {
    ConfigLocations locations;
    locations.executable = executableDirectory;
#if defined(_WIN32)
    auto programData = environment.get("ProgramData");
    if (!programData.has_value()) {
        return Result<ConfigLocations>::err("ProgramData is not set");
    }
    auto appData = environment.get("APPDATA");
    if (!appData.has_value()) {
        return Result<ConfigLocations>::err("APPDATA is not set");
    }
    locations.system = std::filesystem::path{*programData} / "Time Tracker";
    locations.user = std::filesystem::path{*appData} / "Time Tracker";
#else
    locations.system = std::filesystem::path{"/etc/time-tracker"};
    auto configHome = environment.get("XDG_CONFIG_HOME");
    if (configHome.has_value()) {
        locations.user = std::filesystem::path{*configHome} / "time-tracker";
    } else {
        auto home = environment.get("HOME");
        if (!home.has_value()) {
            return Result<ConfigLocations>::err("HOME is not set");
        }
        locations.user = std::filesystem::path{*home} / ".config" / "time-tracker";
    }
#endif
    return Result<ConfigLocations>::ok(std::move(locations));
}

Result<void> ConfigService::load(const IEnvironment& environment, IFilesystem& files,
                                 const ConfigLocations& locations,
                                 const std::vector<std::string>& arguments, std::ostream& log) {
    locations_ = locations;
    const auto compiled = compiledDefault(environment);
    Overlay effective;
    if (compiled.present) {
        effective.value = compiled.generic;
        effective.layer = ConfigLayer::Default;
    }

    Overlay fileLayer;
    if (auto applied = applyConfigTrio(files, locations, fileLayer, effective); !applied) {
        return applied;
    }
    if (auto applied = applyDotTrio(files, locations, systemDotenv_, userDotenv_, exeDotenv_, effective);
        !applied) {
        return applied;
    }

    auto runtime = readRuntimeOverlay(environment, arguments);
    if (!runtime) {
        return Result<void>::err(runtime.error());
    }
    processDirectory_ = runtime.value().process;
    cliDirectory_ = runtime.value().command;
    applyRuntime(runtime.value(), effective);

    auto chosen = requireEffective(effective, compiled);
    if (!chosen) {
        return Result<void>::err(chosen.error());
    }

    if (auto remembered = rememberFileLayer(compiled, fileLayer, effective.sawFile, fileDirectory_,
                                            fileLayer_, files, locations_);
        !remembered) {
        return remembered;
    }

    dataDirectory_ = chosen.value();
    layer_ = effective.layer;
    log << "config dataDirectory=" << dataDirectory_.generic_string()
        << " layer=" << configLayerName(layer_) << '\n';
    return Result<void>::ok();
}

Result<void> ConfigService::storeDataDirectory(IFilesystem& files,
                                               const std::filesystem::path& directory) {
    if (auto absolute = requireAbsolute(directory); !absolute) {
        return absolute;
    }
    const auto previousDirectory = fileDirectory_;
    const auto previousLayer = fileLayer_;
    fileDirectory_ = directory.generic_string();
    auto wrote = writeDocument(files, locations_, fileDirectory_);
    if (!wrote) {
        fileDirectory_ = previousDirectory;
        fileLayer_ = previousLayer;
        return Result<void>::err(wrote.error());
    }
    fileLayer_ = wrote.value();
    reapplyOverlays();
    return Result<void>::ok();
}

std::filesystem::path ConfigService::effectiveIfStored(const std::filesystem::path& directory,
                                                       ConfigLayer& winning) const {
    auto effective = directory;
    winning = fileLayer_;
    applySlot(effective, winning, systemDotenv_, ConfigLayer::SystemDotenv);
    applySlot(effective, winning, userDotenv_, ConfigLayer::UserDotenv);
    applySlot(effective, winning, exeDotenv_, ConfigLayer::ExeDotenv);
    applySlot(effective, winning, processDirectory_, ConfigLayer::ProcessEnv);
    applySlot(effective, winning, cliDirectory_, ConfigLayer::Cli);
    return effective;
}

void ConfigService::reapplyOverlays() {
    dataDirectory_ = std::filesystem::path{fileDirectory_};
    layer_ = fileLayer_;
    applySlot(dataDirectory_, layer_, systemDotenv_, ConfigLayer::SystemDotenv);
    applySlot(dataDirectory_, layer_, userDotenv_, ConfigLayer::UserDotenv);
    applySlot(dataDirectory_, layer_, exeDotenv_, ConfigLayer::ExeDotenv);
    applySlot(dataDirectory_, layer_, processDirectory_, ConfigLayer::ProcessEnv);
    applySlot(dataDirectory_, layer_, cliDirectory_, ConfigLayer::Cli);
}

}  // namespace timetracker
