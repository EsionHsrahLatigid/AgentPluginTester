#include "SourceEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace agent_plugin_host::audio
{
namespace
{
constexpr int smoothingSamples = 64;

float clampAudioValue (float value) noexcept
{
    if (! std::isfinite (value))
        return 0.0f;

    return std::clamp (value, -1.0f, 1.0f);
}
} // namespace

bool SourceEngine::prepare (const Config& newConfig) noexcept
{
    if (! isValidConfig (newConfig))
        return false;

    config = newConfig;
    prepared = true;
    reset();
    return true;
}

void SourceEngine::reset() noexcept
{
    phaseRadians = 0.0;
    noiseState = config.seed == 0 ? 1 : config.seed;
    absoluteSamplePosition = 0;
    currentGain = decibelsToGainChecked (config.levelDb);
    targetGain = currentGain;
    gainStep = 0.0f;
    smoothingSamplesRemaining = 0;

    if (fileSource != nullptr)
        fileSource->reset();
}

void SourceEngine::renderNextBlock (juce::AudioBuffer<float>& destination, int numSamples) noexcept
{
    if (! prepared || numSamples <= 0)
        return;

    const auto channelsToRender = std::min ({ destination.getNumChannels(), config.channels });
    const auto samplesToRender = std::min ({ destination.getNumSamples(), numSamples, config.maxBlockSize });

    destination.clear();

    if (channelsToRender <= 0 || samplesToRender <= 0)
        return;

    if (config.type == SourceType::file)
    {
        if (fileSource != nullptr)
            fileSource->renderNextBlock (destination, samplesToRender);

        absoluteSamplePosition += samplesToRender;
        return;
    }

    const auto phaseIncrement = twoPi * static_cast<double> (config.frequencyHz) / config.sampleRate;

    for (int sample = 0; sample < samplesToRender; ++sample)
    {
        advanceGain();

        auto value = 0.0f;

        switch (config.type)
        {
            case SourceType::silence:
                break;

            case SourceType::sine:
                value = static_cast<float> (std::sin (phaseRadians)) * currentGain;
                phaseRadians += phaseIncrement;
                if (phaseRadians >= twoPi)
                    phaseRadians = std::fmod (phaseRadians, twoPi);
                break;

            case SourceType::whiteNoise:
                value = nextWhiteNoiseSample() * currentGain;
                break;

            case SourceType::impulse:
            {
                const auto impulseDue = absoluteSamplePosition == config.impulseSampleOffset
                    || (config.impulseRepeatInterval > 0
                        && absoluteSamplePosition >= config.impulseSampleOffset
                        && ((absoluteSamplePosition - config.impulseSampleOffset) % config.impulseRepeatInterval) == 0);
                value = impulseDue ? clampAudioValue (config.impulseAmplitude * currentGain) : 0.0f;
                break;
            }

            case SourceType::file:
                break;
        }

        for (int channel = 0; channel < channelsToRender; ++channel)
            destination.setSample (channel, sample, value);

        ++absoluteSamplePosition;
    }
}

void SourceEngine::setFileSource (FileSource* newFileSource) noexcept
{
    fileSource = newFileSource;
    if (prepared && fileSource != nullptr)
        fileSource->reset();
}

bool SourceEngine::setType (SourceType newType) noexcept
{
    if (newType == SourceType::file && fileSource == nullptr)
        return false;

    config.type = newType;
    phaseRadians = 0.0;
    absoluteSamplePosition = 0;
    noiseState = config.seed == 0 ? 1 : config.seed;
    if (newType == SourceType::file && fileSource != nullptr)
        fileSource->reset();
    return true;
}

void SourceEngine::setLevelDb (float newLevelDb) noexcept
{
    if (! std::isfinite (newLevelDb))
        return;

    config.levelDb = std::clamp (newLevelDb, -120.0f, 24.0f);
    targetGain = decibelsToGainChecked (config.levelDb);
    smoothingSamplesRemaining = smoothingSamples;
    gainStep = (targetGain - currentGain) / static_cast<float> (smoothingSamples);
}

void SourceEngine::setFrequencyHz (float newFrequencyHz) noexcept
{
    if (! std::isfinite (newFrequencyHz) || newFrequencyHz < 0.0f)
        return;

    config.frequencyHz = std::min (newFrequencyHz, static_cast<float> (config.sampleRate * 0.5));
}

bool SourceEngine::isValidConfig (const Config& candidate) noexcept
{
    return std::isfinite (candidate.sampleRate)
        && candidate.sampleRate > 0.0
        && candidate.maxBlockSize > 0
        && candidate.channels >= 0
        && candidate.frequencyHz >= 0.0f
        && std::isfinite (candidate.frequencyHz)
        && std::isfinite (candidate.levelDb)
        && candidate.impulseSampleOffset >= 0
        && candidate.impulseRepeatInterval >= 0
        && std::isfinite (candidate.impulseAmplitude);
}

float SourceEngine::decibelsToGainChecked (float db) noexcept
{
    if (! std::isfinite (db))
        return 0.0f;

    return juce::Decibels::decibelsToGain (std::clamp (db, -120.0f, 24.0f));
}

float SourceEngine::nextWhiteNoiseSample() noexcept
{
    auto x = noiseState;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    noiseState = x == 0 ? 1 : x;

    constexpr auto scale = 1.0f / static_cast<float> (std::numeric_limits<std::uint32_t>::max());
    const auto unit = static_cast<float> (static_cast<std::uint32_t> (noiseState >> 32)) * scale;
    return unit * 2.0f - 1.0f;
}

void SourceEngine::advanceGain() noexcept
{
    if (smoothingSamplesRemaining <= 0)
    {
        currentGain = targetGain;
        return;
    }

    currentGain += gainStep;
    --smoothingSamplesRemaining;

    if (smoothingSamplesRemaining == 0)
        currentGain = targetGain;
}
} // namespace agent_plugin_host::audio
