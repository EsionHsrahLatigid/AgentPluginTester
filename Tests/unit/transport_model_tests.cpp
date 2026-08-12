#include "../../Source/transport/TransportModel.h"

#include <juce_core/juce_core.h>

namespace agentpluginhost::tests
{
class TransportModelTest final : public juce::UnitTest
{
public:
    TransportModelTest() : juce::UnitTest ("TransportModel", "transport") {}

    void runTest() override
    {
        testDefaultsAndPositionMath();
        testStoppedTransportDoesNotAdvance();
        testRecordingImpliesPlayback();
        testLoopWrapping();
        testValidationKeepsLastGoodValues();
    }

private:
    void testDefaultsAndPositionMath()
    {
        beginTest ("defaults and position calculations");

        transport::TransportModel transport;
        transport.prepare (48000.0, 512);

        auto position = transport.getPosition();
        expect (position.hasValue());
        expect (position->getIsPlaying());
        expect (! position->getIsRecording());
        expectEquals (position->getBpm().orFallback (0.0), 120.0);
        expectEquals (position->getTimeInSamples().orFallback (-1), static_cast<int64_t> (0));

        transport.advanceBlock (24000);
        position = transport.getPosition();
        expectEquals (position->getTimeInSamples().orFallback (-1), static_cast<int64_t> (24000));
        expectWithinAbsoluteError (position->getTimeInSeconds().orFallback (-1.0), 0.5, 0.000001);
        expectWithinAbsoluteError (position->getPpqPosition().orFallback (-1.0), 1.0, 0.000001);
    }

    void testStoppedTransportDoesNotAdvance()
    {
        beginTest ("stopped transport does not advance");

        transport::TransportModel transport;
        transport.prepare (48000.0, 512);
        transport.setPlaying (false);
        transport.advanceBlock (512);

        expectEquals (transport.getSamplePosition(), static_cast<int64_t> (0));
        expect (! transport.getPosition()->getIsPlaying());
    }

    void testLoopWrapping()
    {
        beginTest ("looping wraps sample position at loop end");

        transport::TransportModel transport;
        transport.prepare (48000.0, 512);
        transport.setLoopRangeSamples (100, 200);
        transport.setLooping (true);
        transport.seekSamples (190);
        transport.advanceBlock (25);

        expectEquals (transport.getSamplePosition(), static_cast<int64_t> (115));
        const auto position = transport.getPosition();
        expect (position->getIsLooping());
        expectWithinAbsoluteError (position->getLoopPoints().orFallback (juce::AudioPlayHead::LoopPoints{}).ppqStart,
                                   100.0 / 48000.0 * 2.0,
                                   0.000001);
    }

    void testRecordingImpliesPlayback()
    {
        beginTest ("recording implies playback and stopping clears recording");

        transport::TransportModel transport;
        transport.prepare (48000.0, 512);
        transport.setPlaying (false);
        transport.setRecording (true);

        expect (transport.isPlaying());
        expect (transport.isRecording());

        transport.setPlaying (false);

        expect (! transport.isPlaying());
        expect (! transport.isRecording());
    }

    void testValidationKeepsLastGoodValues()
    {
        beginTest ("invalid tempo and time signature values are ignored");

        transport::TransportModel transport;
        transport.prepare (48000.0, 512);
        transport.setBpm (90.0);
        transport.setBpm (-1.0);
        transport.setTimeSignature (7, 8);
        transport.setTimeSignature (0, 0);

        expectEquals (transport.getBpm(), 90.0);
        expectEquals (transport.getTimeSigNumerator(), 7);
        expectEquals (transport.getTimeSigDenominator(), 8);
    }
};

static TransportModelTest transportModelTest;
} // namespace agentpluginhost::tests
