#include "../../Source/capture/CaptureWriter.h"
#include <juce_audio_formats/juce_audio_formats.h>

class CaptureWriterTest final : public juce::UnitTest
{
public:
    CaptureWriterTest() : juce::UnitTest ("CaptureWriter", "capture") {}

    void runTest() override
    {
        beginTest ("writes finalized 32-bit float WAV off the caller thread");
        auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getNonexistentChildFile ("aph-capture-test", {}, false);
        expect (directory.createDirectory());
        auto output = directory.getChildFile ("capture.wav");

        aph::capture::CaptureWriter writer;
        expect (writer.start (output, 48000.0, 2, 4096).wasOk());
        juce::AudioBuffer<float> block (2, 256);
        block.clear();
        block.setSample (0, 0, 0.5f);
        block.setSample (1, 0, -0.5f);
        expect (writer.push (block));
        expect (writer.stop().wasOk());
        expectEquals (writer.getSamplesAccepted(), static_cast<std::int64_t> (256));

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (output));
        expect (reader != nullptr);
        if (reader != nullptr)
        {
            expectEquals (static_cast<int> (reader->numChannels), 2);
            expectEquals (reader->lengthInSamples, static_cast<juce::int64> (256));
            expectWithinAbsoluteError (reader->sampleRate, 48000.0, 0.01);
        }
        output.deleteFile();
        directory.deleteRecursively();
    }
};

static CaptureWriterTest captureWriterTest;
