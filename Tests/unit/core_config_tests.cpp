#include "../../Source/core/HostConfig.h"

#include <juce_core/juce_core.h>

namespace aph::tests
{
class CoreConfigTests final : public juce::UnitTest
{
public:
    CoreConfigTests() : UnitTest("HostConfig", "core") {}

    void runTest() override
    {
        beginTest("CLI defaults and options");
        {
            const auto result = HostConfigParser::parseCommandLine({
                "--plugin", "/tmp/Synth.vst3",
                "--plugin", "/tmp/Delay.vst3",
                "--mode", "offline",
                "--source", "sine",
                "--frequency", "220",
                "--no-gui",
                "--run-seconds", "4",
                "--report", "/tmp/report.json"
            });

            expect(result.validation.ok());
            expectEquals(result.config.plugins.size(), 2);
            expectEquals(HostConfigParser::toString(result.config.mode), juce::String("offline"));
            expectEquals(HostConfigParser::toString(result.config.source.type), juce::String("sine"));
            expectEquals(result.config.source.frequencyHz, 220.0);
            expect(! result.config.gui);
            expectEquals(result.config.runSeconds, 4.0);
            expectEquals(result.config.reportPath.getFullPathName(), juce::File("/tmp/report.json").getFullPathName());
        }

        beginTest("CLI plugin list replaces session plugins");
        {
            auto session = juce::JSON::parse(R"json({
                "schemaVersion": "1.0",
                "mode": "offline",
                "plugins": [{ "path": "FromSession.vst3" }],
                "audio": { "sampleRate": 48000, "blockSize": 256 },
                "report": { "path": "report.json" }
            })json");

            const auto tempSession = juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("agent-plugin-host-session.json");
            expect(tempSession.replaceWithText(juce::JSON::toString(session, true)));

            const auto result = HostConfigParser::parseCommandLine({
                "--session", tempSession.getFullPathName(),
                "--plugin", "/tmp/FromCli.vst3"
            });

            expect(result.validation.ok());
            expectEquals(result.config.plugins.size(), 1);
            expectEquals(result.config.plugins[0].path.getFullPathName(), juce::File("/tmp/FromCli.vst3").getFullPathName());
            expectEquals(result.config.audio.sampleRate, 48000.0);
        }

        beginTest("Session path resolution and unknown-field warning");
        {
            HostConfig config;
            const auto sessionFile = juce::File("/tmp/sessions/example/session.json");
            const auto session = juce::JSON::parse(R"json({
                "schemaVersion": "1.0",
                "unknownTopLevel": true,
                "source": { "type": "file", "file": "input.wav" },
                "plugins": [{ "path": "Plugin.vst3", "bypass": true }],
                "durationSeconds": 1.5,
                "capture": { "path": "output.wav" },
                "report": { "path": "report.json" }
            })json");

            const auto validation = HostConfigParser::mergeSessionJson(config, session, sessionFile);
            const auto sessionDirectory = sessionFile.getParentDirectory();
            expect(validation.errors.isEmpty());
            expectEquals(validation.warnings.size(), 1);
            expectEquals(config.source.inputFile.getFullPathName(), sessionDirectory.getChildFile("input.wav").getFullPathName());
            expectEquals(config.plugins[0].path.getFullPathName(), sessionDirectory.getChildFile("Plugin.vst3").getFullPathName());
            expect(config.plugins[0].bypass);
            expectEquals(config.recordPath.getFullPathName(), sessionDirectory.getChildFile("output.wav").getFullPathName());
            expectEquals(config.reportPath.getFullPathName(), sessionDirectory.getChildFile("report.json").getFullPathName());
        }

        beginTest("Validation rejects invalid ranges and event order");
        {
            HostConfig config;
            config.audio.outputChannels = 0;
            config.oscPort = 70000;
            config.timeoutSeconds = 0.0;
            config.source.type = InputSourceType::file;

            auto late = juce::var(new juce::DynamicObject());
            late.getDynamicObject()->setProperty("atSeconds", 2.0);
            auto early = juce::var(new juce::DynamicObject());
            early.getDynamicObject()->setProperty("atSeconds", 1.0);
            config.scheduledEvents.add(late);
            config.scheduledEvents.add(early);

            const auto validation = HostConfigParser::validate(config);
            expect(! validation.ok());
            expectGreaterOrEqual(validation.errors.size(), 5);
        }
    }
};

static CoreConfigTests coreConfigTests;
} // namespace aph::tests
