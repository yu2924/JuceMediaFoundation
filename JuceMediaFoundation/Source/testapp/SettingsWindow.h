//
//  SettingsWindow.h
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#pragma once

#include <JuceHeader.h>

class SettingsWindow : public juce::DocumentWindow
{
protected:
	SettingsWindow(const juce::String& name, juce::Colour clrbg, int btns, bool addtodt = true) : DocumentWindow(name, clrbg, btns, addtodt) {}
public:
	virtual void closeWindow() = 0;
	static juce::Component::SafePointer<SettingsWindow> createInstance(juce::AudioDeviceManager& adm);
};