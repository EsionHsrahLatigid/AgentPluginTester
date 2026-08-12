#include "EventSink.h"

namespace aph
{
namespace
{
juce::var makeObject()
{
    return juce::var(new juce::DynamicObject());
}

void setProperty(juce::var& object, const juce::Identifier& name, const juce::var& value)
{
    object.getDynamicObject()->setProperty(name, value);
}

void mergePayload(juce::var& target, const juce::var& payload)
{
    if (auto* payloadObject = payload.getDynamicObject())
    {
        const auto& properties = payloadObject->getProperties();
        for (int i = 0; i < properties.size(); ++i)
            setProperty(target, properties.getName(i), properties.getValueAt(i));
    }
    else if (! payload.isVoid())
    {
        setProperty(target, "payload", payload);
    }
}
} // namespace

NdjsonEventSink::NdjsonEventSink(juce::OutputStream& outputStream)
    : output(outputStream)
{
}

bool NdjsonEventSink::emit(const juce::String& eventName, const juce::var& payload, juce::String* errorMessage)
{
    if (eventName.isEmpty())
    {
        if (errorMessage != nullptr)
            *errorMessage = "event name is empty";

        return false;
    }

    const auto line = serialiseEvent(eventName, payload) + "\n";
    if (! output.writeText(line, false, false, nullptr))
    {
        if (errorMessage != nullptr)
            *errorMessage = "failed to write NDJSON event";

        return false;
    }

    output.flush();
    return true;
}

juce::var NdjsonEventSink::makeEventObject(const juce::String& eventName, const juce::var& payload)
{
    auto object = makeObject();
    setProperty(object, "schemaVersion", schemaVersion);
    setProperty(object, "event", eventName);
    setProperty(object, "timestamp", juce::Time::getCurrentTime().toISO8601(true));
    mergePayload(object, payload);
    return object;
}

juce::String NdjsonEventSink::serialiseEvent(const juce::String& eventName, const juce::var& payload)
{
    return juce::JSON::toString(makeEventObject(eventName, payload), true);
}

FileEventSink::FileEventSink(const juce::File& file)
    : stream(file)
{
    if (stream.openedOk())
        sink = std::make_unique<NdjsonEventSink>(stream);
}

FileEventSink::~FileEventSink()
{
    stream.flush();
}

bool FileEventSink::emit(const juce::String& eventName, const juce::var& payload, juce::String* errorMessage)
{
    if (sink == nullptr)
    {
        if (errorMessage != nullptr)
            *errorMessage = "event file is not open";

        return false;
    }

    return sink->emit(eventName, payload, errorMessage);
}

bool FileEventSink::isOpen() const
{
    return sink != nullptr;
}
} // namespace aph
