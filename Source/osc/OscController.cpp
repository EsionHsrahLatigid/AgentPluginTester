#include "OscController.h"
#include <cstring>
#include <limits>

namespace aph::osc
{
namespace
{
template <std::size_t N>
void copyText (std::array<char, N>& destination, const juce::String& source)
{
    const auto utf8 = source.toRawUTF8();
    std::strncpy (destination.data(), utf8, destination.size() - 1);
    destination.back() = '\0';
}

float numericValue (const juce::OSCArgument& argument)
{
    if (argument.isFloat32()) return argument.getFloat32();
    if (argument.isInt32()) return static_cast<float> (argument.getInt32());
    return 0.0f;
}

std::int64_t integerValue (const juce::OSCArgument& argument)
{
    if (argument.isInt32()) return argument.getInt32();
    return 0;
}
} // namespace

OscController::OscController()
{
    receiver.addListener (this);
    startTimerHz (120);
}

OscController::~OscController()
{
    stopTimer();
    stop();
    receiver.removeListener (this);
}

juce::Result OscController::start (int listenPort, const juce::String& replyHost, int replyPort)
{
    stop();
    if (listenPort == 0)
        return juce::Result::ok();
    if (listenPort < 1 || listenPort > 65535)
        return juce::Result::fail ("OSC port is out of range");
    if (! receiver.connect (listenPort))
        return juce::Result::fail ("Cannot bind OSC receiver to port " + juce::String (listenPort));

    senderConnected = replyPort > 0 && replyPort <= 65535 && sender.connect (replyHost, replyPort);
    return juce::Result::ok();
}

void OscController::stop()
{
    receiver.disconnect();
    sender.disconnect();
    senderConnected = false;
}

void OscController::oscMessageReceived (const juce::OSCMessage& message)
{
    received.fetch_add (1, std::memory_order_relaxed);
    const auto command = parse (message);
    if (command.type == Command::Type::invalid)
    {
        rejected.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    if (! enqueue (command))
        overflow.fetch_add (1, std::memory_order_relaxed);
}

bool OscController::enqueue (const Command& command) noexcept
{
    const auto scope = fifo.write (1);
    if (scope.blockSize1 + scope.blockSize2 == 0)
        return false;
    commands[static_cast<std::size_t> (scope.startIndex1)] = command;
    return true;
}

void OscController::timerCallback()
{
    for (int processed = 0; processed < 64; ++processed)
    {
        const auto scope = fifo.read (1);
        if (scope.blockSize1 + scope.blockSize2 == 0)
            break;
        const auto command = commands[static_cast<std::size_t> (scope.startIndex1)];
        const auto payload = handler != nullptr ? handler (command) : juce::var();
        sendReply (command, payload, handler != nullptr);
    }
}

Command OscController::parse (const juce::OSCMessage& message) const
{
    Command result;
    const auto address = message.getAddressPattern().toString();
    const auto count = static_cast<int> (message.size());
    auto at = [&] (int index) -> const juce::OSCArgument* { return index >= 0 && index < count ? &message[index] : nullptr; };

    if (address == "/host/ping") result.type = Command::Type::ping;
    else if (address == "/host/state/get") result.type = Command::Type::stateGet;
    else if (address == "/host/quit") result.type = Command::Type::quit;
    else if (address == "/host/panic") result.type = Command::Type::panic;
    else if (address == "/source/type" && at (0) != nullptr && at (0)->isString())
    {
        result.type = Command::Type::sourceType;
        copyText (result.text, at (0)->getString());
    }
    else if (address == "/source/level_db" && at (0) != nullptr)
    {
        result.type = Command::Type::sourceLevelDb;
        result.value = numericValue (*at (0));
    }
    else if (address == "/source/frequency" && at (0) != nullptr)
    {
        result.type = Command::Type::sourceFrequency;
        result.value = numericValue (*at (0));
    }
    else if ((address == "/midi/note_on" || address == "/midi/note_off") && count >= 3)
    {
        result.type = address.endsWith ("note_on") ? Command::Type::midiNoteOn : Command::Type::midiNoteOff;
        result.channel = static_cast<int> (integerValue (*at (0)));
        result.note = static_cast<int> (integerValue (*at (1)));
        result.value = numericValue (*at (2));
        if (at (3) != nullptr) result.sampleOffset = static_cast<int> (integerValue (*at (3)));
        if (result.channel < 1 || result.channel > 16 || result.note < 0 || result.note > 127
            || result.value < 0.0f || result.value > 1.0f || result.sampleOffset < 0)
            result.type = Command::Type::invalid;
    }
    else if (address == "/capture/start" && at (0) != nullptr && at (0)->isString())
    {
        result.type = Command::Type::captureStart;
        copyText (result.text, at (0)->getString());
    }
    else if (address == "/capture/stop") result.type = Command::Type::captureStop;
    else if (address.startsWith ("/plugin/"))
    {
        const auto tokens = juce::StringArray::fromTokens (address, "/", "");
        if (tokens.size() >= 3)
        {
            result.pluginIndex = tokens[1].getIntValue();
            if (tokens.size() == 3 && tokens[2] == "bypass" && at (0) != nullptr)
            {
                result.type = Command::Type::pluginBypass;
                result.value = numericValue (*at (0));
            }
            else if (tokens.size() == 4 && tokens[2] == "parameter" && tokens[3] == "get"
                     && at (0) != nullptr && at (0)->isString())
            {
                result.type = Command::Type::pluginParameterGet;
                copyText (result.parameterId, at (0)->getString());
            }
            else if (tokens.size() == 4 && tokens[2] == "parameter" && tokens[3] == "set"
                     && at (0) != nullptr && at (0)->isString() && at (1) != nullptr)
            {
                result.type = Command::Type::pluginParameterSet;
                copyText (result.parameterId, at (0)->getString());
                result.value = numericValue (*at (1));
                if (result.value < 0.0f || result.value > 1.0f) result.type = Command::Type::invalid;
            }
        }
    }

    if (count > 0)
    {
        const auto& last = message[count - 1];
        if (last.isInt32()) result.requestId = integerValue (last);
    }
    return result;
}

bool OscController::sendReply (const Command& command, const juce::var& payload, bool ok)
{
    if (! senderConnected) return false;
    const auto json = juce::JSON::toString (payload, true);
    return sender.send (ok ? "/reply" : "/error",
                        static_cast<juce::int32> (juce::jlimit<std::int64_t> (std::numeric_limits<juce::int32>::min(),
                                                                            std::numeric_limits<juce::int32>::max(),
                                                                            command.requestId)),
                        juce::String (ok ? "ok" : "error"),
                        json);
}

bool OscController::sendEvent (const juce::String& name, const juce::var& payload)
{
    return senderConnected && sender.send ("/event", name, juce::JSON::toString (payload, true));
}
} // namespace aph::osc
