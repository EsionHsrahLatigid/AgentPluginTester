#pragma once

#include <juce_osc/juce_osc.h>
#include <array>
#include <atomic>
#include <functional>

namespace aph::osc
{
struct Command final
{
    enum class Type
    {
        ping,
        stateGet,
        quit,
        panic,
        sourceType,
        sourceLevelDb,
        sourceFrequency,
        midiNoteOn,
        midiNoteOff,
        pluginBypass,
        pluginParameterGet,
        pluginParameterSet,
        captureStart,
        captureStop,
        invalid
    };

    Type type = Type::invalid;
    int pluginIndex = -1;
    int channel = 1;
    int note = 0;
    int sampleOffset = 0;
    float value = 0.0f;
    std::int64_t requestId = 0;
    std::array<char, 512> text {};
    std::array<char, 128> parameterId {};
};

class OscController final : private juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>,
                            private juce::Timer
{
public:
    using Handler = std::function<juce::var (const Command&)>;

    OscController();
    ~OscController() override;

    juce::Result start (int listenPort,
                        const juce::String& replyHost = "127.0.0.1",
                        int replyPort = 0);
    void stop();
    void setHandler (Handler newHandler) { handler = std::move (newHandler); }
    bool sendEvent (const juce::String& name, const juce::var& payload);

    std::uint64_t getReceivedCount() const noexcept { return received.load(); }
    std::uint64_t getRejectedCount() const noexcept { return rejected.load(); }
    std::uint64_t getOverflowCount() const noexcept { return overflow.load(); }

private:
    void oscMessageReceived (const juce::OSCMessage&) override;
    void timerCallback() override;
    Command parse (const juce::OSCMessage&) const;
    bool enqueue (const Command&) noexcept;
    bool sendReply (const Command&, const juce::var& payload, bool ok);

    static constexpr int capacity = 256;
    juce::OSCReceiver receiver;
    juce::OSCSender sender;
    juce::AbstractFifo fifo { capacity };
    std::array<Command, capacity> commands;
    Handler handler;
    std::atomic<std::uint64_t> received { 0 }, rejected { 0 }, overflow { 0 };
    bool senderConnected = false;
};
} // namespace aph::osc
