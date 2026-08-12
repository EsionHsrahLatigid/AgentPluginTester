#include "PluginLoader.h"

namespace agent_plugin_host::plugins
{

PluginLoader::PluginLoader()
{
    juce::addDefaultFormatsToManager (formatManager);
}

PluginScanResult PluginLoader::scanSinglePluginFile (const juce::File& pluginPath,
                                                     const juce::File& deadManMarker) const
{
    PluginScanResult result;
    result.path = pluginPath.getFullPathName();

    if (const auto validationError = validatePluginPath (pluginPath); validationError.isNotEmpty())
    {
        result.error = validationError;
        return result;
    }

    writeDeadManMarker (deadManMarker, pluginPath);

    juce::KnownPluginList knownPlugins;
    auto pathText = pluginPath.getFullPathName();

    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormat (i);
        if (format == nullptr || ! format->fileMightContainThisPluginType (pathText))
            continue;

        juce::OwnedArray<juce::PluginDescription> foundForFormat;
        const auto scanSucceeded = knownPlugins.scanAndAddFile (pathText, false, foundForFormat, *format);

        if (! scanSucceeded)
        {
            if (result.descriptions.isEmpty())
                result.error = "Plugin scan failed for " + pluginPath.getFullPathName();

            continue;
        }

        for (auto* description : foundForFormat)
            result.descriptions.add (new juce::PluginDescription (*description));

        if (! foundForFormat.isEmpty())
            result.error = {};
    }

    if (result.descriptions.isEmpty() && result.error.isEmpty())
        result.error = "No plugin classes were found in " + pluginPath.getFullPathName();

    return result;
}

PluginLoadResult PluginLoader::createInstanceBlocking (const juce::PluginDescription& description,
                                                       double sampleRate,
                                                       int maximumBlockSize,
                                                       int slotIndex) const
{
    PluginLoadResult result;
    juce::String error;
    result.instance = formatManager.createPluginInstance (description, sampleRate, maximumBlockSize, error);
    result.error = error;

    if (result.instance == nullptr && result.error.isEmpty())
        result.error = "Plugin instance creation failed";

    if (result.instance != nullptr)
        result.metadata = metadataFor (description, slotIndex, juce::Uuid().toString(), result.instance.get());

    return result;
}

void PluginLoader::createInstanceAsync (const juce::PluginDescription& description,
                                        double sampleRate,
                                        int maximumBlockSize,
                                        int slotIndex,
                                        AsyncLoadCallback loadCallback) const
{
    formatManager.createPluginInstanceAsync (
        description,
        sampleRate,
        maximumBlockSize,
        [description, slotIndex, storedCallback = std::move (loadCallback)] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                                                             const juce::String& error) mutable
        {
            PluginLoadResult result;
            result.error = error;
            result.instance = std::move (instance);

            if (result.instance == nullptr && result.error.isEmpty())
                result.error = "Plugin instance creation failed";

            if (result.instance != nullptr)
                result.metadata = metadataFor (description, slotIndex, juce::Uuid().toString(), result.instance.get());

            storedCallback (std::move (result));
        });
}

juce::String PluginLoader::validatePluginPath (const juce::File& pluginPath)
{
    if (! pluginPath.exists())
        return "Plugin path does not exist: " + pluginPath.getFullPathName();

    if (! pluginPath.hasFileExtension (".vst3"))
        return "Only VST3 plugins are supported by the initial scanner: " + pluginPath.getFullPathName();

    return {};
}

void PluginLoader::writeDeadManMarker (const juce::File& markerFile, const juce::File& pluginPath)
{
    if (markerFile == juce::File())
        return;

    juce::DynamicObject::Ptr marker = new juce::DynamicObject();
    marker->setProperty ("schemaVersion", "1.0");
    marker->setProperty ("event", "scanner_active");
    marker->setProperty ("path", pluginPath.getFullPathName());
    marker->setProperty ("updatedAt", juce::Time::getCurrentTime().toISO8601 (true));

    markerFile.replaceWithText (juce::JSON::toString (juce::var (marker.get()), false));
}

PluginMetadata PluginLoader::metadataFor (const juce::PluginDescription& description,
                                          int slotIndex,
                                          const juce::String& runtimeId,
                                          const juce::AudioPluginInstance* instance)
{
    auto metadata = PluginMetadata::fromDescription (description, slotIndex, runtimeId);

    if (instance != nullptr)
    {
        metadata.reportedLatencySamples = instance->getLatencySamples();
        metadata.acceptsMidi = instance->acceptsMidi();
        metadata.producesMidi = instance->producesMidi();
        metadata.inputChannels = instance->getTotalNumInputChannels();
        metadata.outputChannels = instance->getTotalNumOutputChannels();
    }

    return metadata;
}

} // namespace agent_plugin_host::plugins
