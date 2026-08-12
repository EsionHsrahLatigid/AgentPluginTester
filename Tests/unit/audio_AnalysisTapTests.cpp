#include "../../Source/audio/AudioStatistics.h"

#include <juce_events/juce_events.h>

#include <limits>

namespace agent_plugin_host::audio::tests
{
class AnalysisTapTests final : public juce::UnitTest
{
public:
    AnalysisTapTests() : UnitTest ("AnalysisTap", "audio") {}

    void runTest() override
    {
        testFiniteStatistics();
        testNonFiniteAndClipping();
        testReset();
        testPrepareValidation();
    }

private:
    void testFiniteStatistics()
    {
        beginTest ("captures peak RMS DC silence and zero crossings");

        AnalysisTap tap;
        expect (tap.prepare (48000.0, 1, 1.0e-6f, 1.0f));

        juce::AudioBuffer<float> buffer (1, 6);
        const float samples[] = { 0.0f, 0.5f, -0.5f, 0.0f, 1.0f, -1.0f };
        for (int i = 0; i < 6; ++i)
            buffer.setSample (0, i, samples[i]);

        tap.processBlock (buffer, 6);
        const auto snapshot = tap.snapshot();

        expectEquals (snapshot.channelCount, 1);
        expectEquals (static_cast<int> (snapshot.totalSampleCount), 6);
        expectWithinAbsoluteError (snapshot.absolutePeak, 1.0f, 1.0e-7f);
        expectWithinAbsoluteError (snapshot.dcOffset, 0.0f, 1.0e-7f);
        expectEquals (static_cast<int> (snapshot.clippedSampleCount), 2);
        expectEquals (static_cast<int> (snapshot.silenceSampleCount), 2);
        expectEquals (static_cast<int> (snapshot.zeroCrossingCount), 3);
        expect (! snapshot.isSilent);
    }

    void testNonFiniteAndClipping()
    {
        beginTest ("counts NaN infinity and clipping separately");

        AudioStatistics statistics;
        expect (statistics.prepare (44100.0, 1));

        juce::AudioBuffer<float> buffer (1, 5);
        buffer.setSample (0, 0, std::numeric_limits<float>::quiet_NaN());
        buffer.setSample (0, 1, std::numeric_limits<float>::infinity());
        buffer.setSample (0, 2, -std::numeric_limits<float>::infinity());
        buffer.setSample (0, 3, 1.25f);
        buffer.setSample (0, 4, 0.0f);

        statistics.processBlock (buffer, 5);
        const auto snapshot = statistics.snapshot();

        expect (snapshot.hasNonFinite);
        expectEquals (static_cast<int> (snapshot.nanCount), 1);
        expectEquals (static_cast<int> (snapshot.positiveInfinityCount), 1);
        expectEquals (static_cast<int> (snapshot.negativeInfinityCount), 1);
        expectEquals (static_cast<int> (snapshot.clippedSampleCount), 1);
        expectWithinAbsoluteError (snapshot.absolutePeak, 1.25f, 1.0e-7f);
    }

    void testReset()
    {
        beginTest ("reset clears accumulated data");

        AnalysisTap tap;
        expect (tap.prepare (48000.0, 1));

        juce::AudioBuffer<float> buffer (1, 4);
        buffer.setSample (0, 0, 0.25f);
        tap.processBlock (buffer, 4);
        tap.reset();

        const auto snapshot = tap.snapshot();
        expectEquals (static_cast<int> (snapshot.totalSampleCount), 0);
        expect (! snapshot.hasNonFinite);
    }

    void testPrepareValidation()
    {
        beginTest ("invalid prepare is rejected");

        AnalysisTap tap;
        expect (! tap.prepare (0.0, 1));
        expect (! tap.prepare (48000.0, AudioStatistics::maxChannels + 1));
        expect (! tap.isPrepared());
    }
};

static AnalysisTapTests analysisTapTests;
} // namespace agent_plugin_host::audio::tests
