#include "../plugins/PluginLoader.h"

#include <juce_events/juce_events.h>

#include <iostream>

namespace
{

juce::var descriptionToJson (const juce::PluginDescription& description)
{
    juce::DynamicObject::Ptr object = new juce::DynamicObject();
    object->setProperty ("name", description.name);
    object->setProperty ("descriptiveName", description.descriptiveName);
    object->setProperty ("format", description.pluginFormatName);
    object->setProperty ("manufacturer", description.manufacturerName);
    object->setProperty ("version", description.version);
    object->setProperty ("category", description.category);
    object->setProperty ("fileOrIdentifier", description.fileOrIdentifier);
    object->setProperty ("uniqueId", description.createIdentifierString());
    object->setProperty ("isInstrument", description.isInstrument);
    object->setProperty ("numInputChannels", description.numInputChannels);
    object->setProperty ("numOutputChannels", description.numOutputChannels);
    return juce::var (object.get());
}

void printJson (const juce::var& value)
{
    std::cout << juce::JSON::toString (value, true).toStdString() << std::endl;
}

juce::File markerFromArgs (const juce::StringArray& args)
{
    const auto markerIndex = args.indexOf ("--dead-man");
    if (markerIndex >= 0 && markerIndex + 1 < args.size())
        return juce::File (args[markerIndex + 1]);

    return {};
}

juce::File pluginPathFromArgs (const juce::StringArray& args)
{
    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--dead-man")
        {
            ++i;
            continue;
        }

        return juce::File (args[i]);
    }

    return {};
}

} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    const auto pluginPath = pluginPathFromArgs (args);
    const auto deadManMarker = markerFromArgs (args);

    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("schemaVersion", "1.0");
    root->setProperty ("event", "scan_completed");
    root->setProperty ("path", pluginPath.getFullPathName());

    if (pluginPath == juce::File())
    {
        root->setProperty ("passed", false);
        root->setProperty ("error", "Usage: AgentPluginScanner <plugin-bundle> [--dead-man <marker.json>]");
        printJson (juce::var (root.get()));
        return 2;
    }

    agent_plugin_host::plugins::PluginLoader loader;
    const auto result = loader.scanSinglePluginFile (pluginPath, deadManMarker);

    juce::Array<juce::var> plugins;
    for (const auto* description : result.descriptions)
        plugins.add (descriptionToJson (*description));

    root->setProperty ("plugins", plugins);
    root->setProperty ("passed", result.succeeded());
    root->setProperty ("error", result.error);

    printJson (juce::var (root.get()));
    return result.succeeded() ? 0 : 3;
}
