#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <vector>

namespace agentpluginhost::ui
{

struct MeterSnapshot
{
    float peakDbfs = -120.0f;
    float rmsDbfs = -120.0f;
    int nonFiniteCount = 0;
    int clippedSampleCount = 0;
};

struct PluginSlotState
{
    int index = 0;
    juce::String stableId;
    juce::String name = "empty";
    juce::String vendor;
    juce::String version;
    juce::String format = "VST3";
    juce::String loadState = "empty";
    bool bypassed = false;
    bool editorVisible = false;
    bool genericEditorVisible = false;
    int latencySamples = 0;
    MeterSnapshot meter;
};

struct HostUiState
{
    juce::String sessionName = "untitled";
    juce::String mode = "realtime";
    juce::String sourceType = "silence";
    float sourceLevelDb = -18.0f;
    float sourceFrequencyHz = 440.0f;
    int sampleRate = 48000;
    int blockSize = 256;
    int inputChannels = 2;
    int outputChannels = 2;

    bool playing = false;
    bool recording = false;
    double bpm = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    int64_t samplePosition = 0;

    MeterSnapshot inputMeter;
    MeterSnapshot outputMeter;
    MeterSnapshot chainMeter;

    juce::String oscBind = "127.0.0.1";
    int oscPort = 9000;
    bool oscEnabled = true;
    int oscReceivedCount = 0;
    int oscRejectedCount = 0;
    int oscQueueOverflowCount = 0;
    juce::String lastOscEvent = "/host/ping";

    juce::String reportPath;
    juce::String reportStatus = "idle";
    bool passed = false;
    int warningCount = 0;
    int errorCount = 0;
    juce::String lastStatus = "ready";

    std::vector<PluginSlotState> plugins;
};

struct HostUiActions
{
    std::function<void()> play;
    std::function<void()> stop;
    std::function<void()> panic;
    std::function<void()> toggleRecording;
    std::function<void()> writeReport;
    std::function<void(int, float)> noteOn;
    std::function<void(int, float)> noteOff;
    std::function<void(int)> setSourceByDelta;
    std::function<void(int)> toggleBypass;
    std::function<void(int)> showNativeEditor;
    std::function<void(int)> showGenericEditor;
    std::function<void(int)> removePlugin;
    std::function<void(int, int)> movePlugin;
};

class PixelButton final : public juce::Button
{
public:
    PixelButton (juce::String buttonText, juce::String automationId);

    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PixelButton)
};

class HostMainComponent final : public juce::Component,
                                private juce::Button::Listener,
                                private juce::Timer
{
public:
    static constexpr int defaultWidth = 1080;
    static constexpr int defaultHeight = 720;
    static constexpr int minimumWidth = 900;
    static constexpr int minimumHeight = 600;
    static constexpr int maximumWidth = 1600;
    static constexpr int maximumHeight = 1000;

    HostMainComponent();
    ~HostMainComponent() override;

    void setActions (HostUiActions newActions);
    void setState (HostUiState newState);
    const HostUiState& getState() const noexcept;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    static constexpr int grid = 4;
    static constexpr int maxPluginRows = 8;
    static constexpr int midiKeyCount = 13;

    struct PluginControls
    {
        PixelButton bypass { "BYP", "" };
        PixelButton editor { "GUI", "" };
        PixelButton generic { "PARAMS", "" };
        PixelButton moveUp { "UP", "" };
        PixelButton moveDown { "DN", "" };
    };

    void buttonClicked (juce::Button*) override;
    void timerCallback() override;

    void applyAutomationIds();
    void layoutPluginControls();
    void updateButtonText();
    static int noteForKeyCode (int keyCode) noexcept;
    int noteAtPosition (juce::Point<int>) const noexcept;

    void drawHeader (juce::Graphics&) const;
    void drawMidiKeyboard (juce::Graphics&) const;
    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;
    void drawReadout (juce::Graphics&, juce::Rectangle<int>, const juce::String& label, const juce::String& value, bool inverted = false) const;
    void drawMeter (juce::Graphics&, juce::Rectangle<int>, const juce::String& label, const MeterSnapshot&) const;
    void drawPluginRow (juce::Graphics&, juce::Rectangle<int>, const PluginSlotState&) const;
    void drawDitherWarning (juce::Graphics&, juce::Rectangle<int>) const;

    static juce::String formatDb (float db);
    static float meterNormalised (float db);

    HostUiState state;
    HostUiActions actions;

    PixelButton sourcePrev { "<", "source.prev" };
    PixelButton sourceNext { ">", "source.next" };
    PixelButton playButton { "PLAY", "transport.play" };
    PixelButton stopButton { "STOP", "transport.stop" };
    PixelButton panicButton { "PANIC", "transport.panic" };
    PixelButton recordButton { "REC", "capture.toggle" };
    PixelButton reportButton { "JSON", "report.write" };

    std::array<PluginControls, maxPluginRows> pluginButtons;
    std::array<juce::Rectangle<int>, maxPluginRows> pluginRowBounds {};
    std::array<juce::Rectangle<int>, midiKeyCount> midiKeyBounds {};
    std::array<bool, midiKeyCount> heldNoteKeys {};
    int mouseHeldNote = -1;

    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> sourceBounds;
    juce::Rectangle<int> transportBounds;
    juce::Rectangle<int> chainBounds;
    juce::Rectangle<int> meterBounds;
    juce::Rectangle<int> oscBounds;
    juce::Rectangle<int> reportBounds;
    juce::Rectangle<int> statusBounds;
    juce::Rectangle<int> midiBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostMainComponent)
};

class HostMainWindow final : public juce::DocumentWindow
{
public:
    HostMainWindow (juce::String name, HostUiActions actions, HostUiState initialState);

    HostMainComponent& getHostComponent() noexcept;
    void closeButtonPressed() override;

private:
    HostMainComponent* hostComponent = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostMainWindow)
};

} // namespace agentpluginhost::ui
