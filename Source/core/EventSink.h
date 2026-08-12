#pragma once

#include <juce_core/juce_core.h>

#include <memory>

namespace aph
{
class EventSink
{
public:
    virtual ~EventSink() = default;

    virtual bool emit(const juce::String& eventName,
                      const juce::var& payload = {},
                      juce::String* errorMessage = nullptr) = 0;
};

class NdjsonEventSink final : public EventSink
{
public:
    static constexpr const char* schemaVersion = "1.0";

    explicit NdjsonEventSink(juce::OutputStream& outputStream);

    bool emit(const juce::String& eventName,
              const juce::var& payload = {},
              juce::String* errorMessage = nullptr) override;

    static juce::var makeEventObject(const juce::String& eventName, const juce::var& payload = {});
    static juce::String serialiseEvent(const juce::String& eventName, const juce::var& payload = {});

private:
    juce::OutputStream& output;
};

class FileEventSink final : public EventSink
{
public:
    explicit FileEventSink(const juce::File& file);
    ~FileEventSink() override;

    bool emit(const juce::String& eventName,
              const juce::var& payload = {},
              juce::String* errorMessage = nullptr) override;

    bool isOpen() const;

private:
    juce::FileOutputStream stream;
    std::unique_ptr<NdjsonEventSink> sink;
};
} // namespace aph
