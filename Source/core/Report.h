#pragma once

#include "HostConfig.h"

#include <juce_core/juce_core.h>

namespace aph
{
struct PluginReport
{
    int index = 0;
    juce::String uuid;
    juce::String path;
    juce::String name;
    juce::String vendor;
    juce::String version;
    juce::String format = "VST3";
    int latencySamples = 0;
    bool loaded = false;
    bool bypass = false;

    juce::var toJson() const;
};

struct TimelineEvent
{
    juce::String name;
    juce::String timestamp;
    juce::var payload;

    juce::var toJson() const;
};

struct AudioMeasurement
{
    juce::String tap;
    int64_t sampleCount = 0;
    int channelCount = 0;
    double sampleRate = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    double absolutePeak = 0.0;
    double peakDbfs = -100.0;
    double rms = 0.0;
    double rmsDbfs = -100.0;
    double mean = 0.0;
    double dcOffset = 0.0;
    double crestFactor = 0.0;
    int64_t clippedSampleCount = 0;
    int64_t nanCount = 0;
    int64_t positiveInfinityCount = 0;
    int64_t negativeInfinityCount = 0;
    int64_t denormalCount = 0;
    int64_t silentSampleCount = 0;
    double maxConsecutiveSilenceSeconds = 0.0;
    int64_t zeroCrossingCount = 0;
    juce::Array<juce::var> channels;

    juce::var toJson() const;
};

struct AssertionResult
{
    juce::String type;
    juce::String op;
    double expectedValue = 0.0;
    double actualValue = 0.0;
    bool passed = false;
    juce::String message;

    juce::var toJson() const;
};

struct Artifact
{
    juce::String type;
    juce::String path;
    juce::String description;

    juce::var toJson() const;
};

class Report
{
public:
    juce::String schemaVersion = "1.0";
    juce::String hostVersion = "0.1.0";
    juce::var platform;
    HostConfig configuration;
    juce::Array<PluginReport> plugins;
    juce::Array<TimelineEvent> timeline;
    juce::NamedValueSet measurements;
    juce::Array<AssertionResult> assertions;
    juce::Array<Artifact> artifacts;
    juce::Array<ValidationIssue> errors;
    juce::Array<ValidationIssue> warnings;
    juce::String startedAt;
    juce::String completedAt;
    double durationSeconds = 0.0;
    bool passed = false;
    ExitCode exitCode = ExitCode::success;

    Report();

    void addMeasurement(AudioMeasurement measurement);
    void addError(juce::String path, juce::String message);
    void addWarning(juce::String path, juce::String message);

    juce::var toJson() const;
    juce::String toJsonString(bool pretty = true) const;
    bool writeToFile(const juce::File& file, juce::String* errorMessage = nullptr) const;

    static juce::String timestampNowUtc();
};
} // namespace aph
