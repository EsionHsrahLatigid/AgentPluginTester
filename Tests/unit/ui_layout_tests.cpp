#include "../../Source/ui/AgentPluginHostUI.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace agentpluginhost::ui::tests
{
class HostUiLayoutTests final : public juce::UnitTest
{
public:
    HostUiLayoutTests() : juce::UnitTest ("HostUiLayout", "ui") {}

    void runTest() override
    {
        beginTest ("large test-console defaults remain explicit");
        expectEquals (HostMainComponent::defaultWidth, 1080);
        expectEquals (HostMainComponent::defaultHeight, 720);
        expectGreaterOrEqual (HostMainComponent::minimumWidth, 900);
        expectGreaterOrEqual (HostMainComponent::minimumHeight, 600);

        beginTest ("plugin test controls remain visible and clickable at minimum size");
        HostMainComponent component;
        component.setBounds (0, 0, HostMainComponent::minimumWidth, HostMainComponent::minimumHeight);

        HostUiActions actions;
        actions.toggleBypass = [] (int) {};
        actions.showNativeEditor = [] (int) {};
        actions.showGenericEditor = [] (int) {};
        actions.movePlugin = [] (int, int) {};
        component.setActions (std::move (actions));

        HostUiState state;
        for (int index = 0; index < 8; ++index)
        {
            PluginSlotState plugin;
            plugin.index = index;
            plugin.name = "Test Plugin " + juce::String (index);
            plugin.vendor = "EsionHsrahLatigid";
            plugin.version = "1.0";
            state.plugins.push_back (std::move (plugin));
        }
        component.setState (std::move (state));

        for (int row = 0; row < 8; ++row)
        {
            for (const auto& suffix : { "bypass", "editor.native", "editor.generic", "move.up", "move.down" })
            {
                const auto id = "plugin." + juce::String (row) + "." + suffix;
                auto* child = component.findChildWithID (id);
                expect (child != nullptr, "Missing automation target " + id);
                if (child == nullptr) continue;
                expect (child->isVisible(), "Hidden automation target " + id);
                expectGreaterOrEqual (child->getWidth(), 32, "Control too narrow: " + id);
                expectGreaterOrEqual (child->getHeight(), 28, "Control too short: " + id);
                expect (component.getLocalBounds().contains (child->getBounds()), "Control outside host: " + id);
            }
        }

        beginTest ("native and generic editor actions use discoverable labels");
        auto* native = dynamic_cast<juce::Button*> (component.findChildWithID ("plugin.0.editor.native"));
        auto* generic = dynamic_cast<juce::Button*> (component.findChildWithID ("plugin.0.editor.generic"));
        expect (native != nullptr && native->getButtonText().containsIgnoreCase ("GUI"));
        expect (generic != nullptr && generic->getButtonText().containsIgnoreCase ("PARAM"));
    }
};

static HostUiLayoutTests hostUiLayoutTests;
} // namespace agentpluginhost::ui::tests
