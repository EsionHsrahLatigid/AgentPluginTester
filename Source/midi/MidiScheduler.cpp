#include "MidiScheduler.h"

namespace agentpluginhost::midi
{
void MidiScheduler::prepare (size_t maxEvents)
{
    maxEvents_ = maxEvents;
    events_.clear();
    events_.reserve (maxEvents_);
    reset();
    prepared_ = true;
}

void MidiScheduler::reset() noexcept
{
    events_.clear();
    readIndex_ = 0;
    blockStartSample_ = 0;
    nextSequence_ = 0;
    droppedEvents_ = 0;
}

bool MidiScheduler::scheduleAtSample (juce::MidiMessage message, int64_t absoluteSample)
{
    if (! prepared_ || maxEvents_ == 0)
        return false;

    if (pendingEvents() >= maxEvents_)
    {
        ++droppedEvents_;
        return false;
    }

    compactConsumedEventsIfNeeded();

    ScheduledEvent event { std::move (message), std::max<int64_t> (0, absoluteSample), nextSequence_++ };
    const auto begin = events_.begin() + static_cast<std::vector<ScheduledEvent>::difference_type> (readIndex_);
    const auto end = events_.end();
    const auto insertion = std::upper_bound (begin, end, event, [] (const auto& lhs, const auto& rhs)
    {
        if (lhs.samplePosition != rhs.samplePosition)
            return lhs.samplePosition < rhs.samplePosition;

        return lhs.sequence < rhs.sequence;
    });

    events_.insert (insertion, std::move (event));
    return true;
}

bool MidiScheduler::scheduleForNextBlock (juce::MidiMessage message, int sampleOffset)
{
    return scheduleAtSample (std::move (message), blockStartSample_ + std::max (0, sampleOffset));
}

void MidiScheduler::beginBlock (int64_t blockStartSample) noexcept
{
    blockStartSample_ = std::max<int64_t> (0, blockStartSample);
}

void MidiScheduler::renderBlockToMidiBuffer (juce::MidiBuffer& output, int numSamples)
{
    renderBlock (numSamples, [&output] (const juce::MidiMessage& message, int sampleOffset)
    {
        output.addEvent (message, sampleOffset);
    });
}

int MidiScheduler::bytesRequiredForMidiBuffer (size_t maxEvents) noexcept
{
    constexpr auto midiHeaderBytes = 8;
    constexpr auto maxShortMessageBytes = 3;
    return static_cast<int> (maxEvents * (midiHeaderBytes + maxShortMessageBytes));
}

void MidiScheduler::prepareMidiBuffer (juce::MidiBuffer& buffer, size_t maxEvents)
{
    buffer.ensureSize (bytesRequiredForMidiBuffer (maxEvents));
}

void MidiScheduler::compactConsumedEventsIfNeeded()
{
    if (readIndex_ == 0)
        return;

    events_.erase (events_.begin(),
                   events_.begin() + static_cast<std::vector<ScheduledEvent>::difference_type> (readIndex_));
    readIndex_ = 0;
}
} // namespace agentpluginhost::midi
