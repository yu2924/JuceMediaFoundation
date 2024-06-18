//
//  AudioFilePlayer.h
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#pragma once

#include <JuceHeader.h>

class AudioFilePlayer : public juce::ChangeBroadcaster
{
protected:
	AudioFilePlayer() {}
public:
	virtual ~AudioFilePlayer() {}
	virtual double getGainDb() const = 0;
	virtual void setGainDb(double v) = 0;
	virtual bool isLoopEnabled() const = 0;
	virtual void setLoopEnabled(bool v) = 0;
	virtual juce::File getAudioFile() const = 0;
	virtual void setAudioFile(const juce::File& v) = 0;
	virtual juce::String getFormatName() const = 0;
	virtual double getDuration() const = 0;
	virtual double getPlaybackPosition() const = 0;
	virtual void setPlaybackPosition(double v) = 0;
	virtual bool isRunning() const = 0;
	virtual void setRunning(bool v) = 0;
	static std::unique_ptr<AudioFilePlayer> createInstance(juce::AudioDeviceManager& adm, juce::AudioFormatManager& afm);
};
