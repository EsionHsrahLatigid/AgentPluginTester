#include "../../Source/audio/SourceEngine.h"

#include <juce_events/juce_events.h>

#include <algorithm>

namespace agent_plugin_host::audio::tests
{
class SourceEngineTests final : public juce::UnitTest
{
public:
    SourceEngineTests() : UnitTest ("SourceEngine", "audio") {}

    void runTest() override
    {
        testSilence();
        testSineDeterminism();
        testWhiteNoiseDeterminism();
        testImpulseRepeat();
        testFileHook();
        testInvalidPrepare();
    }

private:
    class FixedFileSource final : public SourceEngine::FileSource
    {
    public:
        void reset() noexcept override { sample = 0; }

        void renderNextBlock (juce::AudioBuffer<float>& destination, int numSamples) noexcept override
        {
            const auto samplesToWrite = std::min (numSamples, destination.getNumSamples());
            for (int i = 0; i < samplesToWrite; ++i)
            {
                const auto value = static_cast<float> (++sample) * 0.01f;
                for (int channel = 0; channel < destination.getNumChannels(); ++channel)
                    destination.setSample (channel, i, value);
            }
        }

        int sample = 0;
    };

    void testSilence()
    {
        beginTest ("silence clears destination");

        SourceEngine engine;
        SourceEngine::Config config;
        config.type = SourceType::silence;
        config.channels = 2;
        config.maxBlockSize = 8;
        expect (engine.prepare (config));

        juce::AudioBuffer<float> buffer (2, 8);
        buffer.setSample (0, 0, 1.0f);
        engine.renderNextBlock (buffer, 8);

        expectEquals (buffer.getMagnitude (0, 8), 0.0f);
    }

    void testSineDeterminism()
    {
        beginTest ("sine is deterministic across reset");

        SourceEngine engine;
        SourceEngine::Config config;
        config.type = SourceType::sine;
        config.sampleRate = 48000.0;
        config.frequencyHz = 1000.0f;
        config.levelDb = 0.0f;
        config.maxBlockSize = 16;
        expect (engine.prepare (config));

        juce::AudioBuffer<float> first (2, 16);
        juce::AudioBuffer<float> second (2, 16);
        engine.renderNextBlock (first, 16);
        engine.reset();
        engine.renderNextBlock (second, 16);

        for (int i = 0; i < 16; ++i)
        {
            expectWithinAbsoluteError (first.getSample (0, i), second.getSample (0, i), 1.0e-7f);
            expectWithinAbsoluteError (first.getSample (0, i), first.getSample (1, i), 1.0e-7f);
        }
    }

    void testWhiteNoiseDeterminism()
    {
        beginTest ("white noise uses fixed seed");

        SourceEngine a;
        SourceEngine b;
        SourceEngine::Config config;
        config.type = SourceType::whiteNoise;
        config.levelDb = -6.0f;
        config.seed = 1234;
        config.maxBlockSize = 32;
        expect (a.prepare (config));
        expect (b.prepare (config));

        juce::AudioBuffer<float> bufferA (2, 32);
        juce::AudioBuffer<float> bufferB (2, 32);
        a.renderNextBlock (bufferA, 32);
        b.renderNextBlock (bufferB, 32);

        for (int i = 0; i < 32; ++i)
            expectWithinAbsoluteError (bufferA.getSample (0, i), bufferB.getSample (0, i), 1.0e-7f);
    }

    void testImpulseRepeat()
    {
        beginTest ("impulse emits at configured positions");

        SourceEngine engine;
        SourceEngine::Config config;
        config.type = SourceType::impulse;
        config.levelDb = 0.0f;
        config.impulseSampleOffset = 2;
        config.impulseRepeatInterval = 4;
        config.maxBlockSize = 10;
        expect (engine.prepare (config));

        juce::AudioBuffer<float> buffer (1, 10);
        engine.renderNextBlock (buffer, 10);

        expectEquals (buffer.getSample (0, 0), 0.0f);
        expectEquals (buffer.getSample (0, 2), 1.0f);
        expectEquals (buffer.getSample (0, 6), 1.0f);
    }

    void testFileHook()
    {
        beginTest ("file source delegates to hook");

        SourceEngine engine;
        FixedFileSource file;
        SourceEngine::Config config;
        config.type = SourceType::file;
        config.maxBlockSize = 4;
        expect (engine.prepare (config));
        engine.setFileSource (&file);

        juce::AudioBuffer<float> buffer (1, 4);
        engine.renderNextBlock (buffer, 4);

        expectWithinAbsoluteError (buffer.getSample (0, 0), 0.01f, 1.0e-7f);
        expectWithinAbsoluteError (buffer.getSample (0, 3), 0.04f, 1.0e-7f);
    }

    void testInvalidPrepare()
    {
        beginTest ("invalid prepare is rejected");

        SourceEngine engine;
        SourceEngine::Config config;
        config.sampleRate = 0.0;
        expect (! engine.prepare (config));
        expect (! engine.isPrepared());
    }
};

static SourceEngineTests sourceEngineTests;
} // namespace agent_plugin_host::audio::tests
