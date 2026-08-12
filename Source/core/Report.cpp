#include "Report.h"

#include <utility>

namespace aph
{
namespace
{
juce::var makeObject()
{
    return juce::var(new juce::DynamicObject());
}

void setProperty(juce::var& object, const juce::Identifier& name, const juce::var& value)
{
    object.getDynamicObject()->setProperty(name, value);
}

juce::var issueArrayToJson(const juce::Array<ValidationIssue>& issues)
{
    juce::Array<juce::var> array;
    for (const auto& issue : issues)
        array.add(issue.toJson());

    return array;
}
} // namespace

juce::var PluginReport::toJson() const
{
    auto object = makeObject();
    setProperty(object, "index", index);
    setProperty(object, "uuid", uuid);
    setProperty(object, "path", path);
    setProperty(object, "name", name);
    setProperty(object, "vendor", vendor);
    setProperty(object, "version", version);
    setProperty(object, "format", format);
    setProperty(object, "latencySamples", latencySamples);
    setProperty(object, "loaded", loaded);
    setProperty(object, "bypass", bypass);
    return object;
}

juce::var TimelineEvent::toJson() const
{
    auto object = makeObject();
    setProperty(object, "event", name);
    setProperty(object, "timestamp", timestamp);
    setProperty(object, "payload", payload);
    return object;
}

juce::var AudioMeasurement::toJson() const
{
    auto object = makeObject();
    setProperty(object, "tap", tap);
    setProperty(object, "sampleCount", sampleCount);
    setProperty(object, "channelCount", channelCount);
    setProperty(object, "sampleRate", sampleRate);
    setProperty(object, "minimum", minimum);
    setProperty(object, "maximum", maximum);
    setProperty(object, "absolutePeak", absolutePeak);
    setProperty(object, "peakDbfs", peakDbfs);
    setProperty(object, "rms", rms);
    setProperty(object, "rmsDbfs", rmsDbfs);
    setProperty(object, "mean", mean);
    setProperty(object, "dcOffset", dcOffset);
    setProperty(object, "crestFactor", crestFactor);
    setProperty(object, "clippedSampleCount", clippedSampleCount);
    setProperty(object, "nanCount", nanCount);
    setProperty(object, "positiveInfinityCount", positiveInfinityCount);
    setProperty(object, "negativeInfinityCount", negativeInfinityCount);
    setProperty(object, "denormalCount", denormalCount);
    setProperty(object, "silentSampleCount", silentSampleCount);
    setProperty(object, "maxConsecutiveSilenceSeconds", maxConsecutiveSilenceSeconds);
    setProperty(object, "zeroCrossingCount", zeroCrossingCount);
    setProperty(object, "nonFinite", nanCount + positiveInfinityCount + negativeInfinityCount);
    setProperty(object, "channels", channels);
    return object;
}

juce::var AssertionResult::toJson() const
{
    auto object = makeObject();
    setProperty(object, "type", type);
    setProperty(object, "op", op);
    setProperty(object, "expectedValue", expectedValue);
    setProperty(object, "actualValue", actualValue);
    setProperty(object, "passed", passed);
    setProperty(object, "message", message);
    return object;
}

juce::var Artifact::toJson() const
{
    auto object = makeObject();
    setProperty(object, "type", type);
    setProperty(object, "path", path);
    setProperty(object, "description", description);
    return object;
}

Report::Report()
{
    platform = makeObject();
    setProperty(platform, "os", juce::SystemStats::getOperatingSystemName());
    setProperty(platform, "cpu", juce::SystemStats::getCpuModel());
    setProperty(platform, "numCpus", juce::SystemStats::getNumCpus());
    setProperty(platform, "juceVersion", juce::SystemStats::getJUCEVersion());
    setProperty(platform, "juceRevision", APH_JUCE_REVISION);
    startedAt = timestampNowUtc();
}

void Report::addMeasurement(AudioMeasurement measurement)
{
    const auto tap = measurement.tap.isNotEmpty() ? measurement.tap : "unnamed";
    measurements.set(juce::Identifier(tap), measurement.toJson());
}

void Report::addError(juce::String path, juce::String message)
{
    errors.add({ std::move(path), std::move(message) });
}

void Report::addWarning(juce::String path, juce::String message)
{
    warnings.add({ std::move(path), std::move(message) });
}

juce::var Report::toJson() const
{
    auto object = makeObject();
    setProperty(object, "schemaVersion", schemaVersion);
    setProperty(object, "hostVersion", hostVersion);
    setProperty(object, "platform", platform);
    setProperty(object, "configuration", configuration.toJson());

    juce::Array<juce::var> pluginArray;
    for (const auto& plugin : plugins)
        pluginArray.add(plugin.toJson());
    setProperty(object, "plugins", pluginArray);

    juce::Array<juce::var> timelineArray;
    for (const auto& event : timeline)
        timelineArray.add(event.toJson());
    setProperty(object, "timeline", timelineArray);

    auto measurementObject = makeObject();
    for (int i = 0; i < measurements.size(); ++i)
        setProperty(measurementObject, measurements.getName(i), measurements.getValueAt(i));
    setProperty(object, "measurements", measurementObject);

    juce::Array<juce::var> assertionArray;
    for (const auto& assertion : assertions)
        assertionArray.add(assertion.toJson());
    setProperty(object, "assertions", assertionArray);

    juce::Array<juce::var> artifactArray;
    for (const auto& artifact : artifacts)
        artifactArray.add(artifact.toJson());
    setProperty(object, "artifacts", artifactArray);

    setProperty(object, "errors", issueArrayToJson(errors));
    setProperty(object, "warnings", issueArrayToJson(warnings));
    setProperty(object, "startedAt", startedAt);
    setProperty(object, "completedAt", completedAt);
    setProperty(object, "durationSeconds", durationSeconds);
    setProperty(object, "passed", passed);
    setProperty(object, "exitCode", static_cast<int>(exitCode));
    return object;
}

juce::String Report::toJsonString(bool pretty) const
{
    return juce::JSON::toString(toJson(), ! pretty);
}

bool Report::writeToFile(const juce::File& file, juce::String* errorMessage) const
{
    if (file == juce::File())
    {
        if (errorMessage != nullptr)
            *errorMessage = "report path is empty";

        return false;
    }

    if (! file.getParentDirectory().createDirectory())
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to create report directory: " + file.getParentDirectory().getFullPathName();

        return false;
    }

    if (! file.replaceWithText(toJsonString(true)))
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to write report: " + file.getFullPathName();

        return false;
    }

    return true;
}

juce::String Report::timestampNowUtc()
{
    return juce::Time::getCurrentTime().toISO8601(true);
}
} // namespace aph
