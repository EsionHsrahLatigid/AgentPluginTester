#include "HostRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace aph::app
{
namespace
{
juce::var objectWith (const juce::Identifier& key, const juce::var& value)
{
    auto* object = new juce::DynamicObject();
    object->setProperty (key, value);
    return juce::var (object);
}

AudioMeasurement measurementFor (const juce::String& tapName,
                                 const agent_plugin_host::audio::AudioStatisticsSnapshot& snapshot)
{
    AudioMeasurement result;
    result.tap = tapName;
    result.sampleCount = snapshot.channelCount > 0 ? static_cast<juce::int64> (snapshot.channels[0].sampleCount) : 0;
    result.channelCount = snapshot.channelCount;
    result.sampleRate = snapshot.sampleRate;
    result.absolutePeak = snapshot.absolutePeak;
    result.peakDbfs = snapshot.peakDbfs;
    result.rms = snapshot.rms;
    result.rmsDbfs = snapshot.rmsDbfs;
    result.dcOffset = snapshot.dcOffset;
    result.clippedSampleCount = static_cast<juce::int64> (snapshot.clippedSampleCount);
    result.nanCount = static_cast<juce::int64> (snapshot.nanCount);
    result.positiveInfinityCount = static_cast<juce::int64> (snapshot.positiveInfinityCount);
    result.negativeInfinityCount = static_cast<juce::int64> (snapshot.negativeInfinityCount);
    result.silentSampleCount = static_cast<juce::int64> (snapshot.silenceSampleCount);
    result.maxConsecutiveSilenceSeconds = snapshot.sampleRate > 0.0
        ? static_cast<double> (snapshot.maxContinuousSilenceSamples) / snapshot.sampleRate : 0.0;
    result.zeroCrossingCount = static_cast<juce::int64> (snapshot.zeroCrossingCount);
    if (snapshot.channelCount > 0)
    {
        result.minimum = snapshot.channels[0].minimum;
        result.maximum = snapshot.channels[0].maximum;
        for (int channelIndex = 0; channelIndex < snapshot.channelCount; ++channelIndex)
        {
            const auto& channel = snapshot.channels[static_cast<std::size_t> (channelIndex)];
            result.minimum = juce::jmin (result.minimum, static_cast<double> (channel.minimum));
            result.maximum = juce::jmax (result.maximum, static_cast<double> (channel.maximum));
            auto* channelObject = new juce::DynamicObject();
            channelObject->setProperty ("index", channelIndex);
            channelObject->setProperty ("sampleCount", static_cast<juce::int64> (channel.sampleCount));
            channelObject->setProperty ("minimum", channel.minimum);
            channelObject->setProperty ("maximum", channel.maximum);
            channelObject->setProperty ("absolutePeak", channel.absolutePeak);
            channelObject->setProperty ("peakDbfs", channel.peakDbfs);
            channelObject->setProperty ("rms", channel.rms);
            channelObject->setProperty ("rmsDbfs", channel.rmsDbfs);
            channelObject->setProperty ("dcOffset", channel.dcOffset);
            channelObject->setProperty ("clippedSampleCount", static_cast<juce::int64> (channel.clippedSampleCount));
            channelObject->setProperty ("nanCount", static_cast<juce::int64> (channel.nanCount));
            channelObject->setProperty ("positiveInfinityCount", static_cast<juce::int64> (channel.positiveInfinityCount));
            channelObject->setProperty ("negativeInfinityCount", static_cast<juce::int64> (channel.negativeInfinityCount));
            channelObject->setProperty ("silentSampleCount", static_cast<juce::int64> (channel.silenceSampleCount));
            channelObject->setProperty ("zeroCrossingCount", static_cast<juce::int64> (channel.zeroCrossingCount));
            result.channels.add (juce::var (channelObject));
        }
    }
    result.mean = snapshot.dcOffset;
    result.crestFactor = snapshot.rms > 0.0f ? snapshot.absolutePeak / snapshot.rms : 0.0;
    return result;
}
} // namespace

HostRuntime::HostRuntime (HostConfig configuration)
    : config (std::move (configuration))
{
    pendingSourceType.store (static_cast<int> (config.source.type));
    activeSourceType.store (static_cast<int> (config.source.type));
    pendingSourceLevelDb.store (static_cast<float> (config.source.levelDb));
    activeSourceLevelDb.store (static_cast<float> (config.source.levelDb));
    pendingSourceFrequencyHz.store (static_cast<float> (config.source.frequencyHz));
    activeSourceFrequencyHz.store (static_cast<float> (config.source.frequencyHz));
    report.configuration = config;
    report.startedAt = Report::timestampNowUtc();
    startedTicks = juce::Time::getHighResolutionTicks();
    if (config.eventsTarget.isNotEmpty() && config.eventsTarget != "stdout")
        fileEventSink = std::make_unique<aph::FileEventSink> (juce::File (config.eventsTarget));
    osc.setHandler ([this] (const aph::osc::Command& command) { return handleOscCommand (command); });
}

HostRuntime::~HostRuntime()
{
    stopTimer();
    deviceManager.removeAudioCallback (this);
    capture.stop();
}

void HostRuntime::attachUi (agentpluginhost::ui::HostMainComponent* component)
{
    ui = component;
    if (ui != nullptr)
    {
        ui->setActions (makeUiActions());
        ui->setState (makeUiState());
        if (config.showEditors)
            for (int index = 0; index < pluginChain.size(); ++index)
                showEditor (index, false);
    }
}

juce::Result HostRuntime::prepare()
{
    setState (State::scanning);
    if (const auto loaded = loadPlugins(); loaded.failed())
    {
        report.addError ("plugins", loaded.getErrorMessage());
        exitCode = ExitCode::pluginLoadFailure;
        setState (State::failed);
        return loaded;
    }
    if (hasTimedOut())
    {
        exitCode = ExitCode::timeout;
        report.addError ("timeout", "Startup/plugin scan exceeded timeoutSeconds");
        setState (State::failed);
        return juce::Result::fail ("Startup/plugin scan timed out");
    }

    activeSampleRate = config.audio.sampleRate > 0.0 ? config.audio.sampleRate : 48000.0;
    activeBlockSize = config.audio.blockSize > 0 ? config.audio.blockSize : 256;
    activeChannels = juce::jmax (1, config.audio.outputChannels);

    if (config.mode == RunMode::offline)
    {
        if (const auto result = prepareEngines (activeSampleRate, activeBlockSize, activeChannels); result.failed())
            return result;
    }

    if (config.oscPort != 0)
    {
        const auto replyHost = config.oscReplyHost == "sender" ? "127.0.0.1" : config.oscReplyHost;
        if (const auto result = osc.start (config.oscPort, replyHost, config.oscReplyPort); result.failed())
            report.addWarning ("osc", result.getErrorMessage());
    }

    setState (State::ready);
    return juce::Result::ok();
}

juce::Result HostRuntime::loadPlugins()
{
    setState (State::loading);
    for (int index = 0; index < config.plugins.size(); ++index)
    {
        const auto& specification = config.plugins.getReference (index);
        auto scan = pluginLoader.scanSinglePluginFile (specification.path);
        if (! scan.succeeded())
            return juce::Result::fail (scan.error);

        juce::PluginDescription* selected = nullptr;
        if (scan.descriptions.size() == 1)
            selected = scan.descriptions[0];
        else if (specification.classId.isNotEmpty())
            for (auto* description : scan.descriptions)
                if (description->createIdentifierString() == specification.classId)
                    selected = description;

        if (selected == nullptr)
            return juce::Result::fail ("Plug-in bundle contains multiple classes; specify classId for " + specification.path.getFullPathName());

        if (const auto added = addPluginClass (specification.path, *selected, specification.bypass); added.failed())
            return added;
    }
    return juce::Result::ok();
}

juce::Result HostRuntime::addPluginClass (const juce::File& path,
                                          const juce::PluginDescription& description,
                                          bool bypassed)
{
    const auto index = pluginChain.size();
    auto loaded = pluginLoader.createInstanceBlocking (description, activeSampleRate, activeBlockSize, index);
    if (! loaded.succeeded())
        return juce::Result::fail (loaded.error);

    PluginReport pluginReport;
    pluginReport.index = index;
    pluginReport.uuid = loaded.metadata.runtimeId;
    pluginReport.path = path.getFullPathName();
    pluginReport.name = loaded.metadata.name;
    pluginReport.vendor = loaded.metadata.manufacturerName;
    pluginReport.version = loaded.metadata.version;
    pluginReport.format = loaded.metadata.formatName;
    pluginReport.latencySamples = loaded.metadata.reportedLatencySamples;
    pluginReport.loaded = true;
    pluginReport.bypass = bypassed;
    report.plugins.add (pluginReport);

    const auto addedIndex = pluginChain.addPlugin (std::move (loaded.instance), std::move (loaded.metadata), bypassed);
    if (auto* slot = pluginChain.getSlot (addedIndex); slot != nullptr && slot->instance != nullptr)
        slot->instance->setPlayHead (&transport);

    refreshPluginReportLatencies();
    auto* payload = new juce::DynamicObject();
    payload->setProperty ("index", addedIndex);
    payload->setProperty ("name", description.name);
    emit ("plugin_loaded", juce::var (payload));
    return juce::Result::ok();
}

void HostRuntime::addPlugins (const juce::StringArray& paths)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (paths.isEmpty())
        return;

    if (state != State::ready && state != State::running)
    {
        lastUiStatus = "PLUGIN LOAD UNAVAILABLE WHILE " + stateName (state).toUpperCase();
        if (ui != nullptr) ui->setState (makeUiState());
        return;
    }

    const auto shouldResumeAudio = state == State::running && deviceManager.getCurrentAudioDevice() != nullptr;
    if (shouldResumeAudio)
    {
        suspendingForPluginLoad = true;
        deviceManager.removeAudioCallback (this);
    }

    int addedCount = 0;
    int failedCount = 0;
    for (const auto& pathText : paths)
    {
        const juce::File path (pathText);
        auto scan = pluginLoader.scanSinglePluginFile (path);
        if (! scan.succeeded())
        {
            ++failedCount;
            report.addError ("plugins", scan.error);
            emit ("plugin_load_failed", objectWith ("message", scan.error));
            continue;
        }

        for (const auto* description : scan.descriptions)
        {
            if (description == nullptr)
                continue;

            if (const auto added = addPluginClass (path, *description, false); added.failed())
            {
                ++failedCount;
                const auto message = "Cannot load " + description->name + ": " + added.getErrorMessage();
                report.addError ("plugins", message);
                emit ("plugin_load_failed", objectWith ("message", message));
                continue;
            }

            PluginConfig plugin;
            plugin.path = path;
            plugin.classId = description->createIdentifierString();
            config.plugins.add (plugin);
            ++addedCount;
        }
    }

    report.configuration = config;
    if (shouldResumeAudio)
    {
        deviceManager.addAudioCallback (this);
        suspendingForPluginLoad = false;
    }

    if (addedCount > 0)
        lastUiStatus = juce::String (addedCount) + (addedCount == 1 ? " PLUGIN ADDED" : " PLUGINS ADDED");
    else
        lastUiStatus.clear();
    if (failedCount > 0)
        lastUiStatus += (lastUiStatus.isNotEmpty() ? " / " : "")
                      + juce::String (failedCount) + (failedCount == 1 ? " LOAD FAILED" : " LOADS FAILED");

    if (ui != nullptr)
        ui->setState (makeUiState());
}

juce::Result HostRuntime::prepareEngines (double sampleRate, int blockSize, int channels)
{
    activeSampleRate = sampleRate;
    activeBlockSize = blockSize;
    activeChannels = channels;

    agent_plugin_host::audio::SourceEngine::Config sourceConfig;
    sourceConfig.type = sourceTypeFor (config.source.type);
    sourceConfig.sampleRate = sampleRate;
    sourceConfig.maxBlockSize = blockSize;
    sourceConfig.channels = channels;
    sourceConfig.frequencyHz = static_cast<float> (config.source.frequencyHz);
    sourceConfig.levelDb = static_cast<float> (config.source.levelDb);
    sourceConfig.seed = config.source.seed;

    if (config.source.type == InputSourceType::file)
    {
        if (const auto loaded = audioFile.load (config.source.inputFile, sampleRate, channels); loaded.failed())
            return loaded;
        source.setFileSource (&audioFile);
    }

    if (! source.prepare (sourceConfig)) return juce::Result::fail ("Invalid source configuration");
    if (! inputTap.prepare (sampleRate, channels) || ! outputTap.prepare (sampleRate, channels))
        return juce::Result::fail ("Invalid analysis configuration");

    midiScheduler.prepare (4096);
    agentpluginhost::midi::MidiScheduler::prepareMidiBuffer (realtimeMidi, midiScheduler.capacity());
    transport.prepare (sampleRate, blockSize);
    pluginChain.prepareToPlay (sampleRate, blockSize, channels);
    refreshPluginReportLatencies();
    for (int i = 0; i < pluginChain.size(); ++i)
        if (auto* slot = pluginChain.getSlot (i); slot != nullptr && slot->instance != nullptr)
            slot->instance->setPlayHead (&transport);
    realtimeBuffer.setSize (channels, blockSize, false, true, false);
    scheduleSessionEvents();

    if (config.recordPath != juce::File())
        if (const auto result = capture.start (config.recordPath, sampleRate, channels); result.failed())
            return result;
    return juce::Result::ok();
}

void HostRuntime::scheduleSessionEvents()
{
    for (const auto& event : config.scheduledEvents)
    {
        auto* object = event.getDynamicObject();
        if (object == nullptr) continue;
        const auto seconds = static_cast<double> (object->getProperty ("atSeconds"));
        const auto sample = static_cast<juce::int64> (std::llround (seconds * activeSampleRate));
        const auto type = object->getProperty ("type").toString();
        const auto channel = static_cast<int> (object->getProperty ("channel"));
        const auto note = static_cast<int> (object->getProperty ("note"));
        const auto velocity = static_cast<float> (object->getProperty ("velocity"));
        bool scheduled = true;
        if (type == "noteOn") scheduled = midiScheduler.scheduleAtSample (juce::MidiMessage::noteOn (channel, note, velocity), sample);
        if (type == "noteOff") scheduled = midiScheduler.scheduleAtSample (juce::MidiMessage::noteOff (channel, note, velocity), sample);
        if (! scheduled) report.addWarning ("events", "MIDI scheduler capacity exceeded while loading session events");
    }
}

juce::Result HostRuntime::startRealtime()
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup (setup);
    if (config.audio.sampleRate > 0.0) setup.sampleRate = config.audio.sampleRate;
    if (config.audio.blockSize > 0) setup.bufferSize = config.audio.blockSize;
    const auto error = deviceManager.initialise (config.source.type == InputSourceType::mic ? config.audio.inputChannels : 0,
                                                 config.audio.outputChannels, nullptr, true,
                                                 {}, &setup);
    if (error.isNotEmpty())
    {
        exitCode = ExitCode::deviceInitialisationFailure;
        report.addError ("audioDevice", error);
        setState (State::failed);
        return juce::Result::fail (error);
    }
    deviceManager.addAudioCallback (this);
    setState (State::running);
    startTimerHz (30);
    return juce::Result::ok();
}

ExitCode HostRuntime::runOffline()
{
    setState (State::running);
    emit ("host_started");
    const auto duration = config.runSeconds > 0.0 ? config.runSeconds : 1.0;
    const auto totalSamples = static_cast<juce::int64> (std::llround (duration * activeSampleRate));
    juce::AudioBuffer<float> buffer (activeChannels, activeBlockSize);
    juce::MidiBuffer midi;
    agentpluginhost::midi::MidiScheduler::prepareMidiBuffer (midi, midiScheduler.capacity());

    while (processedSamples.load() < totalSamples && ! stopRequested.load())
    {
        if (hasTimedOut())
        {
            exitCode = ExitCode::timeout;
            break;
        }
        const auto remaining = static_cast<int> (juce::jmin<juce::int64> (activeBlockSize, totalSamples - processedSamples.load()));
        buffer.setSize (activeChannels, remaining, false, false, true);
        processBlock (buffer, midi);
    }
    stop (exitCode);
    return exitCode;
}

void HostRuntime::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    applyPendingSourceChanges();
    source.renderNextBlock (buffer, numSamples);
    inputTap.processBlock (buffer, numSamples);
    midi.clear();
    midiScheduler.beginBlock (processedSamples.load (std::memory_order_relaxed));
    midiScheduler.renderBlockToMidiBuffer (midi, numSamples);
    pluginChain.processBlock (buffer, midi);
    outputTap.processBlock (buffer, numSamples);
    capture.push (buffer);
    transport.advanceBlock (numSamples);
    processedSamples.fetch_add (numSamples, std::memory_order_relaxed);
}

void HostRuntime::audioDeviceIOCallbackWithContext (const float* const* input, int numInputs,
                                                    float* const* output, int numOutputs, int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    if (numSamples > realtimeBuffer.getNumSamples())
    {
        for (int channel = 0; channel < numOutputs; ++channel)
            if (output[channel] != nullptr) juce::FloatVectorOperations::clear (output[channel], numSamples);
        return;
    }

    auto block = juce::AudioBuffer<float> (realtimeBuffer.getArrayOfWritePointers(), activeChannels, numSamples);
    applyPendingSourceChanges();
    if (static_cast<InputSourceType> (activeSourceType.load (std::memory_order_relaxed)) == InputSourceType::mic)
    {
        block.clear();
        for (int channel = 0; channel < juce::jmin (activeChannels, numInputs); ++channel)
            if (input[channel] != nullptr) block.copyFrom (channel, 0, input[channel], numSamples);
    }
    else
    {
        source.renderNextBlock (block, numSamples);
    }
    inputTap.processBlock (block, numSamples);
    realtimeMidi.clear();
    midiScheduler.beginBlock (processedSamples.load (std::memory_order_relaxed));
    midiScheduler.renderBlockToMidiBuffer (realtimeMidi, numSamples);
    pluginChain.processBlock (block, realtimeMidi);
    outputTap.processBlock (block, numSamples);
    capture.push (block);
    transport.advanceBlock (numSamples);
    processedSamples.fetch_add (numSamples, std::memory_order_relaxed);

    for (int channel = 0; channel < numOutputs; ++channel)
        if (output[channel] != nullptr)
        {
            if (channel < activeChannels) juce::FloatVectorOperations::copy (output[channel], block.getReadPointer (channel), numSamples);
            else juce::FloatVectorOperations::clear (output[channel], numSamples);
        }
}

void HostRuntime::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (suspendingForPluginLoad)
        return;

    const auto result = prepareEngines (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples(),
                                        juce::jmax (1, config.audio.outputChannels));
    if (result.failed()) report.addError ("audioDevice", result.getErrorMessage());
}

void HostRuntime::audioDeviceStopped()
{
    if (! suspendingForPluginLoad)
        pluginChain.releaseResources();
}

void HostRuntime::timerCallback()
{
    if (ui != nullptr) ui->setState (makeUiState());
    if (hasTimedOut())
        stop (ExitCode::timeout);
    else if (config.runSeconds > 0.0 && processedSamples.load() >= static_cast<juce::int64> (config.runSeconds * activeSampleRate))
        stop (ExitCode::success);
}

void HostRuntime::stop (ExitCode requestedCode)
{
    if (state == State::completed || state == State::failed || state == State::stopping) return;
    setState (State::stopping);
    stopRequested.store (true);
    stopTimer();
    deviceManager.removeAudioCallback (this);
    capture.stop();
    finaliseReport (requestedCode);
    setState (exitCode == ExitCode::success ? State::completed : State::failed);
    emit ("test_completed", objectWith ("passed", report.passed));
    if (completionHandler != nullptr) completionHandler (exitCode);
}

void HostRuntime::panic()
{
    for (int channel = 1; channel <= 16; ++channel)
        static_cast<void> (midiScheduler.scheduleForNextBlock (juce::MidiMessage::allNotesOff (channel), 0));
}

void HostRuntime::finaliseReport (ExitCode code)
{
    exitCode = code;
    pluginChain.refreshMetadataFromInstances();
    refreshPluginReportLatencies();
    report.completedAt = Report::timestampNowUtc();
    report.durationSeconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startedTicks);
    report.addMeasurement (measurementFor ("input", inputTap.snapshot()));
    report.addMeasurement (measurementFor ("output", outputTap.snapshot()));
    const auto output = outputTap.snapshot();
    const auto nonFinite = output.nanCount + output.positiveInfinityCount + output.negativeInfinityCount;
    if (nonFinite > 0 && exitCode == ExitCode::success) exitCode = ExitCode::fatalRuntimeError;
    if (capture.getOverflowCount() > 0 && exitCode == ExitCode::success) exitCode = ExitCode::assertionFailure;

    auto compare = [] (const juce::String& operation, double actual, double expected)
    {
        const auto approximatelyEqual = std::abs (actual - expected) <= 1.0e-9;
        if (operation == "eq") return approximatelyEqual;
        if (operation == "ne") return ! approximatelyEqual;
        if (operation == "gt") return actual > expected;
        if (operation == "gte") return actual >= expected;
        if (operation == "lt") return actual < expected;
        if (operation == "lte") return actual <= expected;
        return false;
    };
    for (const auto& assertionValue : config.assertions)
    {
        auto* assertionObject = assertionValue.getDynamicObject();
        if (assertionObject == nullptr) continue;
        AssertionResult assertion;
        assertion.type = assertionObject->getProperty ("type").toString();
        assertion.op = assertionObject->getProperty ("op").toString();
        assertion.expectedValue = static_cast<double> (assertionObject->getProperty ("value"));
        if (assertion.type == "nonFiniteCount") assertion.actualValue = static_cast<double> (nonFinite);
        else if (assertion.type == "rmsDbfs") assertion.actualValue = output.rmsDbfs;
        else if (assertion.type == "peakDbfs") assertion.actualValue = output.peakDbfs;
        else
        {
            assertion.message = "Unsupported assertion type";
            assertion.passed = false;
            report.assertions.add (assertion);
            if (exitCode == ExitCode::success) exitCode = ExitCode::assertionFailure;
            continue;
        }
        assertion.passed = compare (assertion.op, assertion.actualValue, assertion.expectedValue);
        assertion.message = assertion.passed ? "passed" : "comparison failed";
        report.assertions.add (assertion);
        if (! assertion.passed && exitCode == ExitCode::success) exitCode = ExitCode::assertionFailure;
    }
    if (config.recordPath != juce::File() && config.recordPath.existsAsFile())
        report.artifacts.add ({ "audio/wav", config.recordPath.getFullPathName(), "32-bit float output capture" });
    report.exitCode = exitCode;
    report.passed = exitCode == ExitCode::success;
    writeReport();
}

void HostRuntime::refreshPluginReportLatencies()
{
    for (int i = 0; i < report.plugins.size(); ++i)
    {
        auto& plugin = report.plugins.getReference (i);
        if (const auto* slot = pluginChain.getSlot (plugin.index))
            plugin.latencySamples = slot->metadata.reportedLatencySamples;
    }
}

void HostRuntime::writeReport()
{
    if (config.reportPath == juce::File()) return;
    juce::String error;
    if (! report.writeToFile (config.reportPath, &error))
    {
        report.addError ("report", error);
        exitCode = ExitCode::outputWriteFailure;
    }
}

void HostRuntime::emit (const juce::String& event, const juce::var& payload) const
{
    if (config.eventsTarget == "stdout")
        std::cout << aph::NdjsonEventSink::serialiseEvent (event, payload).toStdString() << std::endl;
    else if (fileEventSink != nullptr)
        static_cast<void> (fileEventSink->emit (event, payload));
}

void HostRuntime::setState (State newState)
{
    state = newState;
    lastUiStatus = stateName (state);
    TimelineEvent event { "state_changed", Report::timestampNowUtc(), objectWith ("state", stateName (state)) };
    report.timeline.add (event);
    emit ("state_changed", event.payload);
}

agentpluginhost::ui::HostUiState HostRuntime::makeUiState() const
{
    agentpluginhost::ui::HostUiState value;
    value.mode = config.mode == RunMode::offline ? "offline" : "realtime";
    value.sourceType = HostConfigParser::toString (static_cast<InputSourceType> (activeSourceType.load()));
    value.sourceLevelDb = activeSourceLevelDb.load();
    value.sourceFrequencyHz = activeSourceFrequencyHz.load();
    value.sampleRate = static_cast<int> (activeSampleRate);
    value.blockSize = activeBlockSize;
    value.inputChannels = config.audio.inputChannels;
    value.outputChannels = config.audio.outputChannels;
    value.playing = transport.isPlaying();
    value.recording = capture.isRecording();
    value.bpm = transport.getBpm();
    value.samplePosition = transport.getSamplePosition();
    value.oscBind = config.oscBind;
    value.oscPort = config.oscPort;
    value.oscReceivedCount = static_cast<int> (osc.getReceivedCount());
    value.oscRejectedCount = static_cast<int> (osc.getRejectedCount());
    value.oscQueueOverflowCount = static_cast<int> (osc.getOverflowCount());
    value.reportPath = config.reportPath.getFullPathName();
    value.reportStatus = report.completedAt.isNotEmpty() ? "written" : "pending";
    value.passed = report.passed;
    value.errorCount = report.errors.size();
    value.warningCount = report.warnings.size();
    value.lastStatus = lastUiStatus;
    const auto output = outputTap.snapshot();
    value.outputMeter.peakDbfs = output.peakDbfs;
    value.outputMeter.rmsDbfs = output.rmsDbfs;
    value.outputMeter.nonFiniteCount = static_cast<int> (output.nanCount + output.positiveInfinityCount + output.negativeInfinityCount);
    value.outputMeter.clippedSampleCount = static_cast<int> (output.clippedSampleCount);
    value.chainMeter = value.outputMeter;
    const auto input = inputTap.snapshot();
    value.inputMeter.peakDbfs = input.peakDbfs;
    value.inputMeter.rmsDbfs = input.rmsDbfs;
    for (int index = 0; index < pluginChain.size(); ++index)
        if (const auto* slot = pluginChain.getSlot (index))
        {
            agentpluginhost::ui::PluginSlotState plugin;
            plugin.index = index;
            plugin.stableId = slot->runtimeId;
            plugin.name = slot->metadata.name;
            plugin.vendor = slot->metadata.manufacturerName;
            plugin.version = slot->metadata.version;
            plugin.format = slot->metadata.formatName;
            plugin.loadState = "ready";
            plugin.bypassed = slot->bypassed.load();
            for (const auto& window : editorWindows)
                if (window->getPluginId() == slot->runtimeId && window->isVisible())
                {
                    if (window->isGenericEditor()) plugin.genericEditorVisible = true;
                    else plugin.editorVisible = true;
                }
            plugin.latencySamples = slot->metadata.reportedLatencySamples;
            value.plugins.push_back (std::move (plugin));
        }
    return value;
}

agentpluginhost::ui::HostUiActions HostRuntime::makeUiActions()
{
    agentpluginhost::ui::HostUiActions actions;
    actions.addPlugins = [this] (const juce::StringArray& paths) { addPlugins (paths); };
    actions.play = [this] { transport.setPlaying (true); };
    actions.stop = [this] { transport.setPlaying (false); };
    actions.panic = [this] { panic(); };
    actions.toggleRecording = [this]
    {
        if (capture.isRecording()) capture.stop();
        else if (config.recordPath != juce::File()) capture.start (config.recordPath, activeSampleRate, activeChannels);
    };
    actions.writeReport = [this] { writeReport(); };
    actions.noteOn = [this] (int note, float velocity)
    {
        static_cast<void> (midiScheduler.scheduleForNextBlock (juce::MidiMessage::noteOn (1, note, velocity), 0));
    };
    actions.noteOff = [this] (int note, float velocity)
    {
        static_cast<void> (midiScheduler.scheduleForNextBlock (juce::MidiMessage::noteOff (1, note, velocity), 0));
    };
    actions.setSourceByDelta = [this] (int delta)
    {
        constexpr std::array<InputSourceType, 4> available {
            InputSourceType::silence, InputSourceType::sine,
            InputSourceType::whiteNoise, InputSourceType::impulse
        };
        const auto current = static_cast<InputSourceType> (activeSourceType.load());
        auto index = 0;
        for (std::size_t candidate = 0; candidate < available.size(); ++candidate)
            if (available[candidate] == current)
                index = static_cast<int> (candidate);
        const auto count = static_cast<int> (available.size());
        index = (index + (delta < 0 ? -1 : 1) + count) % count;
        pendingSourceType.store (static_cast<int> (available[static_cast<std::size_t> (index)]), std::memory_order_relaxed);
        pendingSourceChanges.fetch_or (1u, std::memory_order_release);
    };
    actions.toggleBypass = [this] (int index) { pluginChain.setBypassed (index, ! pluginChain.isBypassed (index)); };
    actions.showNativeEditor = [this] (int index) { showEditor (index, false); };
    actions.showGenericEditor = [this] (int index) { showEditor (index, true); };
    return actions;
}

void HostRuntime::showEditor (int index, bool generic)
{
    auto* slot = pluginChain.getSlot (index);
    if (slot == nullptr || slot->instance == nullptr) return;

    auto findWindow = [this, &slot] (bool genericEditor)
    {
        return std::find_if (editorWindows.begin(), editorWindows.end(), [&slot, genericEditor] (const auto& window)
        {
            return window->getPluginId() == slot->runtimeId && window->isGenericEditor() == genericEditor;
        });
    };

    if (auto existing = findWindow (generic); existing != editorWindows.end())
    {
        (*existing)->setVisible (true);
        (*existing)->toFront (true);
        if (ui != nullptr) ui->setState (makeUiState());
        return;
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor;
    if (generic) editor = std::make_unique<juce::GenericAudioProcessorEditor> (*slot->instance);
    else
    {
        editor = pluginChain.createEditorForSlot (index);
        if (editor == nullptr)
        {
            generic = true;
            if (auto existing = findWindow (true); existing != editorWindows.end())
            {
                (*existing)->setVisible (true);
                (*existing)->toFront (true);
                if (ui != nullptr) ui->setState (makeUiState());
                return;
            }
            editor = std::make_unique<juce::GenericAudioProcessorEditor> (*slot->instance);
        }
    }

    const auto title = slot->metadata.name + (generic ? " - Parameters" : " - Plugin GUI");
    auto window = std::make_unique<EditorWindow> (slot->runtimeId, generic, title, std::move (editor));
    window->setVisible (true);
    window->toFront (true);
    editorWindows.push_back (std::move (window));
    if (ui != nullptr) ui->setState (makeUiState());
}

juce::var HostRuntime::handleOscCommand (const aph::osc::Command& command)
{
    using Type = aph::osc::Command::Type;
    if (command.type == Type::quit) { stop(); return objectWith ("status", "stopping"); }
    if (command.type == Type::panic) { panic(); return objectWith ("status", "ok"); }
    if (command.type == Type::stateGet) return makeUiState().lastStatus;
    if (command.type == Type::midiNoteOn)
        static_cast<void> (midiScheduler.scheduleForNextBlock (juce::MidiMessage::noteOn (command.channel, command.note, command.value), command.sampleOffset));
    if (command.type == Type::midiNoteOff)
        static_cast<void> (midiScheduler.scheduleForNextBlock (juce::MidiMessage::noteOff (command.channel, command.note, command.value), command.sampleOffset));
    if (command.type == Type::sourceLevelDb)
    {
        if (! std::isfinite (command.value) || command.value < -120.0f || command.value > 24.0f)
            return objectWith ("error", "source level must be finite and between -120 and 24 dB");
        pendingSourceLevelDb.store (command.value, std::memory_order_relaxed);
        pendingSourceChanges.fetch_or (2u, std::memory_order_release);
    }
    if (command.type == Type::sourceFrequency)
    {
        if (! std::isfinite (command.value) || command.value < 0.0f || command.value > activeSampleRate * 0.5)
            return objectWith ("error", "source frequency must be between 0 and Nyquist");
        pendingSourceFrequencyHz.store (command.value, std::memory_order_relaxed);
        pendingSourceChanges.fetch_or (4u, std::memory_order_release);
    }
    if (command.type == Type::sourceType)
    {
        InputSourceType type {};
        if (! HostConfigParser::parseInputSourceType (command.text.data(), type)
            || type == InputSourceType::pinkNoise || type == InputSourceType::sweep)
            return objectWith ("error", "unsupported P0 source type");
        if (type == InputSourceType::file && config.source.type != InputSourceType::file)
            return objectWith ("error", "file source must be preloaded by CLI/session");
        if (type == InputSourceType::mic && config.source.type != InputSourceType::mic)
            return objectWith ("error", "mic input channels must be opened at startup");
        pendingSourceType.store (static_cast<int> (type), std::memory_order_relaxed);
        pendingSourceChanges.fetch_or (1u, std::memory_order_release);
    }
    if (command.type == Type::pluginBypass) pluginChain.setBypassed (command.pluginIndex, command.value != 0.0f);
    if (command.type == Type::pluginParameterGet) return getPluginParameter (command.pluginIndex, command.parameterId.data());
    if (command.type == Type::pluginParameterSet) setPluginParameter (command.pluginIndex, command.parameterId.data(), command.value);
    if (command.type == Type::captureStart) capture.start (juce::File (command.text.data()), activeSampleRate, activeChannels);
    if (command.type == Type::captureStop) capture.stop();
    return objectWith ("status", "ok");
}

juce::var HostRuntime::getPluginParameter (int index, const juce::String& id) const
{
    auto* slot = pluginChain.getSlot (index);
    if (slot == nullptr || slot->instance == nullptr)
        return objectWith ("error", "plugin index is out of range");

    for (auto* parameter : slot->instance->getParameters())
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);
            identified != nullptr && identified->paramID == id)
        {
            auto* result = new juce::DynamicObject();
            result->setProperty ("parameterId", id);
            result->setProperty ("normalizedValue", parameter->getValue());
            result->setProperty ("displayValue", parameter->getCurrentValueAsText());
            return juce::var (result);
        }

    return objectWith ("error", "parameterId was not found");
}

void HostRuntime::applyPendingSourceChanges() noexcept
{
    const auto changes = pendingSourceChanges.exchange (0u, std::memory_order_acquire);
    if ((changes & 1u) != 0u)
    {
        const auto type = static_cast<InputSourceType> (pendingSourceType.load (std::memory_order_relaxed));
        if (type == InputSourceType::mic || source.setType (sourceTypeFor (type)))
            activeSourceType.store (static_cast<int> (type), std::memory_order_relaxed);
    }
    if ((changes & 2u) != 0u)
    {
        const auto level = pendingSourceLevelDb.load (std::memory_order_relaxed);
        source.setLevelDb (level);
        activeSourceLevelDb.store (level, std::memory_order_relaxed);
    }
    if ((changes & 4u) != 0u)
    {
        const auto frequency = pendingSourceFrequencyHz.load (std::memory_order_relaxed);
        source.setFrequencyHz (frequency);
        activeSourceFrequencyHz.store (frequency, std::memory_order_relaxed);
    }
}

bool HostRuntime::hasTimedOut() const noexcept
{
    if (state == State::ready || state == State::running || state == State::completed || state == State::failed)
        return false;

    const auto elapsed = juce::Time::highResolutionTicksToSeconds (
        juce::Time::getHighResolutionTicks() - startedTicks);
    return config.timeoutSeconds > 0.0 && elapsed >= config.timeoutSeconds;
}

void HostRuntime::setPluginParameter (int index, const juce::String& id, float normalised)
{
    if (auto* slot = pluginChain.getSlot (index); slot != nullptr && slot->instance != nullptr)
        for (auto* parameter : slot->instance->getParameters())
            if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter); identified != nullptr && identified->paramID == id)
                parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

agent_plugin_host::audio::SourceType HostRuntime::sourceTypeFor (InputSourceType type)
{
    using Result = agent_plugin_host::audio::SourceType;
    switch (type)
    {
        case InputSourceType::silence:
        case InputSourceType::mic:
        case InputSourceType::pinkNoise:
        case InputSourceType::sweep: return Result::silence;
        case InputSourceType::sine: return Result::sine;
        case InputSourceType::whiteNoise: return Result::whiteNoise;
        case InputSourceType::impulse: return Result::impulse;
        case InputSourceType::file: return Result::file;
    }
    return Result::silence;
}

juce::String HostRuntime::stateName (State value)
{
    switch (value)
    {
        case State::starting: return "starting"; case State::scanning: return "scanning";
        case State::loading: return "loading"; case State::ready: return "ready";
        case State::running: return "running"; case State::stopping: return "stopping";
        case State::completed: return "completed"; case State::failed: return "failed";
    }
    return "unknown";
}

juce::Result HostRuntime::PreloadedAudioFile::load (const juce::File& file, double targetSampleRate, int channels)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (manager.createReaderFor (file));
    if (reader == nullptr) return juce::Result::fail ("Cannot read input file: " + file.getFullPathName());
    if (std::abs (reader->sampleRate - targetSampleRate) > 0.01)
        return juce::Result::fail ("Input file sample rate must match the session sample rate in P0");
    data.setSize (channels, static_cast<int> (reader->lengthInSamples));
    if (! reader->read (&data, 0, data.getNumSamples(), 0, true, true))
        return juce::Result::fail ("Cannot decode input file");
    position = 0;
    return juce::Result::ok();
}

void HostRuntime::PreloadedAudioFile::renderNextBlock (juce::AudioBuffer<float>& destination, int numSamples) noexcept
{
    destination.clear();
    if (data.getNumSamples() == 0) return;
    int written = 0;
    while (written < numSamples)
    {
        const auto count = juce::jmin (numSamples - written, data.getNumSamples() - position);
        for (int channel = 0; channel < destination.getNumChannels(); ++channel)
            destination.copyFrom (channel, written, data, juce::jmin (channel, data.getNumChannels() - 1), position, count);
        written += count;
        position = (position + count) % data.getNumSamples();
    }
}

HostRuntime::EditorWindow::EditorWindow (juce::String id, bool genericEditor, juce::String title,
                                         std::unique_ptr<juce::AudioProcessorEditor> editor)
    : juce::DocumentWindow (std::move (title), juce::Colours::black, closeButton),
      pluginId (std::move (id)), generic (genericEditor)
{
    setUsingNativeTitleBar (true);
    setContentOwned (editor.release(), true);
    centreWithSize (getWidth(), getHeight());
    setResizable (true, false);
}
} // namespace aph::app
