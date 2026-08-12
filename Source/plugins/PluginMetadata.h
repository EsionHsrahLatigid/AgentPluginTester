#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace agent_plugin_host::plugins
{

struct PluginMetadata
{
    int index = -1;
    juce::String runtimeId;
    juce::File file;
    juce::String formatName;
    juce::String name;
    juce::String descriptiveName;
    juce::String manufacturerName;
    juce::String version;
    juce::String category;
    juce::String uniqueId;
    bool isInstrument = false;
    bool acceptsMidi = false;
    bool producesMidi = false;
    int reportedLatencySamples = 0;
    int inputChannels = 0;
    int outputChannels = 0;

    static PluginMetadata fromDescription (const juce::PluginDescription& description,
                                           int slotIndex,
                                           const juce::String& slotRuntimeId)
    {
        PluginMetadata metadata;
        metadata.index = slotIndex;
        metadata.runtimeId = slotRuntimeId;
        metadata.file = juce::File (description.fileOrIdentifier);
        metadata.formatName = description.pluginFormatName;
        metadata.name = description.name;
        metadata.descriptiveName = description.descriptiveName;
        metadata.manufacturerName = description.manufacturerName;
        metadata.version = description.version;
        metadata.category = description.category;
        metadata.uniqueId = description.createIdentifierString();
        metadata.isInstrument = description.isInstrument;
        metadata.acceptsMidi = description.isInstrument;
        metadata.producesMidi = false;
        metadata.inputChannels = description.numInputChannels;
        metadata.outputChannels = description.numOutputChannels;
        return metadata;
    }
};

} // namespace agent_plugin_host::plugins
