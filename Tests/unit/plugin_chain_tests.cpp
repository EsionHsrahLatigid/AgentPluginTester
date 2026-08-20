#include "../../Source/plugins/PluginChain.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace agent_plugin_host::plugins::tests
{

class GainProcessor final : public juce::AudioPluginInstance
{
public:
    explicit GainProcessor (float gainToApply, int latencyToReportAfterPrepare = 0)
        : gain (gainToApply),
          preparedLatencySamples (latencyToReportAfterPrepare)
    {
    }

    const juce::String getName() const override { return "GainProcessor"; }
    void fillInPluginDescription (juce::PluginDescription& description) const override
    {
        description.name = "GainProcessor";
        description.pluginFormatName = "Internal";
        description.manufacturerName = "AgentPluginHostTests";
        description.version = "1.0";
        description.fileOrIdentifier = "internal://gain";
        description.numInputChannels = getTotalNumInputChannels();
        description.numOutputChannels = getTotalNumOutputChannels();
    }

    void prepareToPlay (double, int) override { setLatencySamples (preparedLatencySamples); }
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.applyGain (gain); }
    void processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&) override { buffer.applyGain (static_cast<double> (gain)); }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    float gain = 1.0f;
    int preparedLatencySamples = 0;
};

class PluginChainUnitTests final : public juce::UnitTest
{
public:
    PluginChainUnitTests() : juce::UnitTest ("PluginChain", "plugins") {}

    void runTest() override
    {
        beginTest ("processes plugins in insertion order");
        {
            PluginChain chain;
            chain.addPlugin (std::make_unique<GainProcessor> (2.0f), makeMetadata ("first"));
            chain.addPlugin (std::make_unique<GainProcessor> (4.0f), makeMetadata ("second"));
            chain.prepareToPlay (48000.0, 64, 2);

            juce::AudioBuffer<float> buffer (2, 16);
            buffer.clear();
            buffer.setSample (0, 0, 0.25f);
            buffer.setSample (1, 0, 0.5f);
            juce::MidiBuffer midi;

            chain.processBlock (buffer, midi);

            expectWithinAbsoluteError (buffer.getSample (0, 0), 2.0f, 0.000001f);
            expectWithinAbsoluteError (buffer.getSample (1, 0), 4.0f, 0.000001f);
        }

        beginTest ("bypass preserves audio while keeping MIDI path active");
        {
            PluginChain chain;
            chain.addPlugin (std::make_unique<GainProcessor> (8.0f), makeMetadata ("bypassed"), true);
            chain.prepareToPlay (48000.0, 64, 2);

            juce::AudioBuffer<float> buffer (2, 16);
            buffer.clear();
            buffer.setSample (0, 0, 0.25f);
            buffer.setSample (1, 0, -0.25f);

            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.75f), 3);

            chain.processBlock (buffer, midi);

            expectWithinAbsoluteError (buffer.getSample (0, 0), 0.25f, 0.000001f);
            expectWithinAbsoluteError (buffer.getSample (1, 0), -0.25f, 0.000001f);
            expectEquals (midi.getNumEvents(), 1);
        }

        beginTest ("metadata latency refreshes after prepareToPlay");
        {
            PluginChain chain;
            chain.addPlugin (std::make_unique<GainProcessor> (1.0f, 137), makeMetadata ("latency"));

            expectEquals (chain.getMetadataSnapshot()[0].reportedLatencySamples, 0);

            chain.prepareToPlay (48000.0, 64, 2);

            expectEquals (chain.getMetadataSnapshot()[0].reportedLatencySamples, 137);
        }

        beginTest ("plugins added to a prepared chain are ready on the next block");
        {
            PluginChain chain;
            chain.addPlugin (std::make_unique<GainProcessor> (2.0f), makeMetadata ("first"));
            chain.prepareToPlay (48000.0, 64, 2);
            chain.addPlugin (std::make_unique<GainProcessor> (3.0f), makeMetadata ("added"));

            juce::AudioBuffer<float> buffer (2, 16);
            buffer.clear();
            buffer.setSample (0, 0, 0.25f);
            juce::MidiBuffer midi;
            chain.processBlock (buffer, midi);

            expectWithinAbsoluteError (buffer.getSample (0, 0), 1.5f, 0.000001f);
            expect (chain.isPrepared());
            expectEquals (chain.size(), 2);
        }
    }

private:
    static PluginMetadata makeMetadata (const juce::String& name)
    {
        PluginMetadata metadata;
        metadata.name = name;
        metadata.runtimeId = juce::Uuid().toString();
        return metadata;
    }
};

static PluginChainUnitTests pluginChainUnitTests;

} // namespace agent_plugin_host::plugins::tests
