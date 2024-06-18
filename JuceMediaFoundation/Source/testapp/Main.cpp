//
//  Main.cpp
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#include <JuceHeader.h>
#include "MainWindow.h"
#include "AudioFilePlayer.h"
#include "codec/MediaFoundationAudioFormat.h"

class JuceMediaFoundationApplication : public juce::JUCEApplication
{
private:
	juce::AudioDeviceManager audioDeviceManager;
	juce::AudioFormatManager audioFormatManager;
	juce::AudioThumbnailCache audioThumbnailCache;
	std::unique_ptr<AudioFilePlayer> audioFilePlayer;
	std::unique_ptr<MainWindow> mainWindow;
	juce::TooltipWindow tooltip;
public:
	JuceMediaFoundationApplication() : audioThumbnailCache(16) {}
	virtual const juce::String getApplicationName() override { return ProjectInfo::projectName; }
	virtual const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
	virtual bool moreThanOneInstanceAllowed() override { return false; }
	void initialise(const juce::String&) override
	{
		audioDeviceManager.initialiseWithDefaultDevices(0, 2);
		audioFormatManager.registerBasicFormats();
		audioFormatManager.registerFormat(new MediaFoundationAudioFormat, false);
		audioFilePlayer = AudioFilePlayer::createInstance(audioDeviceManager, audioFormatManager);
		mainWindow = MainWindow::createInstance(audioDeviceManager, audioFormatManager, audioThumbnailCache, *audioFilePlayer);
	}
	void shutdown() override
	{
		mainWindow.reset();
		audioFilePlayer.reset();
	}
	void systemRequestedQuit() override
	{
		quit();
	}
	void anotherInstanceStarted(const juce::String&) override
	{
	}
};

START_JUCE_APPLICATION(JuceMediaFoundationApplication)
