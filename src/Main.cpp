#include <JuceHeader.h>

#include "MainComponent.h"

class VstHostApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "VST Host"; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "vst-host";
        options.filenameSuffix = "settings";
        options.folderName = "gbruce\\vst-host";
        options.osxLibrarySubFolder = "Application Support";

        appProperties.setStorageParameters(options);
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), appProperties);
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, juce::ApplicationProperties& properties)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setResizeLimits(900, 620, 1600, 1200);
            setContentOwned(new MainComponent(properties), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    juce::ApplicationProperties appProperties;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(VstHostApplication)
