//
//  SettingsWindow.cpp
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#include "SettingsWindow.h"

static juce::String SettingsWindowLastWindowStatus;

class SettingsWindowImpl
	: public SettingsWindow
{
public:
	SettingsWindowImpl(juce::AudioDeviceManager& adm)
		: SettingsWindow("Settings", juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId), juce::DocumentWindow::TitleBarButtons::allButtons, true)
	{
		setUsingNativeTitleBar(true);
		setResizable(true, true);
		juce::AudioDeviceSelectorComponent* pane = new juce::AudioDeviceSelectorComponent(adm, 0, 0, 2, 2, false, false, true, false);
		pane->setSize(640, 480);
		setContentOwned(pane, true);
		if(SettingsWindowLastWindowStatus.isNotEmpty()) restoreWindowStateFromString(SettingsWindowLastWindowStatus);
		else centreWithSize(getWidth(), getHeight());
		setVisible(true);
	}
	virtual ~SettingsWindowImpl()
	{
		SettingsWindowLastWindowStatus = getWindowStateAsString();
	}
	virtual void closeButtonPressed() override
	{
		closeWindow();
	}
	virtual void closeWindow() override
	{
		delete this;
	}
};

juce::Component::SafePointer<SettingsWindow> SettingsWindow::createInstance(juce::AudioDeviceManager& adm)
{
	return new SettingsWindowImpl(adm);
}