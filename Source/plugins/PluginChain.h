#pragma once

#include "PluginMetadata.h"

#include <atomic>
#include <memory>
#include <vector>

namespace agent_plugin_host::plugins
{

class PluginChain
{
public:
    struct Slot
    {
        int index = -1;
        juce::String runtimeId;
        PluginMetadata metadata;
        std::unique_ptr<juce::AudioPluginInstance> instance;
        std::atomic<bool> bypassed { false };
        std::atomic<bool> editorRequested { false };
    };

    struct PreparedSpec
    {
        double sampleRate = 0.0;
        int maximumBlockSize = 0;
        int channels = 0;
    };

    PluginChain() = default;
    ~PluginChain();

    PluginChain (const PluginChain&) = delete;
    PluginChain& operator= (const PluginChain&) = delete;

    void clear();
    int size() const noexcept;
    bool isPrepared() const noexcept { return prepared; }
    PreparedSpec getPreparedSpec() const noexcept { return preparedSpec; }

    int addPlugin (std::unique_ptr<juce::AudioPluginInstance> instance,
                   PluginMetadata metadata,
                   bool initiallyBypassed = false);

    const Slot* getSlot (int index) const noexcept;
    Slot* getSlot (int index) noexcept;
    juce::Array<PluginMetadata> getMetadataSnapshot() const;

    bool setBypassed (int index, bool shouldBypass) noexcept;
    bool isBypassed (int index) const noexcept;
    bool setEditorRequested (int index, bool shouldShow) noexcept;

    std::unique_ptr<juce::AudioProcessorEditor> createEditorForSlot (int index);

    void refreshMetadataFromInstances() noexcept;
    void prepareToPlay (double sampleRate, int maximumBlockSize, int channels);
    void releaseResources();
    void reset();

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept;
    void processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi) noexcept;

    int getTotalLatencySamples() const noexcept;

private:
    template <typename SampleType>
    void processBlockInternal (juce::AudioBuffer<SampleType>& buffer, juce::MidiBuffer& midi) noexcept;

    std::vector<std::unique_ptr<Slot>> slots;
    PreparedSpec preparedSpec;
    bool prepared = false;
};

} // namespace agent_plugin_host::plugins
