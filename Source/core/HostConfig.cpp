#include "HostConfig.h"

#include <utility>

namespace aph
{
namespace
{
constexpr const char* hostVersion = "0.1.0";

juce::var makeObject()
{
    return juce::var(new juce::DynamicObject());
}

void setProperty(juce::var& object, const juce::Identifier& name, const juce::var& value)
{
    object.getDynamicObject()->setProperty(name, value);
}

bool hasProperty(const juce::var& object, const juce::Identifier& name)
{
    if (auto* dynamicObject = object.getDynamicObject())
        return dynamicObject->hasProperty(name);

    return false;
}

juce::var getProperty(const juce::var& object, const juce::Identifier& name)
{
    if (auto* dynamicObject = object.getDynamicObject())
        return dynamicObject->getProperty(name);

    return {};
}

juce::Array<juce::Identifier> getPropertyNames(const juce::var& object)
{
    juce::Array<juce::Identifier> names;

    if (auto* dynamicObject = object.getDynamicObject())
    {
        const auto& properties = dynamicObject->getProperties();
        for (int i = 0; i < properties.size(); ++i)
            names.add(properties.getName(i));
    }

    return names;
}

bool isKnown(const juce::Identifier& name, std::initializer_list<const char*> knownNames)
{
    for (auto* knownName : knownNames)
        if (name == juce::Identifier(knownName))
            return true;

    return false;
}

bool toBool(const juce::var& value, bool fallback)
{
    return value.isBool() ? static_cast<bool>(value) : fallback;
}

double toDouble(const juce::var& value, double fallback)
{
    return value.isDouble() || value.isInt() || value.isInt64()
        ? static_cast<double>(value)
        : fallback;
}

int toInt(const juce::var& value, int fallback)
{
    return value.isInt() || value.isInt64() ? static_cast<int>(value) : fallback;
}

std::uint64_t toUint64(const juce::var& value, std::uint64_t fallback)
{
    if (value.isInt64())
        return static_cast<std::uint64_t>(static_cast<int64_t>(value));

    if (value.isInt())
        return static_cast<std::uint64_t>(static_cast<int>(value));

    return fallback;
}

juce::File resolvePath(const juce::String& text, const juce::File& baseDirectory)
{
    if (text.isEmpty())
        return {};

    return juce::File::isAbsolutePath(text) ? juce::File(text) : baseDirectory.getChildFile(text);
}

bool requiresValue(const juce::String& option)
{
    static const juce::StringArray options
    {
        "--plugin", "--session", "--mode", "--source", "--input-file", "--frequency",
        "--level-db", "--seed", "--sample-rate", "--block-size", "--input-channels",
        "--output-channels", "--audio-input-device", "--audio-output-device",
        "--midi-input", "--osc-bind", "--osc-port", "--osc-reply-host",
        "--osc-reply-port", "--run-seconds", "--timeout-seconds", "--record",
        "--report", "--events", "--fail-on", "--log-level", "--inspect-plugin"
    };

    return options.contains(option);
}

bool popValue(const juce::StringArray& arguments,
              int& index,
              const juce::String& option,
              juce::String& value,
              ValidationResult& validation)
{
    if (index + 1 >= arguments.size() || arguments[index + 1].startsWith("--"))
    {
        validation.addError(option, "missing required value");
        return false;
    }

    value = arguments[++index];
    return true;
}

juce::StringArray splitRuleList(const juce::String& text)
{
    juce::StringArray values;
    values.addTokens(text, ",", "");
    values.trim();
    values.removeEmptyStrings();
    return values;
}

void mergeAudio(HostConfig& config,
                const juce::var& audio,
                const juce::File&,
                ValidationResult& validation)
{
    if (! audio.isObject())
    {
        validation.addError("audio", "must be an object");
        return;
    }

    for (const auto& name : getPropertyNames(audio))
        if (! isKnown(name, { "sampleRate", "blockSize", "inputChannels", "outputChannels",
                             "inputDevice", "outputDevice" }))
            validation.addWarning("audio." + name.toString(), "unknown session field");

    if (hasProperty(audio, "sampleRate"))
        config.audio.sampleRate = toDouble(getProperty(audio, "sampleRate"), config.audio.sampleRate);

    if (hasProperty(audio, "blockSize"))
        config.audio.blockSize = toInt(getProperty(audio, "blockSize"), config.audio.blockSize);

    if (hasProperty(audio, "inputChannels"))
        config.audio.inputChannels = toInt(getProperty(audio, "inputChannels"), config.audio.inputChannels);

    if (hasProperty(audio, "outputChannels"))
        config.audio.outputChannels = toInt(getProperty(audio, "outputChannels"), config.audio.outputChannels);

    if (hasProperty(audio, "inputDevice"))
        config.audio.inputDevice = getProperty(audio, "inputDevice").toString();

    if (hasProperty(audio, "outputDevice"))
        config.audio.outputDevice = getProperty(audio, "outputDevice").toString();
}

void mergeSource(HostConfig& config,
                 const juce::var& source,
                 const juce::File& sessionDirectory,
                 ValidationResult& validation)
{
    if (! source.isObject())
    {
        validation.addError("source", "must be an object");
        return;
    }

    for (const auto& name : getPropertyNames(source))
        if (! isKnown(name, { "type", "inputFile", "file", "frequency", "frequencyHz",
                             "levelDb", "levelDB", "level_db", "seed" }))
            validation.addWarning("source." + name.toString(), "unknown session field");

    if (hasProperty(source, "type"))
    {
        InputSourceType parsed {};
        const auto text = getProperty(source, "type").toString();
        if (HostConfigParser::parseInputSourceType(text, parsed))
            config.source.type = parsed;
        else
            validation.addError("source.type", "unsupported input source: " + text);
    }

    const auto fileValue = hasProperty(source, "inputFile") ? getProperty(source, "inputFile")
                         : hasProperty(source, "file") ? getProperty(source, "file")
                         : juce::var();

    if (! fileValue.isVoid())
        config.source.inputFile = resolvePath(fileValue.toString(), sessionDirectory);

    if (hasProperty(source, "frequency"))
        config.source.frequencyHz = toDouble(getProperty(source, "frequency"), config.source.frequencyHz);

    if (hasProperty(source, "frequencyHz"))
        config.source.frequencyHz = toDouble(getProperty(source, "frequencyHz"), config.source.frequencyHz);

    if (hasProperty(source, "levelDb"))
        config.source.levelDb = toDouble(getProperty(source, "levelDb"), config.source.levelDb);

    if (hasProperty(source, "levelDB"))
        config.source.levelDb = toDouble(getProperty(source, "levelDB"), config.source.levelDb);

    if (hasProperty(source, "level_db"))
        config.source.levelDb = toDouble(getProperty(source, "level_db"), config.source.levelDb);

    if (hasProperty(source, "seed"))
        config.source.seed = toUint64(getProperty(source, "seed"), config.source.seed);
}

void mergePlugins(HostConfig& config,
                  const juce::var& plugins,
                  const juce::File& sessionDirectory,
                  ValidationResult& validation)
{
    if (! plugins.isArray())
    {
        validation.addError("plugins", "must be an array");
        return;
    }

    config.plugins.clear();

    auto* array = plugins.getArray();
    for (int i = 0; i < array->size(); ++i)
    {
        const auto& item = array->getReference(i);
        const auto prefix = "plugins[" + juce::String(i) + "]";

        if (! item.isObject())
        {
            validation.addError(prefix, "must be an object");
            continue;
        }

        for (const auto& name : getPropertyNames(item))
            if (! isKnown(name, { "path", "classId", "classID", "bypass" }))
                validation.addWarning(prefix + "." + name.toString(), "unknown session field");

        PluginConfig plugin;
        plugin.path = resolvePath(getProperty(item, "path").toString(), sessionDirectory);
        plugin.classId = hasProperty(item, "classId") ? getProperty(item, "classId").toString()
                       : hasProperty(item, "classID") ? getProperty(item, "classID").toString()
                       : juce::String();
        plugin.bypass = toBool(getProperty(item, "bypass"), false);
        config.plugins.add(plugin);
    }
}

void copyArrayField(juce::Array<juce::var>& target, const juce::var& value, const juce::String& path, ValidationResult& validation)
{
    if (! value.isArray())
    {
        validation.addError(path, "must be an array");
        return;
    }

    target.clear();
    for (const auto& item : *value.getArray())
        target.add(item);
}

} // namespace

juce::var PluginConfig::toJson() const
{
    auto object = makeObject();
    setProperty(object, "path", path.getFullPathName());
    setProperty(object, "classId", classId);
    setProperty(object, "bypass", bypass);
    return object;
}

juce::var AudioConfig::toJson() const
{
    auto object = makeObject();
    setProperty(object, "sampleRate", sampleRate);
    setProperty(object, "blockSize", blockSize);
    setProperty(object, "inputChannels", inputChannels);
    setProperty(object, "outputChannels", outputChannels);
    setProperty(object, "inputDevice", inputDevice);
    setProperty(object, "outputDevice", outputDevice);
    return object;
}

juce::var SourceConfig::toJson() const
{
    auto object = makeObject();
    setProperty(object, "type", HostConfigParser::toString(type));
    setProperty(object, "inputFile", inputFile.getFullPathName());
    setProperty(object, "frequencyHz", frequencyHz);
    setProperty(object, "levelDb", levelDb);
    setProperty(object, "seed", static_cast<int64_t>(seed));
    return object;
}

juce::var HostConfig::toJson() const
{
    auto object = makeObject();
    setProperty(object, "schemaVersion", schemaVersion);
    setProperty(object, "mode", HostConfigParser::toString(mode));
    setProperty(object, "gui", gui);
    setProperty(object, "showEditors", showEditors);
    setProperty(object, "audio", audio.toJson());
    setProperty(object, "source", source.toJson());

    juce::Array<juce::var> pluginArray;
    for (const auto& plugin : plugins)
        pluginArray.add(plugin.toJson());
    setProperty(object, "plugins", pluginArray);

    setProperty(object, "events", scheduledEvents);
    setProperty(object, "assertions", assertions);

    juce::Array<juce::var> failRules;
    for (const auto& rule : failOn)
        failRules.add(rule);
    setProperty(object, "failOn", failRules);

    setProperty(object, "midiInput", midiInput);
    setProperty(object, "oscBind", oscBind);
    setProperty(object, "oscPort", oscPort);
    setProperty(object, "oscReplyHost", oscReplyHost);
    setProperty(object, "oscReplyPort", oscReplyPort);
    setProperty(object, "runSeconds", runSeconds);
    setProperty(object, "timeoutSeconds", timeoutSeconds);
    setProperty(object, "recordPath", recordPath.getFullPathName());
    setProperty(object, "reportPath", reportPath.getFullPathName());
    setProperty(object, "eventsTarget", eventsTarget);
    setProperty(object, "logLevel", HostConfigParser::toString(logLevel));
    setProperty(object, "listDevices", listDevices);
    setProperty(object, "inspectPluginPath", inspectPluginPath.getFullPathName());
    return object;
}

juce::var ValidationIssue::toJson() const
{
    auto object = makeObject();
    setProperty(object, "path", path);
    setProperty(object, "message", message);
    return object;
}

void ValidationResult::addError(juce::String path, juce::String message)
{
    errors.add({ std::move(path), std::move(message) });
}

void ValidationResult::addWarning(juce::String path, juce::String message)
{
    warnings.add({ std::move(path), std::move(message) });
}

ParseResult HostConfigParser::parseCommandLine(int argc, const char* const* argv)
{
    juce::StringArray arguments;
    for (int i = 1; i < argc; ++i)
        arguments.add(argv[i]);

    return parseCommandLine(arguments);
}

ParseResult HostConfigParser::parseCommandLine(const juce::StringArray& arguments)
{
    ParseResult result;
    juce::Array<PluginConfig> cliPlugins;

    for (int i = 0; i < arguments.size(); ++i)
    {
        const auto option = arguments[i];
        if (option != "--session")
            continue;

        juce::String value;
        if (! popValue(arguments, i, option, value, result.validation))
            continue;

        const auto sessionFile = juce::File(value);
        const auto parsed = juce::JSON::parse(sessionFile);
        if (parsed.isVoid())
        {
            result.validation.addError("--session", "failed to parse session JSON: " + sessionFile.getFullPathName());
            continue;
        }

        auto sessionValidation = mergeSessionJson(result.config, parsed, sessionFile);
        result.validation.errors.addArray(sessionValidation.errors);
        result.validation.warnings.addArray(sessionValidation.warnings);
    }

    for (int i = 0; i < arguments.size(); ++i)
    {
        const auto option = arguments[i];

        if (! option.startsWith("--"))
        {
            result.validation.addError("argv[" + juce::String(i) + "]", "unexpected positional argument: " + option);
            continue;
        }

        if (option == "--help")
        {
            result.config.helpRequested = true;
            result.shouldExitImmediately = true;
            continue;
        }

        if (option == "--version")
        {
            result.config.versionRequested = true;
            result.shouldExitImmediately = true;
            continue;
        }

        if (option == "--gui")
        {
            result.config.gui = true;
            continue;
        }

        if (option == "--no-gui")
        {
            result.config.gui = false;
            continue;
        }

        if (option == "--show-editors")
        {
            result.config.showEditors = true;
            continue;
        }

        if (option == "--list-devices")
        {
            result.config.listDevices = true;
            result.shouldExitImmediately = true;
            continue;
        }

        juce::String value;
        if (requiresValue(option) && ! popValue(arguments, i, option, value, result.validation))
            continue;

        if (option == "--session")
        {
            continue;
        }
        else if (option == "--plugin")
        {
            cliPlugins.add({ juce::File(value), {}, false });
        }
        else if (option == "--mode")
        {
            RunMode mode {};
            if (parseRunMode(value, mode))
                result.config.mode = mode;
            else
                result.validation.addError(option, "unsupported mode: " + value);
        }
        else if (option == "--source")
        {
            InputSourceType type {};
            if (parseInputSourceType(value, type))
                result.config.source.type = type;
            else
                result.validation.addError(option, "unsupported source: " + value);
        }
        else if (option == "--input-file")
        {
            result.config.source.inputFile = juce::File(value);
        }
        else if (option == "--frequency")
        {
            result.config.source.frequencyHz = value.getDoubleValue();
        }
        else if (option == "--level-db")
        {
            result.config.source.levelDb = value.getDoubleValue();
        }
        else if (option == "--seed")
        {
            result.config.source.seed = static_cast<std::uint64_t>(value.getLargeIntValue());
        }
        else if (option == "--sample-rate")
        {
            result.config.audio.sampleRate = value.getDoubleValue();
        }
        else if (option == "--block-size")
        {
            result.config.audio.blockSize = value.getIntValue();
        }
        else if (option == "--input-channels")
        {
            result.config.audio.inputChannels = value.getIntValue();
        }
        else if (option == "--output-channels")
        {
            result.config.audio.outputChannels = value.getIntValue();
        }
        else if (option == "--audio-input-device")
        {
            result.config.audio.inputDevice = value;
        }
        else if (option == "--audio-output-device")
        {
            result.config.audio.outputDevice = value;
        }
        else if (option == "--midi-input")
        {
            result.config.midiInput = value;
        }
        else if (option == "--osc-bind")
        {
            result.config.oscBind = value;
        }
        else if (option == "--osc-port")
        {
            result.config.oscPort = value.getIntValue();
        }
        else if (option == "--osc-reply-host")
        {
            result.config.oscReplyHost = value;
        }
        else if (option == "--osc-reply-port")
        {
            result.config.oscReplyPort = value.getIntValue();
        }
        else if (option == "--run-seconds")
        {
            result.config.runSeconds = value.getDoubleValue();
        }
        else if (option == "--timeout-seconds")
        {
            result.config.timeoutSeconds = value.getDoubleValue();
        }
        else if (option == "--record")
        {
            result.config.recordPath = juce::File(value);
        }
        else if (option == "--report")
        {
            result.config.reportPath = juce::File(value);
        }
        else if (option == "--events")
        {
            result.config.eventsTarget = value;
        }
        else if (option == "--fail-on")
        {
            result.config.failOn = splitRuleList(value);
        }
        else if (option == "--log-level")
        {
            LogLevel level {};
            if (parseLogLevel(value, level))
                result.config.logLevel = level;
            else
                result.validation.addError(option, "unsupported log level: " + value);
        }
        else if (option == "--inspect-plugin")
        {
            result.config.inspectPluginPath = juce::File(value);
            result.shouldExitImmediately = true;
        }
        else
        {
            result.validation.addError(option, "unknown option");
        }
    }

    if (! cliPlugins.isEmpty())
    {
        result.config.plugins.clear();
        result.config.plugins.addArray(cliPlugins);
    }

    const auto validation = validate(result.config);
    result.validation.errors.addArray(validation.errors);
    result.validation.warnings.addArray(validation.warnings);

    if (! result.validation.ok())
        result.exitCode = ExitCode::invalidConfiguration;

    if (result.config.helpRequested)
        result.message = getHelpText();
    else if (result.config.versionRequested)
        result.message = getVersionText();

    return result;
}

ValidationResult HostConfigParser::mergeSessionJson(HostConfig& config,
                                                    const juce::var& session,
                                                    const juce::File& sessionFile)
{
    ValidationResult validation;
    const auto sessionDirectory = sessionFile.getParentDirectory();

    if (! session.isObject())
    {
        validation.addError("session", "must be a JSON object");
        return validation;
    }

    for (const auto& name : getPropertyNames(session))
        if (! isKnown(name, { "schemaVersion", "mode", "gui", "showEditors", "audio", "source",
                             "plugins", "events", "durationSeconds", "capture", "assertions",
                             "report", "failOn", "timeoutSeconds" }))
            validation.addWarning(name.toString(), "unknown session field");

    const auto schema = getProperty(session, "schemaVersion").toString();
    if (schema.isNotEmpty() && schema != "1.0")
        validation.addError("schemaVersion", "unsupported session schemaVersion: " + schema);

    if (hasProperty(session, "mode"))
    {
        RunMode parsed {};
        const auto text = getProperty(session, "mode").toString();
        if (parseRunMode(text, parsed))
            config.mode = parsed;
        else
            validation.addError("mode", "unsupported mode: " + text);
    }

    if (hasProperty(session, "gui"))
        config.gui = toBool(getProperty(session, "gui"), config.gui);

    if (hasProperty(session, "showEditors"))
        config.showEditors = toBool(getProperty(session, "showEditors"), config.showEditors);

    if (hasProperty(session, "audio"))
        mergeAudio(config, getProperty(session, "audio"), sessionDirectory, validation);

    if (hasProperty(session, "source"))
        mergeSource(config, getProperty(session, "source"), sessionDirectory, validation);

    if (hasProperty(session, "plugins"))
        mergePlugins(config, getProperty(session, "plugins"), sessionDirectory, validation);

    if (hasProperty(session, "events"))
        copyArrayField(config.scheduledEvents, getProperty(session, "events"), "events", validation);

    if (hasProperty(session, "assertions"))
        copyArrayField(config.assertions, getProperty(session, "assertions"), "assertions", validation);

    if (hasProperty(session, "durationSeconds"))
        config.runSeconds = toDouble(getProperty(session, "durationSeconds"), config.runSeconds);

    if (hasProperty(session, "timeoutSeconds"))
        config.timeoutSeconds = toDouble(getProperty(session, "timeoutSeconds"), config.timeoutSeconds);

    if (hasProperty(session, "capture"))
    {
        const auto capture = getProperty(session, "capture");
        if (capture.isObject() && hasProperty(capture, "path"))
            config.recordPath = resolvePath(getProperty(capture, "path").toString(), sessionDirectory);
        else
            validation.addError("capture", "must be an object with a path field");
    }

    if (hasProperty(session, "report"))
    {
        const auto report = getProperty(session, "report");
        if (report.isObject() && hasProperty(report, "path"))
            config.reportPath = resolvePath(getProperty(report, "path").toString(), sessionDirectory);
        else
            validation.addError("report", "must be an object with a path field");
    }

    if (hasProperty(session, "failOn"))
    {
        const auto rules = getProperty(session, "failOn");
        if (rules.isArray())
        {
            config.failOn.clear();
            for (const auto& rule : *rules.getArray())
                config.failOn.add(rule.toString());
        }
        else
        {
            config.failOn = splitRuleList(rules.toString());
        }
    }

    return validation;
}

ValidationResult HostConfigParser::validate(const HostConfig& config)
{
    ValidationResult validation;

    if (config.schemaVersion != "1.0")
        validation.addError("schemaVersion", "unsupported configuration schemaVersion: " + config.schemaVersion);

    if (config.audio.sampleRate < 0.0)
        validation.addError("audio.sampleRate", "must be positive or 0 for device/default");

    if (config.audio.blockSize < 0)
        validation.addError("audio.blockSize", "must be positive or 0 for device/default");

    if (config.audio.inputChannels < 0)
        validation.addError("audio.inputChannels", "must be zero or greater");

    if (config.audio.outputChannels <= 0)
        validation.addError("audio.outputChannels", "must be greater than zero");

    if (config.oscPort < 0 || config.oscPort > 65535)
        validation.addError("oscPort", "must be between 0 and 65535");

    if (config.oscReplyPort != -1 && (config.oscReplyPort < 0 || config.oscReplyPort > 65535))
        validation.addError("oscReplyPort", "must be between 0 and 65535, or -1 for sender");

    if (config.runSeconds < 0.0)
        validation.addError("runSeconds", "must be zero or greater");

    if (config.timeoutSeconds <= 0.0)
        validation.addError("timeoutSeconds", "must be greater than zero");

    if (config.source.type == InputSourceType::file && config.source.inputFile == juce::File())
        validation.addError("source.inputFile", "is required when source is file");

    if (config.source.type == InputSourceType::sine && config.source.frequencyHz <= 0.0)
        validation.addError("source.frequencyHz", "must be greater than zero for sine source");

    for (int i = 0; i < config.plugins.size(); ++i)
        if (config.plugins.getReference(i).path == juce::File())
            validation.addError("plugins[" + juce::String(i) + "].path", "is required");

    double lastTime = -1.0;
    for (int i = 0; i < config.scheduledEvents.size(); ++i)
    {
        const auto& event = config.scheduledEvents.getReference(i);
        if (! event.isObject())
        {
            validation.addError("events[" + juce::String(i) + "]", "must be an object");
            continue;
        }

        const auto atSeconds = hasProperty(event, "atSeconds") ? toDouble(getProperty(event, "atSeconds"), -1.0) : lastTime;
        if (atSeconds < 0.0)
            validation.addError("events[" + juce::String(i) + "].atSeconds", "must be zero or greater when present");

        if (atSeconds < lastTime)
            validation.addError("events[" + juce::String(i) + "].atSeconds", "events must be sorted by time");

        lastTime = atSeconds;
    }

    return validation;
}

juce::String HostConfigParser::getHelpText()
{
    return "AgentPluginHost 0.1.0\n"
           "Usage: AgentPluginHost [global-options] --plugin <path> [--plugin <path> ...]\n\n"
           "Options:\n"
           "  --plugin <path>                 Add VST3 to the serial chain; repeatable.\n"
           "  --session <path>                Load JSON session; relative paths resolve from the session directory.\n"
           "  --mode <realtime|offline>       Processing mode; default realtime.\n"
           "  --gui | --no-gui                Show or hide host GUI; default GUI on.\n"
           "  --show-editors                  Show plugin editors after load.\n"
           "  --source <type>                 mic, sine, white-noise, pink-noise, impulse, sweep, silence, file.\n"
           "  --input-file <path>             Audio file used by --source file.\n"
           "  --frequency <hz>                Sine frequency; default 440.\n"
           "  --level-db <dbfs>               Generated signal level; default -18.\n"
           "  --seed <uint64>                 Deterministic noise/random seed; default 1.\n"
           "  --sample-rate <hz>              Sample rate; 0 means device/default.\n"
           "  --block-size <samples>          Block size; 0 means device/default.\n"
           "  --input-channels <count>        Logical input channels; default 2.\n"
           "  --output-channels <count>       Logical output channels; default 2.\n"
           "  --audio-input-device <name/id>  Realtime input device.\n"
           "  --audio-output-device <name/id> Realtime output device.\n"
           "  --midi-input <name/id>          External MIDI input.\n"
           "  --osc-bind <address>            OSC bind address; default 127.0.0.1.\n"
           "  --osc-port <port>               OSC port, 0 disables; default 9000.\n"
           "  --osc-reply-host <address>      Reply host; default sender.\n"
           "  --osc-reply-port <port>         Reply port; default sender.\n"
           "  --run-seconds <seconds>         Automatic run duration; 0 means unlimited.\n"
           "  --timeout-seconds <seconds>     Startup/load/shutdown watchdog; default 30.\n"
           "  --record <path>                 Write float WAV capture.\n"
           "  --report <path>                 Write final JSON report.\n"
           "  --events <path|stdout>          Write NDJSON events; default stdout.\n"
           "  --fail-on <a,b>                 Failure rule list; default non-finite,load-error.\n"
           "  --log-level <level>             error, warn, info, debug.\n"
           "  --list-devices                  List audio/MIDI devices and exit.\n"
           "  --inspect-plugin <path>         Inspect plugin metadata and exit.\n"
           "  --version                       Print version and exit.\n"
           "  --help                          Print this help and exit.\n\n"
           "Exit codes: 0 success, 2 invalid CLI/session, 3 plugin load failure, 4 device failure,\n"
           "5 assertion failure, 6 timeout, 7 output/report write failure, 8 fatal runtime error.\n";
}

juce::String HostConfigParser::getVersionText()
{
    return "AgentPluginHost " + juce::String(hostVersion);
}

juce::String HostConfigParser::toString(RunMode value)
{
    return value == RunMode::offline ? "offline" : "realtime";
}

juce::String HostConfigParser::toString(InputSourceType value)
{
    switch (value)
    {
        case InputSourceType::mic: return "mic";
        case InputSourceType::sine: return "sine";
        case InputSourceType::whiteNoise: return "white-noise";
        case InputSourceType::pinkNoise: return "pink-noise";
        case InputSourceType::impulse: return "impulse";
        case InputSourceType::sweep: return "sweep";
        case InputSourceType::file: return "file";
        case InputSourceType::silence: break;
    }

    return "silence";
}

juce::String HostConfigParser::toString(LogLevel value)
{
    switch (value)
    {
        case LogLevel::error: return "error";
        case LogLevel::warn: return "warn";
        case LogLevel::debug: return "debug";
        case LogLevel::info: break;
    }

    return "info";
}

bool HostConfigParser::parseRunMode(const juce::String& text, RunMode& value)
{
    if (text == "realtime")
    {
        value = RunMode::realtime;
        return true;
    }

    if (text == "offline")
    {
        value = RunMode::offline;
        return true;
    }

    return false;
}

bool HostConfigParser::parseInputSourceType(const juce::String& text, InputSourceType& value)
{
    if (text == "silence") value = InputSourceType::silence;
    else if (text == "mic") value = InputSourceType::mic;
    else if (text == "sine") value = InputSourceType::sine;
    else if (text == "white-noise") value = InputSourceType::whiteNoise;
    else if (text == "pink-noise") value = InputSourceType::pinkNoise;
    else if (text == "impulse") value = InputSourceType::impulse;
    else if (text == "sweep") value = InputSourceType::sweep;
    else if (text == "file") value = InputSourceType::file;
    else return false;

    return true;
}

bool HostConfigParser::parseLogLevel(const juce::String& text, LogLevel& value)
{
    if (text == "error") value = LogLevel::error;
    else if (text == "warn") value = LogLevel::warn;
    else if (text == "info") value = LogLevel::info;
    else if (text == "debug") value = LogLevel::debug;
    else return false;

    return true;
}
} // namespace aph
