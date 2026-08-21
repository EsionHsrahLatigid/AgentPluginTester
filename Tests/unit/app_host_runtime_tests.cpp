#include "../../Source/app/HostRuntime.h"

namespace aph::app::tests
{
namespace
{
juce::File stagedGainFixture()
{
    auto root = juce::File (APH_SOURCE_DIR).getChildFile ("artifacts/host-release");
   #if JUCE_MAC
    return root.getChildFile ("macos-universal/fixtures/aph_test_gain.vst3");
   #elif JUCE_WINDOWS
    return root.getChildFile ("windows-x64/fixtures/aph_test_gain.vst3");
   #else
    return root.getChildFile ("linux-x64/fixtures/aph_test_gain.vst3");
   #endif
}
} // namespace

class HostRuntimeUiLoadingTests final : public juce::UnitTest
{
public:
    HostRuntimeUiLoadingTests() : juce::UnitTest ("HostRuntimeUiLoading", "app") {}

    void runTest() override
    {
        beginTest ("a staged VST3 can be added after preparation");
        const auto fixture = stagedGainFixture();
        if (! fixture.exists())
        {
            logMessage ("Skipped because the host-release fixture is not staged: " + fixture.getFullPathName());
            return;
        }

        HostConfig config;
        config.mode = RunMode::offline;
        config.gui = false;
        config.oscPort = 0;
        config.eventsTarget.clear();
        config.audio.sampleRate = 48000.0;
        config.audio.blockSize = 256;
        config.audio.outputChannels = 2;

        HostRuntime runtime (config);
        expect (runtime.prepare().wasOk());
        expectEquals (runtime.getPluginCount(), 0);

        runtime.addPlugins ({ fixture.getFullPathName() });

        expectEquals (runtime.getPluginCount(), 1);
        expectEquals (runtime.getConfig().plugins.size(), 1);
        expectEquals (runtime.getReport().plugins.size(), 1);
        expect (runtime.getReport().plugins[0].loaded);
        expect (runtime.getReport().plugins[0].path == fixture.getFullPathName());
        expect (runtime.getReport().plugins[0].format == "VST3");
    }
};

static HostRuntimeUiLoadingTests hostRuntimeUiLoadingTests;
} // namespace aph::app::tests
