#pragma once

#include "../audio/AudioStatistics.h"
#include "../audio/SourceEngine.h"
#include "../capture/CaptureWriter.h"
#include "../core/HostConfig.h"
#include "../core/EventSink.h"
#include "../core/Report.h"
#include "../midi/MidiScheduler.h"
#include "../osc/OscController.h"
#include "../plugins/PluginChain.h"
#include "../plugins/PluginLoader.h"
#include "../transport/TransportModel.h"
#include "../ui/AgentPluginHostUI.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace aph::app
{
class HostRuntime final : private juce::AudioIODeviceCallback,
                          private juce::Timer
{
public:
    enum class State { starting, scanning, loading, ready, running, stopping, completed, failed };

    explicit HostRuntime (HostConfig configuration);
    ~HostRuntime() override;

    void setCompletionHandler (std::function<void (ExitCode)> handler) { completionHandler = std::move (handler); }
    void attachUi (agentpluginhost::ui::HostMainComponent* component);

    juce::Result prepare();
    juce::Result startRealtime();
    ExitCode runOffline();
    void stop (ExitCode requestedCode = ExitCode::success);
    void panic();
    void writeReport();

    State getState() const noexcept { return state; }
    ExitCode getExitCode() const noexcept { return exitCode; }
    const HostConfig& getConfig() const noexcept { return config; }
    Report& getReport() noexcept { return report; }

private:
    class PreloadedAudioFile final : public agent_plugin_host::audio::SourceEngine::FileSource
    {
    public:
        juce::Result load (const juce::File&, double targetSampleRate, int channels);
        void reset() noexcept override { position = 0; }
        void renderNextBlock (juce::AudioBuffer<float>&, int numSamples) noexcept override;
    private:
        juce::AudioBuffer<float> data;
        int position = 0;
    };

    class EditorWindow final : public juce::DocumentWindow
    {
    public:
        EditorWindow (juce::String pluginId, bool genericEditor, juce::String title,
                      std::unique_ptr<juce::AudioProcessorEditor> editorToOwn);
        void closeButtonPressed() override { setVisible (false); }
        const juce::String& getPluginId() const noexcept { return pluginId; }
        bool isGenericEditor() const noexcept { return generic; }

    private:
        juce::String pluginId;
        bool generic = false;
    };

    void audioDeviceIOCallbackWithContext (const float* const*, int,
                                           float* const*, int, int,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void timerCallback() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) noexcept;
    juce::Result prepareEngines (double sampleRate, int blockSize, int channels);
    juce::Result loadPlugins();
    void scheduleSessionEvents();
    void finaliseReport (ExitCode);
    void refreshPluginReportLatencies();
    void emit (const juce::String& event, const juce::var& payload = {}) const;
    void setState (State);
    agentpluginhost::ui::HostUiState makeUiState() const;
    agentpluginhost::ui::HostUiActions makeUiActions();
    juce::var handleOscCommand (const aph::osc::Command&);
    void showEditor (int index, bool generic);
    void setPluginParameter (int index, const juce::String& id, float normalised);
    juce::var getPluginParameter (int index, const juce::String& id) const;
    void applyPendingSourceChanges() noexcept;
    bool hasTimedOut() const noexcept;
    static agent_plugin_host::audio::SourceType sourceTypeFor (InputSourceType);
    static juce::String stateName (State);

    HostConfig config;
    Report report;
    State state = State::starting;
    ExitCode exitCode = ExitCode::success;
    juce::int64 startedTicks = 0;
    double activeSampleRate = 48000.0;
    int activeBlockSize = 256;
    int activeChannels = 2;
    std::atomic<juce::int64> processedSamples { 0 };
    std::atomic<bool> stopRequested { false };
    std::atomic<unsigned int> pendingSourceChanges { 0 };
    std::atomic<int> pendingSourceType { static_cast<int> (InputSourceType::silence) };
    std::atomic<float> pendingSourceLevelDb { -18.0f };
    std::atomic<float> pendingSourceFrequencyHz { 440.0f };
    std::atomic<int> activeSourceType { static_cast<int> (InputSourceType::silence) };
    std::atomic<float> activeSourceLevelDb { -18.0f };
    std::atomic<float> activeSourceFrequencyHz { 440.0f };

    agent_plugin_host::audio::SourceEngine source;
    agent_plugin_host::audio::AnalysisTap inputTap, outputTap;
    agentpluginhost::midi::MidiScheduler midiScheduler;
    agentpluginhost::transport::TransportModel transport;
    agent_plugin_host::plugins::PluginLoader pluginLoader;
    agent_plugin_host::plugins::PluginChain pluginChain;
    aph::capture::CaptureWriter capture;
    aph::osc::OscController osc;
    std::unique_ptr<aph::FileEventSink> fileEventSink;
    PreloadedAudioFile audioFile;

    juce::AudioDeviceManager deviceManager;
    juce::AudioBuffer<float> realtimeBuffer;
    juce::MidiBuffer realtimeMidi;
    agentpluginhost::ui::HostMainComponent* ui = nullptr;
    std::vector<std::unique_ptr<EditorWindow>> editorWindows;
    std::function<void (ExitCode)> completionHandler;
};
} // namespace aph::app
