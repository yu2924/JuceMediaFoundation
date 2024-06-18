//
//  MediaFoundationAudioFormat.h
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#pragma once

#include <JuceHeader.h>

#if JUCE_WINDOWS

class MediaFoundationAudioFormat : public juce::AudioFormat
{
public:
	MediaFoundationAudioFormat();
	virtual juce::Array<int> getPossibleSampleRates() override;
	virtual juce::Array<int> getPossibleBitDepths() override;
	virtual bool canDoStereo() override;
	virtual bool canDoMono() override;
	virtual bool isCompressed() override;
	virtual juce::AudioFormatReader* createReaderFor(juce::InputStream*, bool deleteStreamIfOpeningFails) override;
	virtual juce::AudioFormatWriter* createWriterFor(juce::OutputStream*, double sampleRateToUse,
	unsigned int numberOfChannels, int bitsPerSample,
	const juce::StringPairArray& metadataValues, int qualityOptionIndex) override;
};

#endif // JUCE_WINDOWS
