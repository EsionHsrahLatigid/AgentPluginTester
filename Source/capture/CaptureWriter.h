#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>

namespace aph::capture
{
class CaptureWriter final
{
public:
    CaptureWriter();
    ~CaptureWriter();

    juce::Result start (const juce::File& destination,
                        double sampleRate,
                        int channelCount,
                        int fifoSamples = 262144);
    bool push (const juce::AudioBuffer<float>& buffer) noexcept;
    juce::Result stop();

    bool isRecording() const noexcept { return active.load (std::memory_order_acquire); }
    std::uint64_t getOverflowCount() const noexcept { return overflowCount.load(); }
    std::int64_t getSamplesAccepted() const noexcept { return samplesAccepted.load(); }
    juce::File getDestination() const { return finalFile; }

private:
    juce::TimeSliceThread writerThread { "AgentPluginHost capture" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::File finalFile;
    juce::File temporaryFile;
    std::atomic<bool> active { false };
    std::atomic<std::uint64_t> overflowCount { 0 };
    std::atomic<std::int64_t> samplesAccepted { 0 };
    int channels = 0;
};
} // namespace aph::capture
