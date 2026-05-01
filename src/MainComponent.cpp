#include <algorithm>
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
constexpr int parameterRowHeight = 64;
constexpr int parameterRowGap = 6;
constexpr int outerMargin = 20;
constexpr int sectionGap = 12;
constexpr int headerHeight = 90;
constexpr int buttonRowHeight = 36;
constexpr int statusHeight = 110;
constexpr int pluginInfoHeight = 140;
constexpr int deviceAreaHeight = 250;

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

juce::String categoryToString(juce::AudioProcessorParameter::Category category)
{
    switch (category)
    {
        case juce::AudioProcessorParameter::genericParameter: return "Generic";
        case juce::AudioProcessorParameter::inputGain: return "Input gain";
        case juce::AudioProcessorParameter::outputGain: return "Output gain";
        case juce::AudioProcessorParameter::inputMeter: return "Input meter";
        case juce::AudioProcessorParameter::outputMeter: return "Output meter";
        case juce::AudioProcessorParameter::compressorLimiterGainReductionMeter: return "Gain reduction meter";
        case juce::AudioProcessorParameter::expanderGateGainReductionMeter: return "Gate meter";
        case juce::AudioProcessorParameter::analysisMeter: return "Analysis meter";
        case juce::AudioProcessorParameter::otherMeter: return "Meter";
    }

    return "Other";
}

class ParameterRowComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    ParameterRowComponent(juce::AudioProcessorParameter& pluginParameter,
                          juce::String parameterIdentifier,
                          int parameterIndex)
        : parameter(pluginParameter),
          parameterId(std::move(parameterIdentifier)),
          index(parameterIndex)
    {
        const auto parameterName = parameter.getName(128).trim();
        const auto displayName = parameterName.isNotEmpty() ? parameterName
                                                            : "Parameter " + juce::String(index + 1);
        const auto label = parameter.getLabel().trim();
        const auto metadata = "ID: " + (parameterId.isNotEmpty() ? parameterId
                                                                  : "index-" + juce::String(index))
                            + " | Category: " + categoryToString(parameter.getCategory())
                            + (label.isNotEmpty() ? " | Unit: " + label : juce::String());

        searchableText = (displayName + " " + metadata + " " + label).toLowerCase();

        nameLabel.setText(displayName, juce::dontSendNotification);
        nameLabel.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        metadataLabel.setText(metadata, juce::dontSendNotification);
        metadataLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        metadataLabel.setFont(juce::Font(juce::FontOptions(13.0f)));

        valueLabel.setJustificationType(juce::Justification::centredRight);
        valueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        valueLabel.setFont(juce::Font(juce::FontOptions(13.0f)));

        valueSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        valueSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        valueSlider.setRange(0.0, 1.0, parameter.isDiscrete() ? 1.0 / juce::jmax(1, parameter.getNumSteps() - 1) : 0.0);
        valueSlider.setDoubleClickReturnValue(true, parameter.getDefaultValue());
        valueSlider.setEnabled(parameter.isAutomatable());

        valueSlider.onDragStart = [this]
        {
            if (valueSlider.isEnabled())
                parameter.beginChangeGesture();
        };

        valueSlider.onDragEnd = [this]
        {
            if (valueSlider.isEnabled())
                parameter.endChangeGesture();
        };

        valueSlider.onValueChange = [this]
        {
            if (ignoreSliderCallbacks || !valueSlider.isEnabled())
                return;

            parameter.setValueNotifyingHost((float) valueSlider.getValue());
            valueLabel.setText(parameter.getCurrentValueAsText(), juce::dontSendNotification);
        };

        for (auto* child : std::array<juce::Component*, 4> { &nameLabel, &metadataLabel, &valueLabel, &valueSlider })
            addAndMakeVisible(child);

        syncFromParameter();
        startTimerHz(15);
    }

    bool matchesSearch(const juce::String& lowerCaseQuery) const
    {
        return lowerCaseQuery.isEmpty() || searchableText.contains(lowerCaseQuery);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10, 8);
        auto topRow = bounds.removeFromTop(22);

        nameLabel.setBounds(topRow.removeFromLeft(juce::jmax(160, topRow.getWidth() - 140)));
        valueLabel.setBounds(topRow);

        metadataLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(4);
        valueSlider.setBounds(bounds);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff20242d));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff2e3440));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
    }

private:
    void timerCallback() override
    {
        syncFromParameter();
    }

    void syncFromParameter()
    {
        const auto currentValue = parameter.getValue();

        if (! valueSlider.isMouseButtonDown() && std::abs(valueSlider.getValue() - currentValue) > 0.0001)
        {
            const juce::ScopedValueSetter<bool> suppressSliderCallbacks(ignoreSliderCallbacks, true);
            valueSlider.setValue(currentValue, juce::dontSendNotification);
        }

        const auto currentText = parameter.getCurrentValueAsText();

        if (valueLabel.getText() != currentText)
            valueLabel.setText(currentText, juce::dontSendNotification);
    }

    juce::AudioProcessorParameter& parameter;
    juce::String parameterId;
    juce::String searchableText;
    int index = 0;
    bool ignoreSliderCallbacks = false;
    juce::Label nameLabel;
    juce::Label metadataLabel;
    juce::Label valueLabel;
    juce::Slider valueSlider;
};
}

class MainComponent::PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow(MainComponent& ownerIn, juce::AudioPluginInstance& processorIn, const juce::String& title)
        : juce::DocumentWindow(title,
                               juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton),
          owner(ownerIn),
          processor(processorIn)
    {
        setUsingNativeTitleBar(true);

        if (auto* editor = processor.createEditorIfNeeded())
        {
            setContentOwned(editor, true);
            setResizable(editor->isResizable(), false);
            setConstrainer(&constrainer);

            if (! editor->isResizable())
                setResizeLimits(editor->getWidth(), editor->getHeight(), editor->getWidth(), editor->getHeight());
        }

        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        toFront(true);
    }

    ~PluginEditorWindow() override
    {
        if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(getContentComponent()))
            processor.editorBeingDeleted(editor);

        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        owner.refreshPluginState();
    }

private:
    class DecoratorConstrainer final : public juce::BorderedComponentBoundsConstrainer
    {
    public:
        explicit DecoratorConstrainer(juce::DocumentWindow& windowIn)
            : window(windowIn)
        {
        }

        juce::ComponentBoundsConstrainer* getWrappedConstrainer() const override
        {
            if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(window.getContentComponent()))
                return editor->getConstrainer();

            return nullptr;
        }

        juce::BorderSize<int> getAdditionalBorder() const override
        {
            const auto nativeFrame = [&]() -> juce::BorderSize<int>
            {
                if (auto* peer = window.getPeer())
                    if (const auto frameSize = peer->getFrameSizeIfPresent())
                        return *frameSize;

                return {};
            }();

            return nativeFrame.addedTo(window.getContentComponentBorder());
        }

    private:
        juce::DocumentWindow& window;
    };

    MainComponent& owner;
    juce::AudioPluginInstance& processor;
    DecoratorConstrainer constrainer { *this };
};

MainComponent::MainComponent(juce::ApplicationProperties& applicationProperties)
    : appProperties(applicationProperties),
      parameterGroup({}, "Parameters"),
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
    roadmapLabel.setText("Milestone 3: plugin editors open in a floating window and parameters stay searchable in the main host. Next: note input and playback.", juce::dontSendNotification);

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

    parameterGroup.setTextLabelPosition(juce::Justification::centredLeft);

    parameterSearchEditor.setTextToShowWhenEmpty("Search by name, ID, category, or unit", juce::Colours::grey);
    parameterSearchEditor.onTextChange = [this] { rebuildParameterRows(); };
    parameterSearchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff101217));
    parameterSearchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a2f3a));
    parameterSearchEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);

    parameterCountLabel.setJustificationType(juce::Justification::centredRight);
    parameterCountLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    parameterViewport.setViewedComponent(&parameterListContent, false);
    parameterViewport.setScrollBarsShown(true, false);

    parameterEmptyStateLabel.setJustificationType(juce::Justification::centred);
    parameterEmptyStateLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    loadPluginButton.onClick = [this] { choosePluginToLoad(); };
    showEditorButton.onClick = [this] { showPluginEditorWindow(); };
    unloadPluginButton.onClick = [this] { unloadPlugin(); };
    showEditorButton.setEnabled(false);
    unloadPluginButton.setEnabled(false);
    renderButton.setEnabled(false);

    for (auto* component : std::array<juce::Component*, 13> {
             &titleLabel,
             &summaryLabel,
             &audioStatusLabel,
             &midiStatusLabel,
             &roadmapLabel,
             &pluginStatusLabel,
             &pluginDetailsEditor,
             &loadPluginButton,
             &showEditorButton,
             &unloadPluginButton,
             &renderButton,
             &parameterGroup,
             &parameterSearchEditor,
         })
    {
        addAndMakeVisible(component);
    }

    addAndMakeVisible(parameterCountLabel);
    addAndMakeVisible(parameterViewport);
    addAndMakeVisible(parameterEmptyStateLabel);
    addAndMakeVisible(deviceSelector);

    deviceManager.addAudioCallback(&processorPlayer);
    deviceManager.addMidiInputDeviceCallback({}, &processorPlayer);
    deviceManager.addChangeListener(this);

    initialiseAudioDevices();
    refreshStatusText();
    refreshPluginState();

    setSize(1180, 900);
}

MainComponent::~MainComponent()
{
    pluginFileChooser.reset();
    closePluginEditorWindow();
    clearParameterRows();
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
    auto area = getLocalBounds().reduced(outerMargin);
    auto header = area.removeFromTop(headerHeight);
    auto buttonRow = area.removeFromTop(buttonRowHeight);
    auto status = area.removeFromTop(statusHeight);
    auto pluginArea = area.removeFromTop(pluginInfoHeight);
    auto deviceArea = area.removeFromBottom(deviceAreaHeight);

    titleLabel.setBounds(header.removeFromTop(34));
    summaryLabel.setBounds(header.removeFromTop(24));
    roadmapLabel.setBounds(header);

    loadPluginButton.setBounds(buttonRow.removeFromLeft(150));
    buttonRow.removeFromLeft(10);
    showEditorButton.setBounds(buttonRow.removeFromLeft(140));
    buttonRow.removeFromLeft(10);
    unloadPluginButton.setBounds(buttonRow.removeFromLeft(150));
    buttonRow.removeFromLeft(10);
    renderButton.setBounds(buttonRow.removeFromLeft(230));

    audioStatusLabel.setBounds(status.removeFromTop(48));
    midiStatusLabel.setBounds(status);

    pluginStatusLabel.setBounds(pluginArea.removeFromTop(24));
    pluginArea.removeFromTop(8);
    pluginDetailsEditor.setBounds(pluginArea);

    area.removeFromBottom(sectionGap);
    parameterGroup.setBounds(area);

    auto parameterInner = area.reduced(12);
    parameterInner.removeFromTop(28);
    auto searchRow = parameterInner.removeFromTop(28);
    parameterSearchEditor.setBounds(searchRow.removeFromLeft(searchRow.getWidth() - 170));
    parameterCountLabel.setBounds(searchRow);
    parameterInner.removeFromTop(8);
    parameterViewport.setBounds(parameterInner);
    parameterEmptyStateLabel.setBounds(parameterInner.reduced(16));
    layoutParameterRows();

    deviceSelector.setBounds(deviceArea);
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

    if (! initialDirectory.exists())
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

    if (! file.exists())
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

    if (! vst3Format->fileMightContainThisPluginType(file.getFullPathName()))
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
    closePluginEditorWindow();
    clearParameterRows();
    hostedPlugin.reset();

    hostedPlugin = std::move(instance);
    hostedPluginDescription = description;
    rebuildParameterRows();
    processorPlayer.setProcessor(hostedPlugin.get());

    if (hostedPlugin->hasEditor())
        showPluginEditorWindow();

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
    closePluginEditorWindow();
    clearParameterRows();
    hostedPlugin.reset();
    hostedPluginDescription = {};
    refreshPluginState();
}

void MainComponent::showPluginEditorWindow()
{
    if (hostedPlugin == nullptr || ! hostedPlugin->hasEditor())
        return;

    if (pluginEditorWindow == nullptr)
    {
        pluginEditorWindow = std::make_unique<PluginEditorWindow>(*this,
                                                                  *hostedPlugin,
                                                                  hostedPluginDescription.name + " Editor");
    }
    else
    {
        pluginEditorWindow->setVisible(true);
        pluginEditorWindow->toFront(true);
    }

    refreshPluginState();
}

void MainComponent::closePluginEditorWindow()
{
    pluginEditorWindow.reset();
}

void MainComponent::rebuildParameterRows()
{
    clearParameterRows();

    if (hostedPlugin == nullptr)
    {
        refreshPluginState();
        return;
    }

    const auto filterText = parameterSearchEditor.getText().trim().toLowerCase();
    totalParameterCount = (int) hostedPlugin->getParameters().size();
    filteredParameterCount = 0;

    for (int index = 0; index < totalParameterCount; ++index)
    {
        auto* parameter = hostedPlugin->getParameters()[index];

        if (parameter == nullptr)
            continue;

        juce::String parameterId;

        if (auto* hostedParameter = dynamic_cast<juce::HostedAudioProcessorParameter*>(parameter))
            parameterId = hostedParameter->getParameterID();

        auto* row = new ParameterRowComponent(*parameter, parameterId, index);

        if (! row->matchesSearch(filterText))
        {
            delete row;
            continue;
        }

        parameterRowComponents.add(row);
        parameterListContent.addAndMakeVisible(row);
        ++filteredParameterCount;
    }

    layoutParameterRows();
    refreshPluginState();
}

void MainComponent::clearParameterRows()
{
    for (auto* row : parameterRowComponents)
        parameterListContent.removeChildComponent(row);

    parameterRowComponents.clear(true);
    totalParameterCount = 0;
    filteredParameterCount = 0;
    parameterListContent.setSize(1, 1);
}

void MainComponent::layoutParameterRows()
{
    const auto availableWidth = juce::jmax(220, parameterViewport.getMaximumVisibleWidth() - 6);
    auto y = 0;

    for (auto* row : parameterRowComponents)
    {
        row->setBounds(0, y, availableWidth, parameterRowHeight);
        y += parameterRowHeight + parameterRowGap;
    }

    parameterListContent.setSize(availableWidth, juce::jmax(1, y));
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
    const auto pluginHasEditor = hostedPlugin != nullptr && hostedPlugin->hasEditor();
    const auto editorWindowVisible = pluginEditorWindow != nullptr && pluginEditorWindow->isVisible();

    loadPluginButton.setEnabled(! isPluginLoading);
    showEditorButton.setEnabled(pluginHasEditor && ! isPluginLoading);
    unloadPluginButton.setEnabled(hostedPlugin != nullptr && ! isPluginLoading);

    if (isPluginLoading)
    {
        pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        pluginStatusLabel.setText("Plugin: loading selected VST3...", juce::dontSendNotification);
        pluginDetailsEditor.setText("Scanning the selected module, creating the instrument instance, and preparing the floating editor window and parameter panel...", juce::dontSendNotification);
        pluginDetailsEditor.moveCaretToTop(false);

        parameterViewport.setVisible(false);
        parameterEmptyStateLabel.setVisible(true);
        parameterEmptyStateLabel.setText("Loading parameters...", juce::dontSendNotification);
        parameterCountLabel.setText({}, juce::dontSendNotification);
        showEditorButton.setButtonText("Show Editor");
        return;
    }

    showEditorButton.setButtonText(editorWindowVisible ? "Focus Editor" : "Show Editor");

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
        details.add("Has native editor: " + yesNo(pluginHasEditor));
        details.add("Editor window: " + juce::String(pluginHasEditor ? (editorWindowVisible ? "Visible" : "Hidden")
                                                                     : "Unavailable"));
        details.add("Parameters: " + juce::String(filteredParameterCount) + " shown / " + juce::String(totalParameterCount) + " total");
        details.add("Inputs: " + juce::String(hostedPluginDescription.numInputChannels)
                  + " | Outputs: " + juce::String(hostedPluginDescription.numOutputChannels));
        details.add("Path: " + hostedPluginDescription.fileOrIdentifier);

        pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        pluginStatusLabel.setText("Plugin: loaded " + hostedPluginDescription.name, juce::dontSendNotification);
        pluginDetailsEditor.setText(details.joinIntoString("\n"), juce::dontSendNotification);
        pluginDetailsEditor.moveCaretToTop(false);

        parameterCountLabel.setText(juce::String(filteredParameterCount) + " shown / " + juce::String(totalParameterCount) + " total",
                                    juce::dontSendNotification);

        if (filteredParameterCount > 0)
        {
            parameterViewport.setVisible(true);
            parameterEmptyStateLabel.setVisible(false);
        }
        else
        {
            parameterViewport.setVisible(false);
            parameterEmptyStateLabel.setVisible(true);

            if (totalParameterCount == 0)
            {
                parameterEmptyStateLabel.setText("This plugin did not expose any host parameters.", juce::dontSendNotification);
            }
            else
            {
                parameterEmptyStateLabel.setText("No parameters matched the current search filter.", juce::dontSendNotification);
            }
        }

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
    }
    else
    {
        pluginStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        pluginStatusLabel.setText("Plugin: no VST3 loaded", juce::dontSendNotification);
        pluginDetailsEditor.setText("Use \"Load VST3...\" to select a single instrument/synth plug-in.\n\n"
                                  "The plugin's native UI opens in a separate floating window so the main host stays dedicated to parameters, MIDI, and render controls.",
                                  juce::dontSendNotification);
        pluginDetailsEditor.moveCaretToTop(false);
    }

    parameterCountLabel.setText({}, juce::dontSendNotification);
    parameterViewport.setVisible(false);
    parameterEmptyStateLabel.setVisible(true);
    parameterEmptyStateLabel.setText("Load a plug-in to inspect and search its parameters.", juce::dontSendNotification);
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
