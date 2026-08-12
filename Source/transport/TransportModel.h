#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cstdint>

namespace agentpluginhost::transport
{
class TransportModel final : public juce::AudioPlayHead
{
public:
    struct LoopRange
    {
        int64_t startSample = 0;
        int64_t endSample = 0;
    };

    void prepare (double newSampleRate, int maximumBlockSize);
    void reset() noexcept;

    juce::Optional<PositionInfo> getPosition() const override;

    void setPlaying (bool shouldPlay) noexcept;
    void setRecording (bool shouldRecord) noexcept;
    void setBpm (double newBpm) noexcept;
    void setTimeSignature (int numerator, int denominator) noexcept;
    void setLooping (bool shouldLoop) noexcept { looping_ = shouldLoop && loop_.endSample > loop_.startSample; }
    void setLoopRangeSamples (int64_t startSample, int64_t endSample) noexcept;
    void seekSamples (int64_t samplePosition) noexcept;
    void advanceBlock (int numSamples) noexcept;

    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] bool isPlaying() const noexcept { return playing_; }
    [[nodiscard]] bool isRecording() const noexcept { return recording_; }
    [[nodiscard]] bool isLooping() const noexcept { return looping_; }
    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] double getBpm() const noexcept { return bpm_; }
    [[nodiscard]] int getTimeSigNumerator() const noexcept { return timeSigNumerator_; }
    [[nodiscard]] int getTimeSigDenominator() const noexcept { return timeSigDenominator_; }
    [[nodiscard]] int64_t getSamplePosition() const noexcept { return samplePosition_; }
    [[nodiscard]] double getSecondsPosition() const noexcept;
    [[nodiscard]] double getPpqPosition() const noexcept;
    [[nodiscard]] LoopRange getLoopRangeSamples() const noexcept { return loop_; }

private:
    [[nodiscard]] int64_t wrapLoopPosition (int64_t position) const noexcept;
    [[nodiscard]] double ppqForSample (int64_t sample) const noexcept;

    double sampleRate_ = 44100.0;
    int maximumBlockSize_ = 0;
    double bpm_ = 120.0;
    int timeSigNumerator_ = 4;
    int timeSigDenominator_ = 4;
    int64_t samplePosition_ = 0;
    LoopRange loop_;
    bool playing_ = true;
    bool recording_ = false;
    bool looping_ = false;
    bool prepared_ = false;
};
} // namespace agentpluginhost::transport
