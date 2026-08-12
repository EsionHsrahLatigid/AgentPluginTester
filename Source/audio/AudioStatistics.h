#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace agent_plugin_host::audio
{
struct AudioStatisticsSnapshot
{
    static constexpr int maxChannels = 64;

    struct Channel
    {
        std::uint64_t sampleCount = 0;
        float minimum = 0.0f;
        float maximum = 0.0f;
        float absolutePeak = 0.0f;
        float peakDbfs = -std::numeric_limits<float>::infinity();
        float rms = 0.0f;
        float rmsDbfs = -std::numeric_limits<float>::infinity();
        float mean = 0.0f;
        float dcOffset = 0.0f;
        float crestFactor = 0.0f;
        std::uint64_t clippedSampleCount = 0;
        std::uint64_t nanCount = 0;
        std::uint64_t positiveInfinityCount = 0;
        std::uint64_t negativeInfinityCount = 0;
        std::uint64_t silenceSampleCount = 0;
        std::uint64_t maxContinuousSilenceSamples = 0;
        std::uint64_t zeroCrossingCount = 0;
        bool hasNonFinite = false;
        bool isSilent = true;
    };

    int channelCount = 0;
    double sampleRate = 0.0;
    std::array<Channel, maxChannels> channels {};

    std::uint64_t totalSampleCount = 0;
    float absolutePeak = 0.0f;
    float peakDbfs = -std::numeric_limits<float>::infinity();
    float rms = 0.0f;
    float rmsDbfs = -std::numeric_limits<float>::infinity();
    float dcOffset = 0.0f;
    std::uint64_t clippedSampleCount = 0;
    std::uint64_t nanCount = 0;
    std::uint64_t positiveInfinityCount = 0;
    std::uint64_t negativeInfinityCount = 0;
    std::uint64_t silenceSampleCount = 0;
    std::uint64_t maxContinuousSilenceSamples = 0;
    std::uint64_t zeroCrossingCount = 0;
    bool hasNonFinite = false;
    bool isSilent = true;
};

class AudioStatistics
{
public:
    static constexpr int maxChannels = AudioStatisticsSnapshot::maxChannels;

    bool prepare (double newSampleRate, int newChannelCount, float newSilenceThreshold = 1.0e-6f,
                  float newClipThreshold = 1.0f) noexcept;
    void reset() noexcept;
    void processBlock (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    [[nodiscard]] AudioStatisticsSnapshot snapshot() const noexcept;
    [[nodiscard]] bool isPrepared() const noexcept { return prepared; }

private:
    struct ChannelAccumulator
    {
        void reset() noexcept;

        std::uint64_t sampleCount = 0;
        std::uint64_t finiteSampleCount = 0;
        double sum = 0.0;
        double sumSquares = 0.0;
        float minimum = std::numeric_limits<float>::infinity();
        float maximum = -std::numeric_limits<float>::infinity();
        float absolutePeak = 0.0f;
        std::uint64_t clippedSampleCount = 0;
        std::uint64_t nanCount = 0;
        std::uint64_t positiveInfinityCount = 0;
        std::uint64_t negativeInfinityCount = 0;
        std::uint64_t silenceSampleCount = 0;
        std::uint64_t currentContinuousSilenceSamples = 0;
        std::uint64_t maxContinuousSilenceSamples = 0;
        std::uint64_t zeroCrossingCount = 0;
        float previousFiniteSample = 0.0f;
        bool hasPreviousFiniteSample = false;
    };

    static AudioStatisticsSnapshot::Channel makeChannelSnapshot (const ChannelAccumulator& accumulator) noexcept;
    static float gainToDb (float value) noexcept;

    bool prepared = false;
    double sampleRate = 0.0;
    int channelCount = 0;
    float silenceThreshold = 1.0e-6f;
    float clipThreshold = 1.0f;
    std::array<ChannelAccumulator, maxChannels> accumulators {};
};

class AnalysisTap
{
public:
    bool prepare (double sampleRate, int channelCount, float silenceThreshold = 1.0e-6f,
                  float clipThreshold = 1.0f) noexcept;
    void reset() noexcept;
    void processBlock (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    [[nodiscard]] AudioStatisticsSnapshot snapshot() const noexcept { return statistics.snapshot(); }
    [[nodiscard]] bool isPrepared() const noexcept { return statistics.isPrepared(); }

private:
    AudioStatistics statistics;
};
} // namespace agent_plugin_host::audio
