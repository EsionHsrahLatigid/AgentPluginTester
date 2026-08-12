#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>

namespace aph
{
enum class ExitCode
{
    success = 0,
    invalidConfiguration = 2,
    pluginLoadFailure = 3,
    deviceInitialisationFailure = 4,
    assertionFailure = 5,
    timeout = 6,
    outputWriteFailure = 7,
    fatalRuntimeError = 8
};

enum class RunMode
{
    realtime,
    offline
};

enum class InputSourceType
{
    silence,
    mic,
    sine,
    whiteNoise,
    pinkNoise,
    impulse,
    sweep,
    file
};

enum class LogLevel
{
    error,
    warn,
    info,
    debug
};

struct PluginConfig
{
    juce::File path;
    juce::String classId;
    bool bypass = false;

    juce::var toJson() const;
};

struct AudioConfig
{
    double sampleRate = 0.0;
    int blockSize = 0;
    int inputChannels = 2;
    int outputChannels = 2;
    juce::String inputDevice = "default";
    juce::String outputDevice = "default";

    juce::var toJson() const;
};

struct SourceConfig
{
    InputSourceType type = InputSourceType::silence;
    juce::File inputFile;
    double frequencyHz = 440.0;
    double levelDb = -18.0;
    std::uint64_t seed = 1;

    juce::var toJson() const;
};

struct HostConfig
{
    juce::String schemaVersion = "1.0";
    RunMode mode = RunMode::realtime;
    bool gui = true;
    bool showEditors = false;
    AudioConfig audio;
    SourceConfig source;
    juce::Array<PluginConfig> plugins;
    juce::Array<juce::var> scheduledEvents;
    juce::Array<juce::var> assertions;
    juce::StringArray failOn { "non-finite", "load-error" };
    juce::String midiInput;
    juce::String oscBind = "127.0.0.1";
    int oscPort = 9000;
    juce::String oscReplyHost = "sender";
    int oscReplyPort = -1;
    double runSeconds = 0.0;
    double timeoutSeconds = 30.0;
    juce::File recordPath;
    juce::File reportPath;
    juce::String eventsTarget = "stdout";
    LogLevel logLevel = LogLevel::info;
    bool listDevices = false;
    juce::File inspectPluginPath;
    bool versionRequested = false;
    bool helpRequested = false;

    juce::var toJson() const;
};

struct ValidationIssue
{
    juce::String path;
    juce::String message;

    juce::var toJson() const;
};

struct ValidationResult
{
    juce::Array<ValidationIssue> errors;
    juce::Array<ValidationIssue> warnings;

    bool ok() const { return errors.isEmpty(); }
    void addError(juce::String path, juce::String message);
    void addWarning(juce::String path, juce::String message);
};

struct ParseResult
{
    HostConfig config;
    ValidationResult validation;
    ExitCode exitCode = ExitCode::success;
    bool shouldExitImmediately = false;
    juce::String message;
};

class HostConfigParser
{
public:
    static ParseResult parseCommandLine(const juce::StringArray& arguments);
    static ParseResult parseCommandLine(int argc, const char* const* argv);

    static ValidationResult mergeSessionJson(HostConfig& config,
                                             const juce::var& session,
                                             const juce::File& sessionFile);

    static ValidationResult validate(const HostConfig& config);

    static juce::String getHelpText();
    static juce::String getVersionText();

    static juce::String toString(RunMode value);
    static juce::String toString(InputSourceType value);
    static juce::String toString(LogLevel value);

    static bool parseRunMode(const juce::String& text, RunMode& value);
    static bool parseInputSourceType(const juce::String& text, InputSourceType& value);
    static bool parseLogLevel(const juce::String& text, LogLevel& value);
};
} // namespace aph
