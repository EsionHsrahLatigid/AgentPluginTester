#include "HostRuntime.h"

#include <JuceHeader.h>
#include <iostream>

namespace
{
juce::var describePlugin (const juce::PluginDescription& description)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("name", description.name);
    object->setProperty ("descriptiveName", description.descriptiveName);
    object->setProperty ("format", description.pluginFormatName);
    object->setProperty ("manufacturer", description.manufacturerName);
    object->setProperty ("version", description.version);
    object->setProperty ("category", description.category);
    object->setProperty ("classId", description.createIdentifierString());
    object->setProperty ("isInstrument", description.isInstrument);
    object->setProperty ("inputChannels", description.numInputChannels);
    object->setProperty ("outputChannels", description.numOutputChannels);
    return juce::var (object);
}

juce::var listDevices()
{
    auto* root = new juce::DynamicObject();
    juce::AudioDeviceManager manager;
    juce::Array<juce::var> audioTypes;
    for (auto* type : manager.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        auto* item = new juce::DynamicObject();
        item->setProperty ("type", type->getTypeName());
        item->setProperty ("inputs", juce::var (type->getDeviceNames (true)));
        item->setProperty ("outputs", juce::var (type->getDeviceNames (false)));
        audioTypes.add (juce::var (item));
    }
    juce::Array<juce::var> midiInputs;
    for (const auto& info : juce::MidiInput::getAvailableDevices())
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("name", info.name);
        item->setProperty ("identifier", info.identifier);
        midiInputs.add (juce::var (item));
    }
    root->setProperty ("schemaVersion", "1.0");
    root->setProperty ("audio", audioTypes);
    root->setProperty ("midiInputs", midiInputs);
    return juce::var (root);
}

class AgentPluginHostApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "AgentPluginHost"; }
    const juce::String getApplicationVersion() override { return APH_VERSION; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        auto arguments = juce::JUCEApplicationBase::getCommandLineParameterArray();
        const auto parsed = aph::HostConfigParser::parseCommandLine (arguments);
        if (! parsed.validation.ok())
        {
            for (const auto& issue : parsed.validation.errors)
                std::cerr << issue.path << ": " << issue.message << '\n';
            finishOnMessageLoop (aph::ExitCode::invalidConfiguration);
            return;
        }
        if (parsed.config.listDevices)
        {
            std::cout << juce::JSON::toString (listDevices(), true).toStdString() << '\n';
            finishOnMessageLoop (aph::ExitCode::success);
            return;
        }
        if (parsed.config.inspectPluginPath != juce::File())
        {
            agent_plugin_host::plugins::PluginLoader loader;
            auto scan = loader.scanSinglePluginFile (parsed.config.inspectPluginPath);
            auto* result = new juce::DynamicObject();
            juce::Array<juce::var> descriptions;
            for (const auto* description : scan.descriptions) descriptions.add (describePlugin (*description));
            result->setProperty ("schemaVersion", "1.0");
            result->setProperty ("path", parsed.config.inspectPluginPath.getFullPathName());
            result->setProperty ("plugins", descriptions);
            result->setProperty ("passed", scan.succeeded());
            result->setProperty ("error", scan.error);
            std::cout << juce::JSON::toString (juce::var (result), true).toStdString() << '\n';
            finishOnMessageLoop (scan.succeeded() ? aph::ExitCode::success : aph::ExitCode::pluginLoadFailure);
            return;
        }
        if (parsed.shouldExitImmediately)
        {
            std::cout << parsed.message.toStdString();
            finishOnMessageLoop (parsed.exitCode);
            return;
        }

        runtime = std::make_unique<aph::app::HostRuntime> (parsed.config);
        runtime->setCompletionHandler ([this] (aph::ExitCode code)
        {
            setApplicationReturnValue (static_cast<int> (code));
            quit();
        });
        if (const auto result = runtime->prepare(); result.failed())
        {
            std::cerr << result.getErrorMessage() << '\n';
            const auto code = runtime->getExitCode();
            finishOnMessageLoop (code == aph::ExitCode::success ? aph::ExitCode::pluginLoadFailure : code);
            return;
        }

        if (parsed.config.gui)
        {
            window = std::make_unique<agentpluginhost::ui::HostMainWindow> (
                "AgentPluginHost", agentpluginhost::ui::HostUiActions {}, agentpluginhost::ui::HostUiState {});
            runtime->attachUi (&window->getHostComponent());
        }

        if (parsed.config.mode == aph::RunMode::offline)
        {
            juce::MessageManager::callAsync ([this]
            {
                if (runtime != nullptr)
                    runtime->runOffline();
            });
        }
        else if (const auto result = runtime->startRealtime(); result.failed())
        {
            std::cerr << result.getErrorMessage() << '\n';
            finishOnMessageLoop (aph::ExitCode::deviceInitialisationFailure);
        }
    }

    void shutdown() override
    {
        if (runtime != nullptr && runtime->getState() != aph::app::HostRuntime::State::completed
            && runtime->getState() != aph::app::HostRuntime::State::failed)
            runtime->stop();
        window.reset();
        runtime.reset();
    }

    void systemRequestedQuit() override
    {
        if (runtime != nullptr) runtime->stop(); else quit();
    }

private:
    void finishOnMessageLoop (aph::ExitCode code)
    {
        setApplicationReturnValue (static_cast<int> (code));
        juce::MessageManager::callAsync ([] { quit(); });
    }

    std::unique_ptr<aph::app::HostRuntime> runtime;
    std::unique_ptr<agentpluginhost::ui::HostMainWindow> window;
};
} // namespace

START_JUCE_APPLICATION (AgentPluginHostApplication)
