#pragma once

#include "PluginMetadata.h"

#include <functional>
#include <memory>

namespace agent_plugin_host::plugins
{

struct PluginScanResult
{
    juce::File path;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    juce::String error;

    bool succeeded() const noexcept { return error.isEmpty() && descriptions.size() > 0; }
};

struct PluginLoadResult
{
    std::unique_ptr<juce::AudioPluginInstance> instance;
    PluginMetadata metadata;
    juce::String error;

    bool succeeded() const noexcept { return instance != nullptr && error.isEmpty(); }
};

class PluginLoader
{
public:
    using AsyncLoadCallback = std::function<void (PluginLoadResult)>;

    PluginLoader();

    static bool isSupportedPluginPath (const juce::String& path);

    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }

    PluginScanResult scanSinglePluginFile (const juce::File& pluginPath,
                                           const juce::File& deadManMarker = {}) const;

    PluginLoadResult createInstanceBlocking (const juce::PluginDescription& description,
                                             double sampleRate,
                                             int maximumBlockSize,
                                             int slotIndex = -1) const;

    void createInstanceAsync (const juce::PluginDescription& description,
                              double sampleRate,
                              int maximumBlockSize,
                              int slotIndex,
                              AsyncLoadCallback callback) const;

private:
    static juce::String validatePluginPath (const juce::File& pluginPath);
    static void writeDeadManMarker (const juce::File& markerFile, const juce::File& pluginPath);
    static PluginMetadata metadataFor (const juce::PluginDescription& description,
                                       int slotIndex,
                                       const juce::String& runtimeId,
                                       const juce::AudioPluginInstance* instance);

    mutable juce::AudioPluginFormatManager formatManager;
};

} // namespace agent_plugin_host::plugins
