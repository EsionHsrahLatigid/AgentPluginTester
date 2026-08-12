#include "TransportModel.h"

#include <algorithm>
#include <cmath>

namespace agentpluginhost::transport
{
void TransportModel::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate_ = std::isfinite (newSampleRate) && newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maximumBlockSize_ = std::max (0, maximumBlockSize);
    reset();
    prepared_ = true;
}

void TransportModel::reset() noexcept
{
    bpm_ = 120.0;
    timeSigNumerator_ = 4;
    timeSigDenominator_ = 4;
    samplePosition_ = 0;
    loop_ = {};
    playing_ = true;
    recording_ = false;
    looping_ = false;
}

juce::Optional<juce::AudioPlayHead::PositionInfo> TransportModel::getPosition() const
{
    PositionInfo info;
    const juce::AudioPlayHead::TimeSignature timeSignature { timeSigNumerator_, timeSigDenominator_ };
    const juce::AudioPlayHead::LoopPoints loopPoints { ppqForSample (loop_.startSample),
                                                       ppqForSample (loop_.endSample) };

    info.setIsPlaying (playing_);
    info.setIsRecording (recording_);
    info.setBpm (bpm_);
    info.setTimeSignature (timeSignature);
    info.setTimeInSamples (samplePosition_);
    info.setTimeInSeconds (getSecondsPosition());
    info.setPpqPosition (getPpqPosition());
    info.setLoopPoints (loopPoints);
    info.setIsLooping (looping_);
    return info;
}

void TransportModel::setPlaying (bool shouldPlay) noexcept
{
    playing_ = shouldPlay;

    if (! playing_)
        recording_ = false;
}

void TransportModel::setRecording (bool shouldRecord) noexcept
{
    recording_ = shouldRecord;

    if (recording_)
        playing_ = true;
}

void TransportModel::setBpm (double newBpm) noexcept
{
    if (std::isfinite (newBpm) && newBpm > 0.0)
        bpm_ = newBpm;
}

void TransportModel::setTimeSignature (int numerator, int denominator) noexcept
{
    if (numerator > 0)
        timeSigNumerator_ = numerator;

    if (denominator > 0)
        timeSigDenominator_ = denominator;
}

void TransportModel::setLoopRangeSamples (int64_t startSample, int64_t endSample) noexcept
{
    loop_.startSample = std::max<int64_t> (0, startSample);
    loop_.endSample = std::max (loop_.startSample, endSample);
    looping_ = looping_ && loop_.endSample > loop_.startSample;
    samplePosition_ = wrapLoopPosition (samplePosition_);
}

void TransportModel::seekSamples (int64_t samplePosition) noexcept
{
    samplePosition_ = wrapLoopPosition (std::max<int64_t> (0, samplePosition));
}

void TransportModel::advanceBlock (int numSamples) noexcept
{
    if (! playing_ || numSamples <= 0)
        return;

    samplePosition_ = wrapLoopPosition (samplePosition_ + static_cast<int64_t> (numSamples));
}

double TransportModel::getSecondsPosition() const noexcept
{
    return static_cast<double> (samplePosition_) / sampleRate_;
}

double TransportModel::getPpqPosition() const noexcept
{
    return ppqForSample (samplePosition_);
}

int64_t TransportModel::wrapLoopPosition (int64_t position) const noexcept
{
    if (! looping_ || loop_.endSample <= loop_.startSample)
        return position;

    const auto length = loop_.endSample - loop_.startSample;
    if (length <= 0 || position < loop_.endSample)
        return position;

    return loop_.startSample + ((position - loop_.startSample) % length);
}

double TransportModel::ppqForSample (int64_t sample) const noexcept
{
    const auto beatsPerSecond = bpm_ / 60.0;
    return (static_cast<double> (sample) / sampleRate_) * beatsPerSecond;
}
} // namespace agentpluginhost::transport
