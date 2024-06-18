//
//  MainWindow.h
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#pragma once

#include <JuceHeader.h>
#include "AudioFilePlayer.h"

class MainWindow : public juce::DocumentWindow
{
protected:
	MainWindow(const juce::String& name, juce::Colour clrbg, int btns, bool addtodt = true) : DocumentWindow(name, clrbg, btns, addtodt) {}
public:
	static std::unique_ptr<MainWindow> createInstance(juce::AudioDeviceManager& adm, juce::AudioFormatManager& afm, juce::AudioThumbnailCache& atc, AudioFilePlayer& afp);
};
