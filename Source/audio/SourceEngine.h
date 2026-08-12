#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cstdint>

namespace agent_plugin_host::audio
{
enum class SourceType
{
    silence,
    sine,
    whiteNoise,
    impulse,
    file
};

class SourceEngine
{
public:
    class FileSource
    {
    public:
        virtual ~FileSource() = default;
        virtual void reset() noexcept = 0;
        virtual void renderNextBlock (juce::AudioBuffer<float>& destination, int numSamples) noexcept = 0;
    };

    struct Config
    {
        SourceType type = SourceType::silence;
        double sampleRate = 48000.0;
        int maxBlockSize = 512;
        int channels = 2;
        float frequencyHz = 440.0f;
        float levelDb = -18.0f;
        std::uint64_t seed = 1;
        int impulseSampleOffset = 0;
        float impulseAmplitude = 1.0f;
        int impulseRepeatInterval = 0;
    };

    bool prepare (const Config& newConfig) noexcept;
    void reset() noexcept;

    void renderNextBlock (juce::AudioBuffer<float>& destination, int numSamples) noexcept;
    void setFileSource (FileSource* newFileSource) noexcept;
    bool setType (SourceType newType) noexcept;
    void setLevelDb (float newLevelDb) noexcept;
    void setFrequencyHz (float newFrequencyHz) noexcept;

    [[nodiscard]] const Config& getConfig() const noexcept { return config; }
    [[nodiscard]] bool isPrepared() const noexcept { return prepared; }

private:
    static constexpr double twoPi = 6.283185307179586476925286766559;

    static bool isValidConfig (const Config& candidate) noexcept;
    static float decibelsToGainChecked (float db) noexcept;
    float nextWhiteNoiseSample() noexcept;
    void advanceGain() noexcept;

    Config config {};
    FileSource* fileSource = nullptr;
    bool prepared = false;
    double phaseRadians = 0.0;
    std::uint64_t noiseState = 1;
    std::int64_t absoluteSamplePosition = 0;
    float currentGain = 0.0f;
    float targetGain = 0.0f;
    float gainStep = 0.0f;
    int smoothingSamplesRemaining = 0;
};
} // namespace agent_plugin_host::audio
