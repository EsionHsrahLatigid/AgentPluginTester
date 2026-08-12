#include "AudioStatistics.h"

#include <algorithm>
#include <cmath>

namespace agent_plugin_host::audio
{
bool AudioStatistics::prepare (double newSampleRate, int newChannelCount, float newSilenceThreshold,
                               float newClipThreshold) noexcept
{
    if (! std::isfinite (newSampleRate) || newSampleRate <= 0.0 || newChannelCount < 0 || newChannelCount > maxChannels
        || ! std::isfinite (newSilenceThreshold) || newSilenceThreshold < 0.0f
        || ! std::isfinite (newClipThreshold) || newClipThreshold <= 0.0f)
    {
        return false;
    }

    sampleRate = newSampleRate;
    channelCount = newChannelCount;
    silenceThreshold = newSilenceThreshold;
    clipThreshold = newClipThreshold;
    prepared = true;
    reset();
    return true;
}

void AudioStatistics::reset() noexcept
{
    for (auto& accumulator : accumulators)
        accumulator.reset();
}

void AudioStatistics::processBlock (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (! prepared || numSamples <= 0)
        return;

    const auto channelsToRead = std::min (channelCount, buffer.getNumChannels());
    const auto samplesToRead = std::min (numSamples, buffer.getNumSamples());

    for (int channel = 0; channel < channelsToRead; ++channel)
    {
        auto& accumulator = accumulators[static_cast<std::size_t> (channel)];
        const auto* samples = buffer.getReadPointer (channel);

        for (int sampleIndex = 0; sampleIndex < samplesToRead; ++sampleIndex)
        {
            const auto sample = samples[sampleIndex];
            ++accumulator.sampleCount;

            if (std::isnan (sample))
            {
                ++accumulator.nanCount;
                continue;
            }

            if (std::isinf (sample))
            {
                sample > 0.0f ? ++accumulator.positiveInfinityCount : ++accumulator.negativeInfinityCount;
                continue;
            }

            const auto absolute = std::abs (sample);
            ++accumulator.finiteSampleCount;
            accumulator.sum += sample;
            accumulator.sumSquares += static_cast<double> (sample) * static_cast<double> (sample);
            accumulator.minimum = std::min (accumulator.minimum, sample);
            accumulator.maximum = std::max (accumulator.maximum, sample);
            accumulator.absolutePeak = std::max (accumulator.absolutePeak, absolute);

            if (absolute >= clipThreshold)
                ++accumulator.clippedSampleCount;

            if (absolute <= silenceThreshold)
            {
                ++accumulator.silenceSampleCount;
                ++accumulator.currentContinuousSilenceSamples;
                accumulator.maxContinuousSilenceSamples = std::max (accumulator.maxContinuousSilenceSamples,
                                                                    accumulator.currentContinuousSilenceSamples);
            }
            else
            {
                accumulator.currentContinuousSilenceSamples = 0;
            }

            if (accumulator.hasPreviousFiniteSample
                && ((sample > 0.0f && accumulator.previousFiniteSample < 0.0f)
                    || (sample < 0.0f && accumulator.previousFiniteSample > 0.0f)))
            {
                ++accumulator.zeroCrossingCount;
            }

            if (sample != 0.0f)
            {
                accumulator.previousFiniteSample = sample;
                accumulator.hasPreviousFiniteSample = true;
            }
        }
    }
}

AudioStatisticsSnapshot AudioStatistics::snapshot() const noexcept
{
    AudioStatisticsSnapshot result;
    result.channelCount = channelCount;
    result.sampleRate = sampleRate;

    double aggregateSquares = 0.0;
    double aggregateSum = 0.0;

    for (int channel = 0; channel < channelCount; ++channel)
    {
        const auto& accumulator = accumulators[static_cast<std::size_t> (channel)];
        auto channelSnapshot = makeChannelSnapshot (accumulator);
        result.channels[static_cast<std::size_t> (channel)] = channelSnapshot;

        result.totalSampleCount += accumulator.sampleCount;
        result.absolutePeak = std::max (result.absolutePeak, channelSnapshot.absolutePeak);
        result.clippedSampleCount += channelSnapshot.clippedSampleCount;
        result.nanCount += channelSnapshot.nanCount;
        result.positiveInfinityCount += channelSnapshot.positiveInfinityCount;
        result.negativeInfinityCount += channelSnapshot.negativeInfinityCount;
        result.silenceSampleCount += channelSnapshot.silenceSampleCount;
        result.maxContinuousSilenceSamples = std::max (result.maxContinuousSilenceSamples,
                                                       channelSnapshot.maxContinuousSilenceSamples);
        result.zeroCrossingCount += channelSnapshot.zeroCrossingCount;
        aggregateSquares += accumulator.sumSquares;
        aggregateSum += accumulator.sum;
    }

    if (result.totalSampleCount > 0)
    {
        result.isSilent = result.silenceSampleCount == result.totalSampleCount;
    }

    std::uint64_t aggregateFiniteSampleCount = 0;
    for (int channel = 0; channel < channelCount; ++channel)
        aggregateFiniteSampleCount += accumulators[static_cast<std::size_t> (channel)].finiteSampleCount;

    if (aggregateFiniteSampleCount > 0)
    {
        result.rms = static_cast<float> (std::sqrt (aggregateSquares
                                                   / static_cast<double> (aggregateFiniteSampleCount)));
        result.rmsDbfs = gainToDb (result.rms);
        result.dcOffset = static_cast<float> (aggregateSum / static_cast<double> (aggregateFiniteSampleCount));
    }

    result.peakDbfs = gainToDb (result.absolutePeak);
    result.hasNonFinite = result.nanCount > 0 || result.positiveInfinityCount > 0 || result.negativeInfinityCount > 0;
    return result;
}

void AudioStatistics::ChannelAccumulator::reset() noexcept
{
    sampleCount = 0;
    finiteSampleCount = 0;
    sum = 0.0;
    sumSquares = 0.0;
    minimum = std::numeric_limits<float>::infinity();
    maximum = -std::numeric_limits<float>::infinity();
    absolutePeak = 0.0f;
    clippedSampleCount = 0;
    nanCount = 0;
    positiveInfinityCount = 0;
    negativeInfinityCount = 0;
    silenceSampleCount = 0;
    currentContinuousSilenceSamples = 0;
    maxContinuousSilenceSamples = 0;
    zeroCrossingCount = 0;
    previousFiniteSample = 0.0f;
    hasPreviousFiniteSample = false;
}

AudioStatisticsSnapshot::Channel AudioStatistics::makeChannelSnapshot (const ChannelAccumulator& accumulator) noexcept
{
    AudioStatisticsSnapshot::Channel result;
    result.sampleCount = accumulator.sampleCount;
    result.minimum = accumulator.sampleCount == 0 ? 0.0f : accumulator.minimum;
    result.maximum = accumulator.sampleCount == 0 ? 0.0f : accumulator.maximum;
    result.absolutePeak = accumulator.absolutePeak;
    result.peakDbfs = gainToDb (result.absolutePeak);
    result.clippedSampleCount = accumulator.clippedSampleCount;
    result.nanCount = accumulator.nanCount;
    result.positiveInfinityCount = accumulator.positiveInfinityCount;
    result.negativeInfinityCount = accumulator.negativeInfinityCount;
    result.silenceSampleCount = accumulator.silenceSampleCount;
    result.maxContinuousSilenceSamples = accumulator.maxContinuousSilenceSamples;
    result.zeroCrossingCount = accumulator.zeroCrossingCount;
    result.hasNonFinite = accumulator.nanCount > 0 || accumulator.positiveInfinityCount > 0
        || accumulator.negativeInfinityCount > 0;

    if (accumulator.sampleCount > 0)
        result.isSilent = accumulator.silenceSampleCount == accumulator.sampleCount;

    if (accumulator.finiteSampleCount > 0)
    {
        result.rms = static_cast<float> (std::sqrt (accumulator.sumSquares
                                                   / static_cast<double> (accumulator.finiteSampleCount)));
        result.rmsDbfs = gainToDb (result.rms);
        result.mean = static_cast<float> (accumulator.sum / static_cast<double> (accumulator.finiteSampleCount));
        result.dcOffset = result.mean;
        result.crestFactor = result.rms > 0.0f ? result.absolutePeak / result.rms : 0.0f;
    }

    return result;
}

float AudioStatistics::gainToDb (float value) noexcept
{
    if (value <= 0.0f || ! std::isfinite (value))
        return -std::numeric_limits<float>::infinity();

    return juce::Decibels::gainToDecibels (value);
}

bool AnalysisTap::prepare (double newSampleRate, int newChannelCount, float silenceThreshold,
                           float clipThreshold) noexcept
{
    return statistics.prepare (newSampleRate, newChannelCount, silenceThreshold, clipThreshold);
}

void AnalysisTap::reset() noexcept
{
    statistics.reset();
}

void AnalysisTap::processBlock (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    statistics.processBlock (buffer, numSamples);
}
} // namespace agent_plugin_host::audio
