#include "../../Source/midi/MidiScheduler.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace agentpluginhost::tests
{
class MidiSchedulerTest final : public juce::UnitTest
{
public:
    MidiSchedulerTest() : juce::UnitTest ("MidiScheduler", "midi") {}

    void runTest() override
    {
        testBlockOffsetsAndFifo();
        testCapacityAndReset();
        testPastEventsAreDiscarded();
    }

private:
    struct RenderedEvent
    {
        int note = 0;
        int offset = 0;
    };

    void testBlockOffsetsAndFifo()
    {
        beginTest ("renders block-relative sample offsets and preserves FIFO for equal samples");

        midi::MidiScheduler scheduler;
        scheduler.prepare (8);
        scheduler.beginBlock (100);

        expect (scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 60, 0.7f), 104));
        expect (scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 62, 0.7f), 100));
        expect (scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 64, 0.7f), 104));
        expect (scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 65, 0.7f), 107));

        std::vector<RenderedEvent> rendered;
        rendered.reserve (4);
        scheduler.renderBlock (8, [&rendered] (const juce::MidiMessage& message, int offset)
        {
            rendered.push_back ({ message.getNoteNumber(), offset });
        });

        expectEquals (static_cast<int> (rendered.size()), 4);
        expectEquals (rendered[0].note, 62);
        expectEquals (rendered[0].offset, 0);
        expectEquals (rendered[1].note, 60);
        expectEquals (rendered[1].offset, 4);
        expectEquals (rendered[2].note, 64);
        expectEquals (rendered[2].offset, 4);
        expectEquals (rendered[3].note, 65);
        expectEquals (rendered[3].offset, 7);
        expectEquals (static_cast<int> (scheduler.pendingEvents()), 0);
    }

    void testCapacityAndReset()
    {
        beginTest ("bounded capacity reports drops and reset clears state");

        midi::MidiScheduler scheduler;
        scheduler.prepare (1);

        expect (scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 60, 1.0f), 0));
        expect (! scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 61, 1.0f), 1));
        expectEquals (static_cast<int> (scheduler.pendingEvents()), 1);
        expectEquals (static_cast<int> (scheduler.droppedEvents()), 1);

        scheduler.reset();
        expectEquals (static_cast<int> (scheduler.pendingEvents()), 0);
        expectEquals (static_cast<int> (scheduler.droppedEvents()), 0);
        expect (scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 62, 1.0f), 0));
    }

    void testPastEventsAreDiscarded()
    {
        beginTest ("events before the current block do not render");

        midi::MidiScheduler scheduler;
        scheduler.prepare (4);
        scheduler.beginBlock (100);
        expect (scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 60, 1.0f), 99));
        expect (scheduler.scheduleAtSample (juce::MidiMessage::noteOn (1, 62, 1.0f), 100));

        auto rendered = 0;
        scheduler.renderBlock (16, [&rendered] (const juce::MidiMessage&, int) { ++rendered; });
        expectEquals (rendered, 1);
        expectEquals (static_cast<int> (scheduler.pendingEvents()), 0);
    }
};

static MidiSchedulerTest midiSchedulerTest;
} // namespace agentpluginhost::tests
