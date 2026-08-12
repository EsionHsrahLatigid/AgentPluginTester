#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace agentpluginhost::midi
{
class MidiScheduler
{
public:
    struct ScheduledEvent
    {
        juce::MidiMessage message;
        int64_t samplePosition = 0;
        uint64_t sequence = 0;
    };

    void prepare (size_t maxEvents);
    void reset() noexcept;

    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }
    [[nodiscard]] size_t capacity() const noexcept { return maxEvents_; }
    [[nodiscard]] size_t pendingEvents() const noexcept { return events_.size() - readIndex_; }
    [[nodiscard]] uint64_t droppedEvents() const noexcept { return droppedEvents_; }

    [[nodiscard]] bool scheduleAtSample (juce::MidiMessage message, int64_t absoluteSample);
    [[nodiscard]] bool scheduleForNextBlock (juce::MidiMessage message, int sampleOffset);

    void beginBlock (int64_t blockStartSample) noexcept;

    template <typename EmitEvent>
    void renderBlock (int numSamples, EmitEvent&& emitEvent)
    {
        if (! prepared_ || numSamples <= 0)
            return;

        const auto blockEnd = blockStartSample_ + static_cast<int64_t> (numSamples);

        while (readIndex_ < events_.size())
        {
            const auto& event = events_[readIndex_];

            if (event.samplePosition < blockStartSample_)
            {
                ++readIndex_;
                continue;
            }

            if (event.samplePosition >= blockEnd)
                break;

            const auto offset = static_cast<int> (event.samplePosition - blockStartSample_);
            emitEvent (event.message, offset);
            ++readIndex_;
        }

        blockStartSample_ = blockEnd;
    }

    void renderBlockToMidiBuffer (juce::MidiBuffer& output, int numSamples);

    static int bytesRequiredForMidiBuffer (size_t maxEvents) noexcept;
    static void prepareMidiBuffer (juce::MidiBuffer& buffer, size_t maxEvents);

private:
    void compactConsumedEventsIfNeeded();

    std::vector<ScheduledEvent> events_;
    size_t maxEvents_ = 0;
    size_t readIndex_ = 0;
    int64_t blockStartSample_ = 0;
    uint64_t nextSequence_ = 0;
    uint64_t droppedEvents_ = 0;
    bool prepared_ = false;
};
} // namespace agentpluginhost::midi
