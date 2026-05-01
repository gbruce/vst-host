#include <array>

#include "MainComponent.h"

namespace
{
constexpr auto audioDeviceStateKey = "audioDeviceState";
constexpr auto enabledMidiInputsKey = "enabledMidiInputs";
constexpr auto midiInputsConfiguredKey = "midiInputsConfigured";
constexpr auto lastPluginDirectoryKey = "lastPluginDirectory";
constexpr double defaultSampleRate = 44100.0;
constexpr int defaultBufferSize = 512;

juce::String joinEnabledMidiInputs(juce::AudioDeviceManager& deviceManager)
{
    juce::StringArray enabledInputs;

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled(device.identifier))
            enabledInputs.add(device.identifier);

    return enabledInputs.joinIntoString(";");
}

juce::File getDefaultPluginDirectory()
{
    const juce::File defaultDirectory(R"(C:\Program Files\Common Files\VST3)");

    if (defaultDirectory.exists())
        return defaultDirectory;

    const juce::File x86Directory(R"(C:\Program Files (x86)\Common Files\VST3)");

    if (x86Directory.exists())
        return x86Directory;

    return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

juce::String yesNo(bool value)
{
    return value ? "Yes" : "No";
}
}

MainComponent::MainComponent(juce::ApplicationProperties& applicationProperties)
    : appProperties(applicationProperties),
      deviceSelector(deviceManager, 0, 0, 0, 2, true, false, true, false)
{
    pluginFormatManager.addDefaultFormats();

    titleLabel.setText("VST Host", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));

    summaryLabel.setText("Single-plugin JUCE host for loading one VST3 instrument at a time.", juce::dontSendNotification);
    summaryLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    audioStatusLabel.setJustificationType(juce::Justification::topLeft);
    audioStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    midiStatusLabel.setJustificationType(juce::Justification::topLeft);
    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    roadmapLabel.setJustificationType(juce::Justification::topLeft);
    roadmapLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    roadmapLabel.setText("Milestone 2: VST3 file loading is active. Next: embed the editor, expose parameters, and route note input.", juce::dontSendNotification);

    pluginStatusLabel.setJustificationType(juce::Justification::topLeft);
    pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    pluginDetailsEditor.setMultiLine(true);
    pluginDetailsEditor.setReadOnly(true);
    pluginDetailsEditor.setScrollbarsShown(true);
    pluginDetailsEditor.setCaretVisible(false);
    pluginDetailsEditor.setPopupMenuEnabled(false);
    pluginDetailsEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff101217));
    pluginDetailsEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a2f3a));
    pluginDetailsEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);

    loadPluginButton.onClick = [this] { choosePluginToLoad(); };
    unloadPluginButton.onClick = [this] { unloadPlugin(); };
    unloadPluginButton.setEnabled(false);
    renderButton.setEnabled(false);

    for (auto* component : std::array<juce::Component*, 11> {
             &titleLabel,
             &summaryLabel,
             &audioStatusLabel,
             &midiStatusLabel,
             &roadmapLabel,
             &pluginStatusLabel,
             &pluginDetailsEditor,
             &loadPluginButton,
             &unloadPluginButton,
             &renderButton,
             &deviceSelector
         })
    {
        addAndMakeVisible(component);
    }

    deviceManager.addAudioCallback(&processorPlayer);
    deviceManager.addMidiInputDeviceCallback({}, &processorPlayer);
    deviceManager.addChangeListener(this);

    initialiseAudioDevices();
    refreshStatusText();
    refreshPluginState();

    setSize(1100, 780);
}

MainComponent::~MainComponent()
{
    pluginFileChooser.reset();
    processorPlayer.setProcessor(nullptr);
    hostedPlugin.reset();

    saveSettings();

    deviceManager.removeChangeListener(this);
    deviceManager.removeMidiInputDeviceCallback({}, &processorPlayer);
    deviceManager.removeAudioCallback(&processorPlayer);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff181a1f));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(90);
    auto buttonRow = area.removeFromTop(36);
    auto status = area.removeFromTop(110);
    auto pluginArea = area.removeFromTop(210);

    titleLabel.setBounds(header.removeFromTop(34));
    summaryLabel.setBounds(header.removeFromTop(24));
    roadmapLabel.setBounds(header);

    loadPluginButton.setBounds(buttonRow.removeFromLeft(170));
    buttonRow.removeFromLeft(12);
    unloadPluginButton.setBounds(buttonRow.removeFromLeft(170));
    buttonRow.removeFromLeft(12);
    renderButton.setBounds(buttonRow.removeFromLeft(230));

    audioStatusLabel.setBounds(status.removeFromTop(48));
    midiStatusLabel.setBounds(status);

    pluginStatusLabel.setBounds(pluginArea.removeFromTop(24));
    pluginArea.removeFromTop(8);
    pluginDetailsEditor.setBounds(pluginArea);

    area.removeFromTop(12);
    deviceSelector.setBounds(area);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &deviceManager)
        return;

    refreshStatusText();
    refreshPluginState();
    saveSettings();
}

void MainComponent::initialiseAudioDevices()
{
    const auto* settings = appProperties.getUserSettings();
    std::unique_ptr<juce::XmlElement> audioState;

    if (const auto savedXml = settings->getValue(audioDeviceStateKey); savedXml.isNotEmpty())
        audioState = juce::parseXML(savedXml);

    const auto error = deviceManager.initialise(0, 2, audioState.get(), true);

    restoreMidiInputSelection();

    if (error.isNotEmpty())
        audioStatusLabel.setText("Audio device warning: " + error, juce::dontSendNotification);
}

void MainComponent::restoreMidiInputSelection()
{
    auto* settings = appProperties.getUserSettings();
    const auto wasConfigured = settings->getBoolValue(midiInputsConfiguredKey, false);
    const auto savedIds = juce::StringArray::fromTokens(settings->getValue(enabledMidiInputsKey), ";", "");
    const auto availableInputs = juce::MidiInput::getAvailableDevices();

    for (const auto& device : availableInputs)
    {
        const auto enableDevice = wasConfigured ? savedIds.contains(device.identifier) : true;
        deviceManager.setMidiInputDeviceEnabled(device.identifier, enableDevice);
    }
}

void MainComponent::choosePluginToLoad()
{
    if (isPluginLoading)
        return;

    auto initialDirectory = juce::File(appProperties.getUserSettings()->getValue(lastPluginDirectoryKey));

    if (!initialDirectory.exists())
        initialDirectory = getDefaultPluginDirectory();

    pluginFileChooser = std::make_unique<juce::FileChooser>("Select a VST3 instrument",
                                                            initialDirectory,
                                                            "*.vst3");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectDirectories;
    const auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    pluginFileChooser->launchAsync(chooserFlags, [safeThis](const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        const auto selectedFile = chooser.getResult();
        safeThis->pluginFileChooser.reset();

        if (selectedFile == juce::File())
            return;

        safeThis->appProperties.getUserSettings()->setValue(lastPluginDirectoryKey,
                                                            selectedFile.getParentDirectory().getFullPathName());
        safeThis->appProperties.saveIfNeeded();
        safeThis->loadPluginFromFile(selectedFile);
    });
}

void MainComponent::loadPluginFromFile(const juce::File& file)
{
    isPluginLoading = true;
    lastPluginError.clear();
    refreshPluginState();

    auto failLoad = [this](juce::String message)
    {
        isPluginLoading = false;
        lastPluginError = std::move(message);
        refreshPluginState();
    };

    if (!file.exists())
        return failLoad("The selected path does not exist:\n" + file.getFullPathName());

    juce::AudioPluginFormat* vst3Format = nullptr;

    for (auto* format : pluginFormatManager.getFormats())
    {
        if (format != nullptr && format->getName() == juce::VST3PluginFormat::getFormatName())
        {
            vst3Format = format;
            break;
        }
    }

    if (vst3Format == nullptr)
        return failLoad("JUCE did not register a VST3 host format. Check the build configuration.");

    if (!vst3Format->fileMightContainThisPluginType(file.getFullPathName()))
        return failLoad("The selected item is not recognized as a VST3 plug-in:\n" + file.getFullPathName());

    juce::OwnedArray<juce::PluginDescription> discoveredTypes;
    vst3Format->findAllTypesForFile(discoveredTypes, file.getFullPathName());

    if (discoveredTypes.isEmpty())
        return failLoad("No loadable plug-in types were found in:\n" + file.getFullPathName());

    const auto instrumentIterator = std::find_if(discoveredTypes.begin(),
                                                 discoveredTypes.end(),
                                                 [] (const juce::PluginDescription* description)
                                                 {
                                                     return description != nullptr && description->isInstrument;
                                                 });

    if (instrumentIterator == discoveredTypes.end() || *instrumentIterator == nullptr)
        return failLoad("The selected VST3 does not expose an instrument/synth entry.\n\nOnly instrument plug-ins are supported in v1.");

    const auto& description = **instrumentIterator;
    juce::String errorMessage;
    auto instance = pluginFormatManager.createPluginInstance(description,
                                                            getCurrentSampleRate(),
                                                            getCurrentBlockSize(),
                                                            errorMessage);

    if (instance == nullptr)
        return failLoad("JUCE could not create the VST3 instance.\n\n" + errorMessage);

    processorPlayer.setProcessor(nullptr);
    hostedPlugin = std::move(instance);
    hostedPluginDescription = description;
    processorPlayer.setProcessor(hostedPlugin.get());

    isPluginLoading = false;
    lastPluginError.clear();
    refreshPluginState();
}

void MainComponent::unloadPlugin()
{
    pluginFileChooser.reset();
    isPluginLoading = false;
    lastPluginError.clear();
    processorPlayer.setProcessor(nullptr);
    hostedPlugin.reset();
    hostedPluginDescription = {};
    refreshPluginState();
}

void MainComponent::saveSettings()
{
    if (auto state = deviceManager.createStateXml())
        appProperties.getUserSettings()->setValue(audioDeviceStateKey, state->toString());

    appProperties.getUserSettings()->setValue(enabledMidiInputsKey, joinEnabledMidiInputs(deviceManager));
    appProperties.getUserSettings()->setValue(midiInputsConfiguredKey, true);
    appProperties.saveIfNeeded();
}

void MainComponent::refreshStatusText()
{
    juce::String audioSummary = "Audio: no device open";

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        audioSummary = "Audio: " + device->getName()
                     + " | Sample rate: " + juce::String(device->getCurrentSampleRate(), 0)
                     + " Hz | Buffer: " + juce::String(device->getCurrentBufferSizeSamples()) + " samples";
    }

    juce::StringArray enabledInputNames;

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled(device.identifier))
            enabledInputNames.add(device.name);

    const auto midiSummary = enabledInputNames.isEmpty()
        ? juce::String("MIDI: no input devices enabled")
        : "MIDI: " + enabledInputNames.joinIntoString(", ");

    audioStatusLabel.setText(audioSummary, juce::dontSendNotification);
    midiStatusLabel.setText(midiSummary, juce::dontSendNotification);
}

void MainComponent::refreshPluginState()
{
    loadPluginButton.setEnabled(!isPluginLoading);
    unloadPluginButton.setEnabled(hostedPlugin != nullptr && !isPluginLoading);

    if (isPluginLoading)
    {
        pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        pluginStatusLabel.setText("Plugin: loading selected VST3...", juce::dontSendNotification);
        pluginDetailsEditor.setText("Scanning the selected module and creating the instrument instance...", juce::dontSendNotification);
        pluginDetailsEditor.moveCaretToTop(false);
        return;
    }

    if (hostedPlugin != nullptr)
    {
        juce::StringArray details;

        if (lastPluginError.isNotEmpty())
        {
            details.add("Last load attempt failed:");
            details.add(lastPluginError);
            details.add({});
        }

        details.add("Name: " + hostedPluginDescription.name);

        if (hostedPluginDescription.descriptiveName.isNotEmpty()
            && hostedPluginDescription.descriptiveName != hostedPluginDescription.name)
        {
            details.add("Descriptive name: " + hostedPluginDescription.descriptiveName);
        }

        details.add("Format: " + hostedPluginDescription.pluginFormatName);
        details.add("Manufacturer: " + hostedPluginDescription.manufacturerName);
        details.add("Version: " + hostedPluginDescription.version);
        details.add("Category: " + hostedPluginDescription.category);
        details.add("Instrument: " + yesNo(hostedPluginDescription.isInstrument));
        details.add("Accepts MIDI: " + yesNo(hostedPlugin->acceptsMidi()));
        details.add("Produces MIDI: " + yesNo(hostedPlugin->producesMidi()));
        details.add("Has editor: " + yesNo(hostedPlugin->hasEditor()));
        details.add("Inputs: " + juce::String(hostedPluginDescription.numInputChannels)
                  + " | Outputs: " + juce::String(hostedPluginDescription.numOutputChannels));
        details.add("Path: " + hostedPluginDescription.fileOrIdentifier);

        pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        pluginStatusLabel.setText("Plugin: loaded " + hostedPluginDescription.name, juce::dontSendNotification);
        pluginDetailsEditor.setText(details.joinIntoString("\n"), juce::dontSendNotification);
        pluginDetailsEditor.moveCaretToTop(false);
        return;
    }

    if (lastPluginError.isNotEmpty())
    {
        pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::salmon);
        pluginStatusLabel.setText("Plugin: load failed", juce::dontSendNotification);
        pluginDetailsEditor.setText(lastPluginError
                                  + "\n\nChoose a .vst3 instrument file to try again.",
                                  juce::dontSendNotification);
        pluginDetailsEditor.moveCaretToTop(false);
        return;
    }

    pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    pluginStatusLabel.setText("Plugin: no VST3 loaded", juce::dontSendNotification);
    pluginDetailsEditor.setText("Use \"Load VST3...\" to select a single instrument/synth plug-in.\n\n"
                              "This milestone validates the file, scans available plug-in types, "
                              "rejects effect-only modules, and creates one hosted instance.",
                              juce::dontSendNotification);
    pluginDetailsEditor.moveCaretToTop(false);
}

double MainComponent::getCurrentSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentSampleRate();

    return defaultSampleRate;
}

int MainComponent::getCurrentBlockSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBufferSizeSamples();

    return defaultBufferSize;
}
