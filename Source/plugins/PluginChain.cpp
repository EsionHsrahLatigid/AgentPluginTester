#include "PluginChain.h"

namespace agent_plugin_host::plugins
{

PluginChain::~PluginChain()
{
    releaseResources();
}

void PluginChain::clear()
{
    releaseResources();
    slots.clear();
}

int PluginChain::size() const noexcept
{
    return static_cast<int> (slots.size());
}

int PluginChain::addPlugin (std::unique_ptr<juce::AudioPluginInstance> instance,
                            PluginMetadata metadata,
                            bool initiallyBypassed)
{
    jassert (instance != nullptr);

    auto slot = std::make_unique<Slot>();
    slot->index = size();
    slot->runtimeId = metadata.runtimeId.isNotEmpty() ? metadata.runtimeId : juce::Uuid().toString();
    slot->metadata = std::move (metadata);
    slot->metadata.index = slot->index;
    slot->metadata.runtimeId = slot->runtimeId;
    slot->metadata.reportedLatencySamples = instance->getLatencySamples();
    slot->metadata.acceptsMidi = instance->acceptsMidi();
    slot->metadata.producesMidi = instance->producesMidi();
    slot->metadata.inputChannels = instance->getTotalNumInputChannels();
    slot->metadata.outputChannels = instance->getTotalNumOutputChannels();
    slot->instance = std::move (instance);
    slot->bypassed.store (initiallyBypassed, std::memory_order_release);

    if (prepared)
        slot->instance->prepareToPlay (preparedSpec.sampleRate, preparedSpec.maximumBlockSize);

    slots.push_back (std::move (slot));
    return size() - 1;
}

const PluginChain::Slot* PluginChain::getSlot (int index) const noexcept
{
    if (index < 0 || index >= size())
        return nullptr;

    return slots[static_cast<size_t> (index)].get();
}

PluginChain::Slot* PluginChain::getSlot (int index) noexcept
{
    if (index < 0 || index >= size())
        return nullptr;

    return slots[static_cast<size_t> (index)].get();
}

juce::Array<PluginMetadata> PluginChain::getMetadataSnapshot() const
{
    juce::Array<PluginMetadata> snapshot;
    snapshot.ensureStorageAllocated (size());

    for (const auto& slot : slots)
        snapshot.add (slot->metadata);

    return snapshot;
}

bool PluginChain::setBypassed (int index, bool shouldBypass) noexcept
{
    if (auto* slot = getSlot (index))
    {
        slot->bypassed.store (shouldBypass, std::memory_order_release);
        return true;
    }

    return false;
}

bool PluginChain::isBypassed (int index) const noexcept
{
    if (const auto* slot = getSlot (index))
        return slot->bypassed.load (std::memory_order_acquire);

    return false;
}

bool PluginChain::setEditorRequested (int index, bool shouldShow) noexcept
{
    if (auto* slot = getSlot (index))
    {
        slot->editorRequested.store (shouldShow, std::memory_order_release);
        return true;
    }

    return false;
}

std::unique_ptr<juce::AudioProcessorEditor> PluginChain::createEditorForSlot (int index)
{
    if (auto* slot = getSlot (index))
    {
        if (slot->instance == nullptr)
            return {};

        if (slot->instance->hasEditor())
            return std::unique_ptr<juce::AudioProcessorEditor> (slot->instance->createEditorAndMakeActive());

        return std::make_unique<juce::GenericAudioProcessorEditor> (*slot->instance);
    }

    return {};
}

void PluginChain::prepareToPlay (double sampleRate, int maximumBlockSize, int channels)
{
    preparedSpec.sampleRate = sampleRate;
    preparedSpec.maximumBlockSize = maximumBlockSize;
    preparedSpec.channels = channels;

    for (auto& slot : slots)
    {
        if (slot->instance != nullptr)
            slot->instance->prepareToPlay (sampleRate, maximumBlockSize);
    }

    prepared = true;
}

void PluginChain::releaseResources()
{
    for (auto& slot : slots)
    {
        if (slot->instance != nullptr)
            slot->instance->releaseResources();
    }

    prepared = false;
    preparedSpec = {};
}

void PluginChain::reset()
{
    for (auto& slot : slots)
    {
        if (slot->instance != nullptr)
            slot->instance->reset();
    }
}

void PluginChain::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept
{
    processBlockInternal (buffer, midi);
}

void PluginChain::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi) noexcept
{
    processBlockInternal (buffer, midi);
}

int PluginChain::getTotalLatencySamples() const noexcept
{
    int total = 0;

    for (const auto& slot : slots)
    {
        if (slot->instance != nullptr)
            total += slot->instance->getLatencySamples();
    }

    return total;
}

template <typename SampleType>
void PluginChain::processBlockInternal (juce::AudioBuffer<SampleType>& buffer, juce::MidiBuffer& midi) noexcept
{
    const auto numSamples = buffer.getNumSamples();

    if (! prepared || numSamples > preparedSpec.maximumBlockSize)
    {
        buffer.clear();
        midi.clear();
        return;
    }

    for (auto& slot : slots)
    {
        auto* instance = slot->instance.get();
        if (instance == nullptr)
            continue;

        if (slot->bypassed.load (std::memory_order_acquire))
            continue;

        instance->processBlock (buffer, midi);
    }
}

template void PluginChain::processBlockInternal<float> (juce::AudioBuffer<float>&, juce::MidiBuffer&) noexcept;
template void PluginChain::processBlockInternal<double> (juce::AudioBuffer<double>&, juce::MidiBuffer&) noexcept;

} // namespace agent_plugin_host::plugins
