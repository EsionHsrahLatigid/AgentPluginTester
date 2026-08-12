#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

class TestSynthProcessor final : public juce::AudioProcessor
{
public:
    TestSynthProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "APH Test Synth"; }
    void prepareToPlay (double newSampleRate, int) override { sampleRate = juce::jmax (1.0, newSampleRate); reset(); }
    void releaseResources() override {}
    void reset() override { active.fill (false); phases.fill (0.0); velocities.fill (0.0f); }
    bool isBusesLayoutSupported (const BusesLayout& layout) const override
    {
        return layout.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
            || layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();
        int cursor = 0;
        for (const auto metadata : midi)
        {
            const auto position = juce::jlimit (cursor, buffer.getNumSamples(), metadata.samplePosition);
            render (buffer, cursor, position);
            handleMidi (metadata.getMessage());
            cursor = position;
        }
        render (buffer, cursor, buffer.getNumSamples());
    }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    void handleMidi (const juce::MidiMessage& message) noexcept
    {
        if (message.isNoteOn())
        {
            const auto note = message.getNoteNumber();
            active[static_cast<std::size_t> (note)] = true;
            velocities[static_cast<std::size_t> (note)] = message.getFloatVelocity();
        }
        else if (message.isNoteOff())
        {
            active[static_cast<std::size_t> (message.getNoteNumber())] = false;
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            active.fill (false);
        }
    }

    void render (juce::AudioBuffer<float>& buffer, int begin, int end) noexcept
    {
        constexpr auto twoPi = juce::MathConstants<double>::twoPi;
        for (int sample = begin; sample < end; ++sample)
        {
            double value = 0.0;
            for (int note = 0; note < 128; ++note)
            {
                if (! active[static_cast<std::size_t> (note)]) continue;
                value += std::sin (phases[static_cast<std::size_t> (note)])
                       * velocities[static_cast<std::size_t> (note)] * 0.12;
                phases[static_cast<std::size_t> (note)] += twoPi * juce::MidiMessage::getMidiNoteInHertz (note) / sampleRate;
                if (phases[static_cast<std::size_t> (note)] >= twoPi) phases[static_cast<std::size_t> (note)] -= twoPi;
            }
            const auto output = static_cast<float> (juce::jlimit (-1.0, 1.0, value));
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample (channel, sample, output);
        }
    }

    double sampleRate = 48000.0;
    std::array<bool, 128> active {};
    std::array<double, 128> phases {};
    std::array<float, 128> velocities {};
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TestSynthProcessor(); }
