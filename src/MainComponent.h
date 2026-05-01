#pragma once

#include <JuceHeader.h>

class MainComponent final : public juce::Component,
                            private juce::ChangeListener
{
public:
    explicit MainComponent(juce::ApplicationProperties& applicationProperties);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class PluginEditorWindow;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void initialiseAudioDevices();
    void restoreMidiInputSelection();
    void choosePluginToLoad();
    void loadPluginFromFile(const juce::File& file);
    void unloadPlugin();
    void showPluginEditorWindow();
    void closePluginEditorWindow();
    void rebuildParameterRows();
    void clearParameterRows();
    void layoutParameterRows();
    void saveSettings();
    void refreshStatusText();
    void refreshPluginState();
    double getCurrentSampleRate() const;
    int getCurrentBlockSize() const;

    juce::ApplicationProperties& appProperties;
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    juce::AudioPluginFormatManager pluginFormatManager;
    std::unique_ptr<juce::AudioPluginInstance> hostedPlugin;
    juce::PluginDescription hostedPluginDescription;
    std::unique_ptr<PluginEditorWindow> pluginEditorWindow;
    std::unique_ptr<juce::FileChooser> pluginFileChooser;
    juce::OwnedArray<juce::Component> parameterRowComponents;
    int totalParameterCount = 0;
    int filteredParameterCount = 0;
    bool isPluginLoading = false;
    juce::String lastPluginError;

    juce::Label titleLabel;
    juce::Label summaryLabel;
    juce::Label audioStatusLabel;
    juce::Label midiStatusLabel;
    juce::Label roadmapLabel;
    juce::Label pluginStatusLabel;
    juce::TextEditor pluginDetailsEditor;
    juce::TextButton loadPluginButton { "Load VST3..." };
    juce::TextButton showEditorButton { "Show Editor" };
    juce::TextButton unloadPluginButton { "Unload VST3" };
    juce::TextButton renderButton { "Render WAV (next milestone)" };
    juce::GroupComponent parameterGroup;
    juce::TextEditor parameterSearchEditor;
    juce::Label parameterCountLabel;
    juce::Viewport parameterViewport;
    juce::Component parameterListContent;
    juce::Label parameterEmptyStateLabel;
    juce::AudioDeviceSelectorComponent deviceSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
