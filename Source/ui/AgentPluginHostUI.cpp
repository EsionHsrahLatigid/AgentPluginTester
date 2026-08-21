#include "AgentPluginHostUI.h"
#include "../plugins/PluginLoader.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>
#include <utility>

namespace agentpluginhost::ui
{
namespace
{
const juce::Colour ink { 0xff050505 };
const juce::Colour low { 0xff2a2a2a };
const juce::Colour mid { 0xff8a8a86 };
const juce::Colour paper { 0xfff2f2f0 };

constexpr auto typefaceName = "Departure Mono";

juce::Font makeFont (float height, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions (typefaceName, height, style));
}

void strokeRect (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour, int thickness = 1)
{
    g.setColour (colour);
    for (int i = 0; i < thickness; ++i)
        g.drawRect (area.reduced (i), 1);
}

juce::String yesNo (bool value)
{
    return value ? "ON" : "OFF";
}

juce::String shortPath (const juce::String& path)
{
    if (path.length() <= 30)
        return path;

    return juce::String ("...") + path.substring (path.length() - 27);
}
} // namespace

PixelButton::PixelButton (juce::String buttonText, juce::String automationId)
    : juce::Button (buttonText)
{
    setButtonText (buttonText);
    setComponentID (automationId);
    setWantsKeyboardFocus (true);
    setTooltip (automationId);
}

void PixelButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds();
    const auto active = getToggleState() || down;
    const auto fg = isEnabled() ? (active ? ink : paper) : mid;
    const auto bg = active ? paper : (highlighted ? low : ink);

    g.fillAll (bg);
    strokeRect (g, area, isEnabled() ? paper : mid, highlighted || hasKeyboardFocus (true) ? 2 : 1);

    if (highlighted && ! active)
    {
        g.setColour (paper);
        for (int x = 2; x < area.getWidth(); x += 8)
            g.fillRect (x, area.getBottom() - 3, 4, 1);
    }

    g.setColour (fg);
    g.setFont (makeFont (juce::jmin (13.0f, static_cast<float> (area.getHeight() - 8)), juce::Font::bold));
    g.drawFittedText (getButtonText(), area.reduced (4, 2), juce::Justification::centred, 1);
}

HostMainComponent::HostMainComponent()
{
    setName ("AgentPluginHost UI");
    setComponentID ("host.main");
    setOpaque (true);
    setWantsKeyboardFocus (true);
    setSize (defaultWidth, defaultHeight);

    for (auto* button : { &sourcePrev, &sourceNext, &playButton, &stopButton, &panicButton, &recordButton, &reportButton })
    {
        addAndMakeVisible (button);
        button->addListener (this);
    }

    for (auto row = 0; row < maxPluginRows; ++row)
    {
        auto& controls = pluginButtons[static_cast<size_t> (row)];
        for (auto* button : { &controls.bypass, &controls.editor, &controls.generic, &controls.moveUp, &controls.moveDown })
        {
            addAndMakeVisible (button);
            button->addListener (this);
        }
    }

    applyAutomationIds();
    updateButtonText();
    startTimerHz (10);
}

HostMainComponent::~HostMainComponent()
{
    for (auto* button : { &sourcePrev, &sourceNext, &playButton, &stopButton, &panicButton, &recordButton, &reportButton })
        button->removeListener (this);

    for (auto& controls : pluginButtons)
        for (auto* button : { &controls.bypass, &controls.editor, &controls.generic, &controls.moveUp, &controls.moveDown })
            button->removeListener (this);
}

void HostMainComponent::setActions (HostUiActions newActions)
{
    actions = std::move (newActions);
}

void HostMainComponent::setState (HostUiState newState)
{
    state = std::move (newState);
    updateButtonText();
    resized();
    repaint();
}

bool HostMainComponent::isSupportedPluginPath (const juce::String& path)
{
    return agent_plugin_host::plugins::PluginLoader::isSupportedPluginPath (path);
}

juce::StringArray HostMainComponent::supportedPluginPaths (const juce::StringArray& paths)
{
    juce::StringArray supported;
    for (const auto& path : paths)
        if (isSupportedPluginPath (path))
            supported.addIfNotAlreadyThere (juce::File (path).getFullPathName());
    return supported;
}

void HostMainComponent::requestPluginLoad (const juce::StringArray& paths)
{
    const auto supported = supportedPluginPaths (paths);
    if (! supported.isEmpty() && actions.addPlugins)
        actions.addPlugins (supported);
}

const HostUiState& HostMainComponent::getState() const noexcept
{
    return state;
}

void HostMainComponent::paint (juce::Graphics& g)
{
    g.fillAll (ink);

    drawHeader (g);

    drawSection (g, sourceBounds, "SOURCE");
    drawReadout (g, sourceBounds.withTrimmedTop (24).removeFromTop (24).reduced (8, 0), "TYPE", state.sourceType, true);
    drawReadout (g, sourceBounds.withTrimmedTop (52).removeFromTop (20).reduced (8, 0), "LEVEL", formatDb (state.sourceLevelDb));
    drawReadout (g, sourceBounds.withTrimmedTop (76).removeFromTop (20).reduced (8, 0), "FREQ", juce::String (state.sourceFrequencyHz, 1) + " Hz");
    drawReadout (g, sourceBounds.withTrimmedTop (100).removeFromTop (20).reduced (8, 0), "I/O", juce::String (state.inputChannels) + " > " + juce::String (state.outputChannels));

    drawSection (g, transportBounds, "TRANSPORT");
    drawReadout (g, transportBounds.withTrimmedTop (24).removeFromTop (24).reduced (8, 0), "STATE", state.playing ? "PLAYING" : "STOPPED", state.playing);
    drawReadout (g, transportBounds.withTrimmedTop (52).removeFromTop (20).reduced (8, 0), "BPM", juce::String (state.bpm, 1));
    drawReadout (g, transportBounds.withTrimmedTop (76).removeFromTop (20).reduced (8, 0), "SIGNATURE", juce::String (state.timeSigNumerator) + "/" + juce::String (state.timeSigDenominator));
    drawReadout (g, transportBounds.withTrimmedTop (100).removeFromTop (20).reduced (8, 0), "SAMPLE", juce::String (state.samplePosition));

    drawSection (g, chainBounds, "PLUGIN CHAIN  /  GUI = NATIVE EDITOR  /  PARAMS = GENERIC EDITOR");
    const auto rows = juce::jmin<int> (static_cast<int> (state.plugins.size()), maxPluginRows);
    for (int i = 0; i < rows; ++i)
        drawPluginRow (g, pluginRowBounds[static_cast<size_t> (i)], state.plugins[static_cast<size_t> (i)]);

    if (state.plugins.empty())
    {
        g.setColour (mid);
        g.setFont (makeFont (11.0f));
        g.drawFittedText ("NO PLUG-IN LOADED\nDROP VST3 / AU HERE  /  USE THE PLUGIN MENU",
                          chainBounds.reduced (16).withTrimmedTop (24), juce::Justification::centred, 2);
    }

    drawSection (g, meterBounds, "METERS");
    auto meters = meterBounds.reduced (8).withTrimmedTop (20);
    const auto meterHeight = juce::jmax (24, (meters.getHeight() - 16) / 3);
    drawMeter (g, meters.removeFromTop (meterHeight), "INPUT", state.inputMeter);
    meters.removeFromTop (8);
    drawMeter (g, meters.removeFromTop (meterHeight), "CHAIN", state.chainMeter);
    meters.removeFromTop (8);
    drawMeter (g, meters.removeFromTop (meterHeight), "OUTPUT", state.outputMeter);

    drawSection (g, oscBounds, "OSC");
    drawReadout (g, oscBounds.withTrimmedTop (24).removeFromTop (20).reduced (8, 0), "BIND", state.oscBind + ":" + juce::String (state.oscPort));
    drawReadout (g, oscBounds.withTrimmedTop (48).removeFromTop (20).reduced (8, 0), "RECEIVED", juce::String (state.oscReceivedCount));
    drawReadout (g, oscBounds.withTrimmedTop (72).removeFromTop (20).reduced (8, 0), "DROPPED", juce::String (state.oscRejectedCount + state.oscQueueOverflowCount),
                 state.oscRejectedCount + state.oscQueueOverflowCount > 0);
    drawReadout (g, oscBounds.withTrimmedTop (96).removeFromTop (20).reduced (8, 0), "LAST", state.lastOscEvent);

    drawSection (g, reportBounds, "REPORT");
    drawReadout (g, reportBounds.withTrimmedTop (24).removeFromTop (20).reduced (8, 0), "RECORD", yesNo (state.recording), state.recording);
    drawReadout (g, reportBounds.withTrimmedTop (48).removeFromTop (20).reduced (8, 0), "JSON", state.reportStatus);
    drawReadout (g, reportBounds.withTrimmedTop (72).removeFromTop (20).reduced (8, 0), "RESULT", state.reportStatus == "written" ? (state.passed ? "PASS" : "FAIL") : "PENDING", state.reportStatus == "written");
    if (reportBounds.getHeight() >= 164)
        drawReadout (g, reportBounds.withTrimmedTop (96).removeFromTop (20).reduced (8, 0), "PATH", shortPath (state.reportPath));

    drawSection (g, statusBounds, "STATUS");
    drawReadout (g, statusBounds.withTrimmedTop (24).removeFromTop (22).reduced (8, 0), "WARNINGS", juce::String (state.warningCount), state.warningCount > 0);
    drawReadout (g, statusBounds.withTrimmedTop (50).removeFromTop (22).reduced (8, 0), "ERRORS", juce::String (state.errorCount), state.errorCount > 0);
    drawReadout (g, statusBounds.withTrimmedTop (76).removeFromTop (juce::jmax (22, statusBounds.getHeight() - 84)).reduced (8, 0), "LAST", state.lastStatus);

    if (state.errorCount > 0)
        drawDitherWarning (g, statusBounds.reduced (2));

    drawMidiKeyboard (g);

    if (pluginDragActive)
    {
        auto overlay = getLocalBounds().reduced (16);
        g.setColour (ink.withAlpha (0.92f));
        g.fillRect (overlay);
        strokeRect (g, overlay, paper, 4);
        g.setColour (paper);
        g.setFont (makeFont (22.0f, juce::Font::bold));
        g.drawFittedText ("DROP PLUG-IN TO ADD TO CHAIN", overlay.reduced (24), juce::Justification::centred, 1);
    }
}

void HostMainComponent::resized()
{
    auto area = getLocalBounds().reduced (grid * 2);
    const auto gap = grid * 2;

    headerBounds = area.removeFromTop (44);
    area.removeFromTop (gap);
    midiBounds = area.removeFromBottom (112);
    area.removeFromBottom (gap);

    const auto leftWidth = juce::jlimit (208, 248, getWidth() / 5);
    const auto rightWidth = juce::jlimit (248, 288, getWidth() / 4);

    auto left = area.removeFromLeft (leftWidth);
    area.removeFromLeft (gap);
    auto right = area.removeFromRight (rightWidth);
    area.removeFromRight (gap);

    sourceBounds = left.removeFromTop (juce::jlimit (152, 176, left.getHeight() * 38 / 100));
    left.removeFromTop (gap);
    transportBounds = left.removeFromTop (juce::jlimit (144, 168, left.getHeight() * 48 / 100));
    left.removeFromTop (gap);
    statusBounds = left;

    meterBounds = right.removeFromTop (juce::jlimit (132, 208, right.getHeight() * 34 / 100));
    right.removeFromTop (gap);
    oscBounds = right.removeFromTop (juce::jlimit (128, 152, right.getHeight() * 42 / 100));
    right.removeFromTop (gap);
    reportBounds = right;

    chainBounds = area;

    sourcePrev.setBounds (sourceBounds.getX() + 8, sourceBounds.getBottom() - 36, 40, 28);
    sourceNext.setBounds (sourceBounds.getRight() - 48, sourceBounds.getBottom() - 36, 40, 28);

    auto transportButtons = transportBounds.reduced (8).removeFromBottom (32);
    playButton.setBounds (transportButtons.removeFromLeft (transportButtons.getWidth() / 3));
    transportButtons.removeFromLeft (grid);
    stopButton.setBounds (transportButtons.removeFromLeft (transportButtons.getWidth() / 2));
    transportButtons.removeFromLeft (grid);
    panicButton.setBounds (transportButtons);

    auto reportButtons = reportBounds.reduced (8).removeFromBottom (32);
    recordButton.setBounds (reportButtons.removeFromLeft (reportButtons.getWidth() / 2));
    reportButtons.removeFromLeft (grid);
    reportButton.setBounds (reportButtons);

    layoutPluginControls();

    auto midiKeys = midiBounds.reduced (8).withTrimmedTop (24).withTrimmedBottom (24);
    const auto keyWidth = midiKeys.getWidth() / midiKeyCount;
    for (int index = 0; index < midiKeyCount; ++index)
    {
        const auto width = index == midiKeyCount - 1 ? midiKeys.getRight() - midiKeys.getX() : keyWidth;
        midiKeyBounds[static_cast<size_t> (index)] = midiKeys.removeFromLeft (width);
    }
}

void HostMainComponent::buttonClicked (juce::Button* button)
{
    if (button == &sourcePrev && actions.setSourceByDelta)
        return actions.setSourceByDelta (-1);
    if (button == &sourceNext && actions.setSourceByDelta)
        return actions.setSourceByDelta (1);
    if (button == &playButton && actions.play)
        return actions.play();
    if (button == &stopButton && actions.stop)
        return actions.stop();
    if (button == &panicButton && actions.panic)
        return actions.panic();
    if (button == &recordButton && actions.toggleRecording)
        return actions.toggleRecording();
    if (button == &reportButton && actions.writeReport)
        return actions.writeReport();

    for (int row = 0; row < maxPluginRows; ++row)
    {
        const auto index = row < static_cast<int> (state.plugins.size()) ? state.plugins[static_cast<size_t> (row)].index : row;
        auto& controls = pluginButtons[static_cast<size_t> (row)];

        if (button == &controls.bypass && actions.toggleBypass)
            return actions.toggleBypass (index);
        if (button == &controls.editor && actions.showNativeEditor)
            return actions.showNativeEditor (index);
        if (button == &controls.generic && actions.showGenericEditor)
            return actions.showGenericEditor (index);
        if (button == &controls.moveUp && actions.movePlugin)
            return actions.movePlugin (index, index - 1);
        if (button == &controls.moveDown && actions.movePlugin)
            return actions.movePlugin (index, index + 1);
    }
}

void HostMainComponent::timerCallback()
{
    constexpr std::array<int, 13> keyCodes { 'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k' };
    for (std::size_t index = 0; index < keyCodes.size(); ++index)
        if (heldNoteKeys[index] && mouseHeldNote != 60 + static_cast<int> (index)
            && ! juce::KeyPress::isKeyCurrentlyDown (keyCodes[index]))
        {
            heldNoteKeys[index] = false;
            if (actions.noteOff) actions.noteOff (60 + static_cast<int> (index), 0.0f);
        }
    repaint();
}

bool HostMainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto note = noteForKeyCode (key.getKeyCode());
    if (note < 0)
        return false;

    const auto index = static_cast<std::size_t> (note - 60);
    if (! heldNoteKeys[index])
    {
        heldNoteKeys[index] = true;
        if (actions.noteOn) actions.noteOn (note, 0.8f);
    }
    return true;
}

void HostMainComponent::mouseDown (const juce::MouseEvent& event)
{
    const auto note = noteAtPosition (event.getPosition());
    if (note < 0)
        return;

    mouseHeldNote = note;
    heldNoteKeys[static_cast<size_t> (note - 60)] = true;
    if (actions.noteOn) actions.noteOn (note, 0.8f);
    repaint (midiBounds);
}

void HostMainComponent::mouseUp (const juce::MouseEvent&)
{
    if (mouseHeldNote < 0)
        return;

    const auto note = mouseHeldNote;
    mouseHeldNote = -1;
    heldNoteKeys[static_cast<size_t> (note - 60)] = false;
    if (actions.noteOff) actions.noteOff (note, 0.0f);
    repaint (midiBounds);
}

bool HostMainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return ! supportedPluginPaths (files).isEmpty();
}

void HostMainComponent::fileDragEnter (const juce::StringArray& files, int, int)
{
    pluginDragActive = isInterestedInFileDrag (files);
    repaint();
}

void HostMainComponent::fileDragExit (const juce::StringArray&)
{
    pluginDragActive = false;
    repaint();
}

void HostMainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    pluginDragActive = false;
    requestPluginLoad (files);
    repaint();
}

int HostMainComponent::noteForKeyCode (int keyCode) noexcept
{
    constexpr std::array<int, 13> keyCodes { 'a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k' };
    for (std::size_t index = 0; index < keyCodes.size(); ++index)
        if (keyCodes[index] == keyCode)
            return 60 + static_cast<int> (index);
    return -1;
}

int HostMainComponent::noteAtPosition (juce::Point<int> position) const noexcept
{
    for (int index = 0; index < midiKeyCount; ++index)
        if (midiKeyBounds[static_cast<size_t> (index)].contains (position))
            return 60 + index;
    return -1;
}

void HostMainComponent::applyAutomationIds()
{
    for (int row = 0; row < maxPluginRows; ++row)
    {
        auto& controls = pluginButtons[static_cast<size_t> (row)];
        const auto prefix = juce::String ("plugin.") + juce::String (row) + ".";
        controls.bypass.setComponentID (prefix + "bypass");
        controls.editor.setComponentID (prefix + "editor.native");
        controls.generic.setComponentID (prefix + "editor.generic");
        controls.moveUp.setComponentID (prefix + "move.up");
        controls.moveDown.setComponentID (prefix + "move.down");
    }
}

void HostMainComponent::layoutPluginControls()
{
    auto rowsArea = chainBounds.reduced (8).withTrimmedTop (20);
    const auto rowHeight = juce::jlimit (44, 60, (rowsArea.getHeight() - (maxPluginRows - 1) * grid) / maxPluginRows);

    for (int row = 0; row < maxPluginRows; ++row)
    {
        auto rowBounds = rowsArea.removeFromTop (rowHeight);
        pluginRowBounds[static_cast<size_t> (row)] = rowBounds;
        rowsArea.removeFromTop (grid);

        auto controlsArea = rowBounds.removeFromRight (236).withSizeKeepingCentre (236, 32);
        auto& controls = pluginButtons[static_cast<size_t> (row)];
        controls.bypass.setBounds (controlsArea.removeFromLeft (48));
        controlsArea.removeFromLeft (grid);
        controls.editor.setBounds (controlsArea.removeFromLeft (44));
        controlsArea.removeFromLeft (grid);
        controls.generic.setBounds (controlsArea.removeFromLeft (64));
        controlsArea.removeFromLeft (grid);
        controls.moveUp.setBounds (controlsArea.removeFromLeft (32));
        controlsArea.removeFromLeft (grid);
        controls.moveDown.setBounds (controlsArea.removeFromLeft (32));

        const auto enabled = row < static_cast<int> (state.plugins.size());
        controls.bypass.setVisible (enabled);
        controls.editor.setVisible (enabled);
        controls.generic.setVisible (enabled);
        controls.moveUp.setVisible (enabled);
        controls.moveDown.setVisible (enabled);
        controls.moveUp.setEnabled (enabled && actions.movePlugin != nullptr && row > 0);
        controls.moveDown.setEnabled (enabled && actions.movePlugin != nullptr
                                      && row + 1 < static_cast<int> (state.plugins.size()));
    }
}

void HostMainComponent::updateButtonText()
{
    playButton.setToggleState (state.playing, juce::dontSendNotification);
    recordButton.setToggleState (state.recording, juce::dontSendNotification);
    reportButton.setEnabled (actions.writeReport != nullptr);
    sourcePrev.setEnabled (actions.setSourceByDelta != nullptr);
    sourceNext.setEnabled (actions.setSourceByDelta != nullptr);

    for (int row = 0; row < maxPluginRows; ++row)
    {
        const auto enabled = row < static_cast<int> (state.plugins.size());
        auto& controls = pluginButtons[static_cast<size_t> (row)];

        if (enabled)
        {
            const auto& plugin = state.plugins[static_cast<size_t> (row)];
            controls.bypass.setToggleState (plugin.bypassed, juce::dontSendNotification);
            controls.editor.setToggleState (plugin.editorVisible, juce::dontSendNotification);
            controls.generic.setToggleState (plugin.genericEditorVisible, juce::dontSendNotification);
            controls.editor.setButtonText (plugin.editorVisible ? "GUI ON" : "GUI");
            controls.generic.setButtonText (plugin.genericEditorVisible ? "PARAM ON" : "PARAMS");
            controls.bypass.setEnabled (actions.toggleBypass != nullptr);
            controls.editor.setEnabled (actions.showNativeEditor != nullptr);
            controls.generic.setEnabled (actions.showGenericEditor != nullptr);
        }
        else
        {
            controls.bypass.setToggleState (false, juce::dontSendNotification);
            controls.editor.setToggleState (false, juce::dontSendNotification);
            controls.generic.setToggleState (false, juce::dontSendNotification);
            controls.editor.setButtonText ("GUI");
            controls.generic.setButtonText ("PARAMS");
        }
    }
}

void HostMainComponent::drawHeader (juce::Graphics& g) const
{
    g.setColour (ink);
    g.fillRect (headerBounds);
    strokeRect (g, headerBounds, paper, 2);

    auto content = headerBounds.reduced (10, 4);
    auto result = content.removeFromRight (156);
    content.removeFromRight (8);
    auto title = content.removeFromLeft (224);

    g.setColour (paper);
    g.setFont (makeFont (17.0f, juce::Font::bold));
    g.drawFittedText ("AGENTPLUGINHOST", title, juce::Justification::centredLeft, 1);

    g.setColour (mid);
    g.setFont (makeFont (11.0f));
    const auto session = state.sessionName + "  /  " + state.mode.toUpperCase()
                       + "  /  " + juce::String (state.sampleRate) + " Hz / " + juce::String (state.blockSize)
                       + "  /  " + juce::String (state.plugins.size()) + " PLUGINS";
    g.drawFittedText (session, content, juce::Justification::centredLeft, 1);

    const auto complete = state.reportStatus == "written";
    const auto success = complete && state.passed;
    g.setColour (success ? paper : low);
    g.fillRect (result);
    strokeRect (g, result, paper);
    g.setColour (success ? ink : paper);
    g.setFont (makeFont (11.0f, juce::Font::bold));
    g.drawFittedText (complete ? (success ? "RESULT PASS" : "RESULT FAIL") : "TEST RUNNING",
                      result.reduced (4), juce::Justification::centred, 1);
}

void HostMainComponent::drawMidiKeyboard (juce::Graphics& g) const
{
    constexpr std::array<const char*, midiKeyCount> keyLabels { "A", "W", "S", "E", "D", "F", "T", "G", "Y", "H", "U", "J", "K" };
    drawSection (g, midiBounds, "MIDI TEST KEYBOARD  /  CLICK OR USE COMPUTER KEYS  /  C4-C5");

    for (int index = 0; index < midiKeyCount; ++index)
    {
        auto key = midiKeyBounds[static_cast<size_t> (index)].reduced (2, 0);
        const auto active = heldNoteKeys[static_cast<size_t> (index)];
        g.setColour (active ? paper : low);
        g.fillRect (key);
        strokeRect (g, key, paper, active ? 2 : 1);
        g.setColour (active ? ink : paper);
        g.setFont (makeFont (12.0f, juce::Font::bold));
        g.drawFittedText (juce::String (keyLabels[static_cast<size_t> (index)]) + "\n" + juce::MidiMessage::getMidiNoteName (60 + index, true, true, 3),
                          key.reduced (4), juce::Justification::centred, 2);
    }

    auto footer = midiBounds.reduced (8).removeFromBottom (20);
    g.setColour (mid);
    g.setFont (makeFont (10.0f));
    const auto detail = "OSC " + state.lastOscEvent + "  /  REPORT " + shortPath (state.reportPath);
    g.drawFittedText (detail, footer, juce::Justification::centredLeft, 1);
}

void HostMainComponent::drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title) const
{
    g.setColour (ink);
    g.fillRect (area);
    strokeRect (g, area, low);

    g.setColour (paper);
    g.fillRect (area.getX(), area.getY(), area.getWidth(), 20);
    g.setColour (ink);
    g.setFont (makeFont (10.0f, juce::Font::bold));
    g.drawFittedText (title, area.withHeight (20).reduced (8, 0), juce::Justification::centredLeft, 1);
}

void HostMainComponent::drawReadout (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label, const juce::String& value, bool inverted) const
{
    const auto labelWidth = juce::jmin (72, area.getWidth() / 2);

    g.setColour (inverted ? paper : low);
    g.fillRect (area);
    g.setColour (inverted ? ink : mid);
    g.setFont (makeFont (10.0f, juce::Font::bold));
    g.drawFittedText (label, area.removeFromLeft (labelWidth).reduced (4, 0), juce::Justification::centredLeft, 1);

    g.setColour (inverted ? ink : paper);
    g.setFont (makeFont (11.0f));
    g.drawFittedText (value, area.reduced (4, 0), juce::Justification::centredRight, 1);
}

void HostMainComponent::drawMeter (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label, const MeterSnapshot& meter) const
{
    g.setColour (low);
    g.fillRect (area);
    strokeRect (g, area, mid);

    auto content = area.reduced (6, 4);
    auto labelArea = content.removeFromLeft (juce::jlimit (24, 48, content.getWidth() / 4));
    g.setColour (paper);
    g.setFont (makeFont (10.0f, juce::Font::bold));
    g.drawFittedText (label, labelArea, juce::Justification::centredLeft, 1);

    auto valueArea = content.removeFromRight (juce::jlimit (38, 88, content.getWidth() / 2));
    g.setColour (mid);
    g.setFont (makeFont (9.0f));
    const auto value = area.getWidth() >= 180
                     ? "P " + formatDb (meter.peakDbfs) + "  R " + formatDb (meter.rmsDbfs)
                     : formatDb (meter.peakDbfs);
    g.drawFittedText (value, valueArea, juce::Justification::centredRight, 1);

    auto bar = content.reduced (0, 4);
    g.setColour (ink);
    g.fillRect (bar);
    strokeRect (g, bar, mid);

    const auto filled = static_cast<int> (static_cast<float> (bar.getWidth()) * meterNormalised (meter.peakDbfs));
    g.setColour (paper);
    for (int x = 0; x < filled; x += 4)
        g.fillRect (bar.getX() + x, bar.getY(), juce::jmin (2, filled - x), bar.getHeight());

    if (meter.nonFiniteCount > 0 || meter.clippedSampleCount > 0)
        drawDitherWarning (g, area.reduced (1));
}

void HostMainComponent::drawPluginRow (juce::Graphics& g, juce::Rectangle<int> area, const PluginSlotState& plugin) const
{
    const auto hasProblem = plugin.loadState.equalsIgnoreCase ("failed") || plugin.loadState.equalsIgnoreCase ("timeout")
                            || plugin.meter.nonFiniteCount > 0;

    g.setColour (plugin.bypassed ? low : ink);
    g.fillRect (area);
    strokeRect (g, area, hasProblem ? paper : low, hasProblem ? 2 : 1);

    auto text = area.reduced (8, 4);
    text.removeFromRight (240);

    auto indexArea = text.removeFromLeft (28);
    g.setColour (plugin.bypassed ? mid : paper);
    g.setFont (makeFont (13.0f, juce::Font::bold));
    g.drawFittedText (juce::String (plugin.index), indexArea, juce::Justification::centred, 1);

    const auto meterWidth = juce::jlimit (88, 140, text.getWidth() / 3);
    auto meterArea = text.removeFromRight (meterWidth);
    drawMeter (g, meterArea.withSizeKeepingCentre (meterWidth, juce::jmin (28, area.getHeight() - 12)), "FX", plugin.meter);
    text.removeFromRight (8);

    g.setColour (plugin.bypassed ? mid : paper);
    g.setFont (makeFont (12.0f, juce::Font::bold));
    g.drawFittedText (plugin.name, text.removeFromTop (20), juce::Justification::centredLeft, 1);

    g.setColour (mid);
    g.setFont (makeFont (10.0f));
    const auto meta = plugin.vendor + " " + plugin.version + " / " + plugin.format + " / "
                      + plugin.loadState.toUpperCase() + " / " + juce::String (plugin.latencySamples) + " spl"
                      + (plugin.editorVisible ? " / GUI OPEN" : "")
                      + (plugin.genericEditorVisible ? " / PARAMS OPEN" : "");
    g.drawFittedText (meta, text, juce::Justification::centredLeft, 1);
}

void HostMainComponent::drawDitherWarning (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (paper);
    for (int y = area.getY(); y < area.getBottom(); y += 8)
        for (int x = area.getX() + ((y / 8) % 2) * 4; x < area.getRight(); x += 8)
            g.fillRect (x, y, 2, 2);
}

juce::String HostMainComponent::formatDb (float db)
{
    if (db <= -119.5f)
        return "-inf";

    return juce::String (db, 1) + "dB";
}

float HostMainComponent::meterNormalised (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

HostMainWindow::HostMainWindow (juce::String name, HostUiActions actions, HostUiState initialState)
    : juce::DocumentWindow (name, ink, juce::DocumentWindow::allButtons)
{
    menuLookAndFeel.setColour (juce::PopupMenu::backgroundColourId, ink);
    menuLookAndFeel.setColour (juce::PopupMenu::textColourId, paper);
    menuLookAndFeel.setColour (juce::PopupMenu::headerTextColourId, mid);
    menuLookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, paper);
    menuLookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setLookAndFeel (&menuLookAndFeel);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (HostMainComponent::minimumWidth, HostMainComponent::minimumHeight,
                     HostMainComponent::maximumWidth, HostMainComponent::maximumHeight);

    auto component = std::make_unique<HostMainComponent>();
    hostComponent = component.get();
    hostComponent->setActions (std::move (actions));
    hostComponent->setState (std::move (initialState));
    setContentOwned (component.release(), true);
    setMenuBar (this, 28);
    centreWithSize (HostMainComponent::defaultWidth, HostMainComponent::defaultHeight);
    setVisible (true);
    hostComponent->grabKeyboardFocus();
}

HostMainWindow::~HostMainWindow()
{
    pluginChooser.reset();
    setMenuBar (nullptr);
    setLookAndFeel (nullptr);
}

HostMainComponent& HostMainWindow::getHostComponent() noexcept
{
    return *hostComponent;
}

void HostMainWindow::closeButtonPressed()
{
    setVisible (false);
}

juce::StringArray HostMainWindow::getMenuBarNames()
{
    return { "PLUGIN" };
}

juce::PopupMenu HostMainWindow::getMenuForIndex (int topLevelMenuIndex, const juce::String&)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&menuLookAndFeel);
    if (topLevelMenuIndex == 0)
    {
        menu.addItem (addVst3MenuItemId, "ADD VST3...");
       #if JUCE_MAC
        menu.addItem (addAudioUnitMenuItemId, "ADD AUDIO UNIT...");
       #endif
    }
    return menu;
}

void HostMainWindow::menuItemSelected (int menuItemId, int)
{
    if (menuItemId == addVst3MenuItemId)
        showPluginChooser (PluginChoice::vst3);
   #if JUCE_MAC
    else if (menuItemId == addAudioUnitMenuItemId)
        showPluginChooser (PluginChoice::audioUnit);
   #endif
}

void HostMainWindow::showPluginChooser (PluginChoice choice)
{
    if (pluginChooser != nullptr)
        return;

    auto& lastDirectory = choice == PluginChoice::audioUnit ? lastAudioUnitDirectory : lastVst3Directory;
    if (lastDirectory == juce::File())
    {
        const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
       #if JUCE_MAC
        const auto formatDirectory = choice == PluginChoice::audioUnit
                                   ? home.getChildFile ("Library/Audio/Plug-Ins/Components")
                                   : home.getChildFile ("Library/Audio/Plug-Ins/VST3");
        lastDirectory = formatDirectory.isDirectory() ? formatDirectory : home;
       #else
        lastDirectory = home;
       #endif
    }

    const auto title = choice == PluginChoice::audioUnit ? "ADD AUDIO UNIT PLUG-INS" : "ADD VST3 PLUG-INS";
    const auto wildcard = choice == PluginChoice::audioUnit ? "*.component" : "*.vst3";

    pluginChooser = std::make_unique<juce::FileChooser> (
        title, lastDirectory, wildcard, true, false, this);

    constexpr auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::canSelectDirectories
                         | juce::FileBrowserComponent::canSelectMultipleItems;
    juce::Component::SafePointer<HostMainWindow> safeThis (this);
    pluginChooser->launchAsync (flags, [safeThis, choice] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        const auto results = chooser.getResults();
        juce::StringArray paths;
        for (const auto& result : results)
            paths.add (result.getFullPathName());

        if (! results.isEmpty())
        {
            auto& remembered = choice == PluginChoice::audioUnit
                             ? safeThis->lastAudioUnitDirectory
                             : safeThis->lastVst3Directory;
            remembered = results.getFirst().getParentDirectory();
        }

        safeThis->hostComponent->requestPluginLoad (paths);
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->pluginChooser.reset();
        });
    });
}

} // namespace agentpluginhost::ui
