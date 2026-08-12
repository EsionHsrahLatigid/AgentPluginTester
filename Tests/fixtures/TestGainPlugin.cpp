#include <juce_audio_processors/juce_audio_processors.h>

class TestGainProcessor final : public juce::AudioProcessor
{
public:
    TestGainProcessor()
        : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          parameters (*this, nullptr, "state", createLayout())
    {
        gain = parameters.getRawParameterValue ("gain");
    }

    const juce::String getName() const override { return "APH Test Gain"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layout) const override
    {
        return layout.getMainInputChannelSet() == layout.getMainOutputChannelSet()
            && (layout.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
                || layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
    }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.applyGain (gain->load (std::memory_order_relaxed));
    }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    void getStateInformation (juce::MemoryBlock& destination) override
    {
        if (auto xml = parameters.copyState().createXml()) copyXmlToBinary (*xml, destination);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size)) parameters.replaceState (juce::ValueTree::fromXml (*xml));
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "gain", 1 }, "Gain", juce::NormalisableRange<float> { 0.0f, 2.0f }, 0.5f));
        return layout;
    }

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* gain = nullptr;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TestGainProcessor(); }
