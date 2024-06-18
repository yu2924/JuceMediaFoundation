//
//  AudioFilePlayer.cpp
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#include "AudioFilePlayer.h"

// ================================================================================
// AudioFileSource

class AudioFileSource : public juce::AudioSource
{
public:
	juce::AudioFormatManager& audioFormatManager;
	juce::File filePath;
	std::unique_ptr<juce::AudioFormatReader> reader;
	juce::AudioSampleBuffer readBuffer;
	int numChannels = 0;
	int bufferSize = 0;
	int64_t nextReadPosition = 0;
	int64_t loopBegin = 0;
	int64_t loopEnd = 0;
	float gain = 1;
	bool loopEnabled = false;
	bool playbackEnabled = false;
	AudioFileSource(juce::AudioFormatManager& afm) : audioFormatManager(afm)
	{
	}
	void updateBuffer()
	{
		readBuffer.setSize(numChannels, bufferSize);
	}
	// --------------------------------------------------------------------------------
	// juce::AudioSource
	virtual void prepareToPlay(int len, double) override
	{
		bufferSize = len;
		updateBuffer();
	}
	virtual void releaseResources() override
	{
		bufferSize = 0;
		readBuffer.setSize(0, 0);
	}
	virtual void getNextAudioBlock(const juce::AudioSourceChannelInfo& asci) override
	{
		asci.clearActiveBufferRegion();
		if(!reader || !playbackEnabled) return;
		if(readBuffer.getNumSamples() < asci.numSamples) readBuffer.setSize(readBuffer.getNumChannels(), asci.numSamples);
		int dstlen = asci.numSamples, dstpos = 0; while(dstpos < dstlen)
		{
			if(loopEnabled && (loopEnd <= nextReadPosition)) nextReadPosition = loopBegin;
			int64_t srcend = loopEnabled ? reader->lengthInSamples : loopEnd;
			int lseg = (int)std::min((int64_t)(dstlen - dstpos), srcend - nextReadPosition);
			if(lseg <= 0) break;
			reader->read(&readBuffer, 0, lseg, nextReadPosition, true, true);
			int cchr = readBuffer.getNumChannels(), ichr = 0;
			int ccho = asci.buffer->getNumChannels(), icho = 0;
			for(int cch = std::max(cchr, ccho), ich = 0; ich < cch; ++ich)
			{
				asci.buffer->addFromWithRamp(icho, asci.startSample + dstpos, readBuffer.getReadPointer(ichr), lseg, gain, gain);
				++ichr; if(cchr <= ichr) ichr = 0;
				++icho; if(ccho <= icho) icho = 0;
			}
			dstpos += lseg;
			nextReadPosition += lseg;
		}
	}
	// --------------------------------------------------------------------------------
	// APIs
	juce::File getFile() const
	{
		return filePath;
	}
	bool setFile(const juce::File& v)
	{
		filePath = juce::File();
		reader = nullptr;
		numChannels = 0;
		nextReadPosition = 0;
		loopBegin = 0;
		loopEnd = 0;
		playbackEnabled = false;
		if(v == juce::File()) return true;
		reader.reset(audioFormatManager.createReaderFor(v));
		if(!reader || (reader->sampleRate <= 0) || (reader->numChannels == 0)) { reader = nullptr; return false; }
		filePath = v;
		numChannels = reader->numChannels;
		nextReadPosition = 0;
		loopBegin = 0;
		loopEnd = reader->lengthInSamples;
		updateBuffer();
		return true;
	}
	juce::String getFormatName() const
	{
		return reader ? reader->getFormatName() : juce::String();
	}
	double getSampleRate() const
	{
		return reader ? reader->sampleRate : 0;
	}
	int getNumChannels() const
	{
		return reader ? reader->numChannels : 0;
	}
	int64_t getTotalLength() const
	{
		return reader ? reader->lengthInSamples : 0;
	}
	int64_t getNextReadPosition() const
	{
		return nextReadPosition;
	}
	void setNextReadPosition(int64_t v)
	{
		int64_t len = reader ? reader->lengthInSamples : 0;
		nextReadPosition = std::max(0ll, std::min(len, v));
	}
	int64_t getLoopBegin() const
	{
		return loopBegin;
	}
	void setLoopBegin(int64_t v)
	{
		int64_t len = reader ? reader->lengthInSamples : 0;
		loopBegin = std::max(0ll, std::min(len, v));
		loopEnd = std::max(loopBegin, loopEnd);
	}
	int64_t getLoopEnd() const
	{
		return loopEnd;
	}
	void setLoopEnd(int64_t v)
	{
		int64_t len = reader ? reader->lengthInSamples : 0;
		loopEnd = std::max(0ll, std::min(len, v));
		loopBegin = std::min(loopEnd, loopBegin);
	}
	float getGain() const
	{
		return gain;
	}
	void setGain(float v)
	{
		gain = std::max(0.0f, v);
	}
	bool isLoopEnabled() const
	{
		return loopEnabled;
	}
	void setLoopEnabled(bool v)
	{
		loopEnabled = v;
	}
	bool isPlaybackEnabled() const
	{
		return playbackEnabled;
	}
	void setPlaybackEnabled(bool v)
	{
		playbackEnabled = reader ? v : false;
	}
};

// ================================================================================
// AudioFormatPlayer
//
// the audio pipeline:
//   [audioFileSource]--[juce::ResamplingAudioSource]--[juce::AudioPluginInstance]--[juce::AudioIODeviceCallback]
//

class AudioFilePlayerImpl
	: public AudioFilePlayer
	, public juce::AudioIODeviceCallback
	, public juce::AsyncUpdater
{
public:
	juce::AudioDeviceManager& audioDeviceManager;
	AudioFileSource audioFileSource;
	juce::ResamplingAudioSource resamplingAudioSource;
	bool pluginHasBypassParam = false;
	bool autoplayEnabled = true;
	bool pluginBypassed = false;
	bool prepared = false;
	AudioFilePlayerImpl(juce::AudioDeviceManager& adm, juce::AudioFormatManager& afm)
		: audioDeviceManager(adm)
		, audioFileSource(afm)
		, resamplingAudioSource(&audioFileSource, false, 2)
	{
		setLoopEnabled(true);
		setGainDb(0);
		audioDeviceManager.addAudioCallback(this);
	}
	virtual ~AudioFilePlayerImpl()
	{
		audioDeviceManager.removeAudioCallback(this);
	}
	// --------------------------------------------------------------------------------
	// juce::AudioIODeviceCallback
	virtual void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* ppo, int ccho, int len, const juce::AudioIODeviceCallbackContext&) override
	{
		for(int ich = 2; ich < ccho; ++ich) juce::FloatVectorOperations::clear(ppo[ich], len);
		juce::AudioSampleBuffer buffer(ppo, std::min(2, ccho), len);
		resamplingAudioSource.getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));
		if(!audioFileSource.isLoopEnabled() && (audioFileSource.getTotalLength() <= audioFileSource.getNextReadPosition())) triggerAsyncUpdate();
	}
	virtual void audioDeviceAboutToStart(juce::AudioIODevice* dev) override
	{
		prepared = true;
		double fsdev = dev->getCurrentSampleRate();
		double fssrc = audioFileSource.getSampleRate();
		if(0 < fssrc) resamplingAudioSource.setResamplingRatio(fssrc / fsdev);
		int lbuf = dev->getCurrentBufferSizeSamples();
		resamplingAudioSource.prepareToPlay(lbuf, fsdev);
	}
	virtual void audioDeviceStopped() override
	{
		prepared = false;
		resamplingAudioSource.releaseResources();
	}
	// --------------------------------------------------------------------------------
	// juce::AsyncUpdater
	virtual void handleAsyncUpdate() override
	{
		audioFileSource.setPlaybackEnabled(false);
		sendChangeMessage();
	}
	// --------------------------------------------------------------------------------
	// AudioFilePlayer
	virtual double getGainDb() const override
	{
		return 20 * std::log10((double)audioFileSource.getGain());
	}
	virtual void setGainDb(double db) override
	{
		juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
		double gain = std::pow(10.0, std::max(-120.0, std::min(0.0, db)) * 0.05);
		audioFileSource.setGain((1e-6 < gain) ? (float)gain : 0.0f);
		sendChangeMessage();
	}
	virtual bool isLoopEnabled() const override
	{
		return audioFileSource.isLoopEnabled();
	}
	virtual void setLoopEnabled(bool v) override
	{
		juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
		audioFileSource.setLoopEnabled(v);
		sendChangeMessage();
	}
	virtual juce::File getAudioFile() const override
	{
		return audioFileSource.getFile();
	}
	virtual void setAudioFile(const juce::File& v) override
	{
		juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
		audioFileSource.setFile(v);
		if(audioFileSource.getFile() != juce::File())
		{
			juce::AudioIODevice* dev = audioDeviceManager.getCurrentAudioDevice();
			double fsdev = dev ? dev->getCurrentSampleRate() : 44100;
			double fssrc = audioFileSource.getSampleRate();
			resamplingAudioSource.setResamplingRatio(fssrc / fsdev);
		}
		sendChangeMessage();
	}
	virtual juce::String getFormatName() const override
	{
		return audioFileSource.getFormatName();
	}
	virtual double getDuration() const override
	{
		double fssrc = audioFileSource.getSampleRate();
		return (0 < fssrc) ? ((double)audioFileSource.getTotalLength() / fssrc) : 0;
	}
	virtual double getPlaybackPosition() const override
	{
		double fssrc = audioFileSource.getSampleRate();
		return (0 < fssrc) ? ((double)audioFileSource.getNextReadPosition() / fssrc) : 0;
	}
	virtual void setPlaybackPosition(double v) override
	{
		juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
		double fssrc = audioFileSource.getSampleRate();
		audioFileSource.setNextReadPosition((int64_t)(fssrc * v));
		sendChangeMessage();
	}
	virtual bool isRunning() const override
	{
		return audioFileSource.isPlaybackEnabled();
	}
	virtual void setRunning(bool v) override
	{
		if(v)
		{
			juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
			if(!audioFileSource.isLoopEnabled() && (audioFileSource.getTotalLength() <= audioFileSource.getNextReadPosition())) audioFileSource.setNextReadPosition(0);
			audioFileSource.setPlaybackEnabled(true);
		}
		else
		{
			juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
			audioFileSource.setPlaybackEnabled(false);
		}
		sendChangeMessage();
	}
};

std::unique_ptr<AudioFilePlayer> AudioFilePlayer::createInstance(juce::AudioDeviceManager& adm, juce::AudioFormatManager& afm)
{
	return std::make_unique<AudioFilePlayerImpl>(adm, afm);
}
