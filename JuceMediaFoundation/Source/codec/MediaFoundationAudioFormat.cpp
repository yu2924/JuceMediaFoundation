//
//  MediaFoundationAudioFormat.cpp
//  JuceMediaFoundation_App
//
//  created by yu2924 on 2024-06-11
//

#include "MediaFoundationAudioFormat.h"

#if JUCE_WINDOWS

#if !defined(WINVER)
#define WINVER _WIN32_WINNT_WIN7
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")
#include <comdef.h>

// ================================================================================
// MFAudioFormatReader

namespace MediaFoundationCodec
{
	_COM_SMARTPTR_TYPEDEF(IStream, __uuidof(IStream));
	_COM_SMARTPTR_TYPEDEF(IMFByteStream, __uuidof(IMFByteStream));
	_COM_SMARTPTR_TYPEDEF(IMFSourceReader, __uuidof(IMFSourceReader));
	_COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
	_COM_SMARTPTR_TYPEDEF(IMFSample, __uuidof(IMFSample));
	_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));

	class JuceComIStream : public IStream
	{
	protected:
		juce::InputStream* inputStream = nullptr;
		LONG refCount = 0;
		JuceComIStream(juce::InputStream* str) : inputStream(str)
		{
		}
	public:
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
		{
			static const QITAB qit[] =
			{
				QITABENT(JuceComIStream, ISequentialStream),
				QITABENT(JuceComIStream, IStream),
				{ 0 }
			};
			return QISearch(this, qit, riid, ppv);
		}
		virtual ULONG STDMETHODCALLTYPE AddRef() override
		{
			return InterlockedIncrement(&refCount);
		}
		virtual ULONG STDMETHODCALLTYPE Release() override
		{
			LONG rc = InterlockedDecrement(&refCount); if(rc == 0) delete this; return rc;
		}
		virtual HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override
		{
			if(pcbRead) *pcbRead = 0;
			ULONG lr = inputStream->read(pv, (int)cb);
			if(pcbRead) *pcbRead = lr;
			return (lr == cb) ? S_OK : S_FALSE;
		}
		virtual HRESULT STDMETHODCALLTYPE Write(const void*, ULONG, ULONG* pcbWritten) override
		{
			if(pcbWritten) *pcbWritten = 0;
			return E_NOTIMPL;
		}
		virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override
		{
			if(plibNewPosition) plibNewPosition->QuadPart = 0;
			int64_t orgpos = 0;
			switch(dwOrigin)
			{
				case STREAM_SEEK_SET: orgpos = 0; break;
				case STREAM_SEEK_CUR: orgpos = inputStream->getPosition(); break;
				case STREAM_SEEK_END: orgpos = inputStream->getTotalLength(); break;
				default: return E_INVALIDARG;
			}
			if(!inputStream->setPosition(orgpos + dlibMove.QuadPart)) return E_FAIL;
			if(plibNewPosition) plibNewPosition->QuadPart = inputStream->getPosition();
			return S_OK;
		}
		virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override
		{
			return E_NOTIMPL;
		}
		virtual HRESULT STDMETHODCALLTYPE CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override
		{
			if(pcbRead) pcbRead->QuadPart = 0;
			if(pcbWritten) pcbWritten->QuadPart = 0;
			int64_t cbx = cb.QuadPart, cbr = 0, cbw = 0;
			char buffer[1024];
			while(!inputStream->isExhausted())
			{
				ULONG lr = inputStream->read(buffer, (int)std::min((int64_t)sizeof(buffer), cbx));
				ULONG lw = 0; pstm->Write(buffer, lr, &lw);
				cbr += lr;
				cbw += lw;
				cbx -= lr;
			}
			if(pcbRead) pcbRead->QuadPart = cbr;
			if(pcbWritten) pcbWritten->QuadPart = cbx;
			return S_OK;
		}
		virtual HRESULT STDMETHODCALLTYPE Commit(DWORD) override
		{
			return S_OK;
		}
		virtual HRESULT STDMETHODCALLTYPE Revert() override
		{
			return E_NOTIMPL;
		}
		virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override
		{
			return E_NOTIMPL;
		}
		virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override
		{
			return E_NOTIMPL;
		}
		virtual HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg, DWORD) override
		{
			if(!pstatstg) return STG_E_INVALIDPOINTER;
			*pstatstg = {};
			pstatstg->type = STGTY_STREAM;
			pstatstg->cbSize.QuadPart = inputStream->getTotalLength();
			return S_OK;
		}
		virtual HRESULT STDMETHODCALLTYPE Clone(IStream**) override
		{
			return E_NOTIMPL;
		}
		static IStreamPtr createInstance(juce::InputStream* str)
		{
			return IStreamPtr(new JuceComIStream(str), true);
		}
	};

	struct MFAutoInit
	{
		struct Shared
		{
			Shared()
			{
				HRESULT r = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
				if(FAILED(r)) { DBG("CoInitializeEx() ERROR: " << juce::String((const wchar_t*)_com_error(r).Description()).quoted()); }
				MFStartup(MF_VERSION);
			}
			~Shared()
			{
				MFShutdown();
				CoUninitialize();
			}
		};
		juce::SharedResourcePointer<Shared> shared;
	};

	static const char* MFFormatName = "Media Foundation";
	// static const char* const MFExtensions[] = { ".3g2", ".3gp", ".3gp2", ".3gpp", ".asf", ".wma", ".wmv", ".aac", ".adts", ".avi", ".mp3", ".m4a", ".m4v", ".mov", ".mp4", ".sami", ".smi", ".wav", nullptr};
	static const char* const MFExtensions[] = { ".m4a", ".m4v", ".mov", ".mp4", nullptr };

	class MFAudioFormatReader : public juce::AudioFormatReader
	{
	public:
		MFAutoInit mfAutoInit;
		IMFSourceReaderPtr mfSourceReader;
		struct
		{
			IMFSamplePtr sample;
			IMFMediaBufferPtr mediaBuffer;
			juce::Range<int64_t> range;
		} sampleBuffer;
		MFAudioFormatReader(juce::InputStream* str)
			: AudioFormatReader(str, TRANS(MFFormatName))
		{
			HRESULT r = createSourceReader();
			if(SUCCEEDED(r)) r = configureSourceReader();
			if(SUCCEEDED(r)) r = determineSourceAttributes();
			// TODO: collect metadata
			if(FAILED(r)) { DBG("  ERROR: " << juce::String((const wchar_t*)_com_error(r).Description()).quoted()); }
		}
		bool isValid() const
		{
			return (mfSourceReader != nullptr) && (0 < sampleRate) && (0 < numChannels) && (0 < bitsPerSample);
		}
		HRESULT createSourceReader()
		{
			IMFByteStreamPtr bstr;
			HRESULT r = MFCreateMFByteStreamOnStream(JuceComIStream::createInstance(input), &bstr);
			if(SUCCEEDED(r)) r = MFCreateSourceReaderFromByteStream(bstr, NULL, &mfSourceReader);
			return r;
		}
		HRESULT configureSourceReader()
		{
			// NOTE: in this implementation, the output sample type is either float32 or int32
			// deselect all streams, then select the first audio stream
			HRESULT r = mfSourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
			if(FAILED(r)) return r;
			r = mfSourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
			if(FAILED(r)) return r;
			// get the native media type
			IMFMediaTypePtr mtnative;
			r = mfSourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &mtnative);
			if(FAILED(r)) return r;
			GUID subtype = GUID_NULL; mtnative->GetGUID(MF_MT_SUBTYPE, &subtype);
			UINT32 bps = 0; mtnative->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bps);
			bool forceint = (subtype == MFAudioFormat_PCM) && (bps == 32);
			// set the partial media type
			IMFMediaTypePtr mtpartial;
			MFCreateMediaType(&mtpartial);
			mtpartial->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
			mtpartial->SetGUID(MF_MT_SUBTYPE, forceint ? MFAudioFormat_PCM : MFAudioFormat_Float);
			r = mfSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, mtpartial);
			if(FAILED(r)) return r;
			// ensure the stream is selected
			r = mfSourceReader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
			if(FAILED(r)) return r;
			return S_OK;
		}
		HRESULT determineSourceAttributes()
		{
			// get the complete media type
			IMFMediaTypePtr mtcomplete;
			HRESULT r = mfSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &mtcomplete);
			if(FAILED(r)) return r;
			// attributes
			GUID subtype = GUID_NULL; mtcomplete->GetGUID(MF_MT_SUBTYPE, &subtype);
			UINT32 sps = 0; mtcomplete->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sps);
			UINT32 cch = 0; mtcomplete->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &cch);
			UINT32 bps = 0; mtcomplete->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bps);
			usesFloatingPointData = (subtype == MFAudioFormat_Float) ? true : false;
			sampleRate = (double)sps;
			numChannels = cch;
			bitsPerSample = bps;
			if(bitsPerSample != 32) return E_UNEXPECTED;
			// length
			PROPVARIANT vdur = {};
			mfSourceReader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &vdur);
			LONGLONG durhns = 0; PropVariantToInt64(vdur, &durhns);
			lengthInSamples = hnsToSample(durhns);
			// DBG("MFAudioFormatReader sampleRate=" << sampleRate << " numChannels=" << (int)numChannels << " bitsPerSample=" << (int)bitsPerSample << " isfloat=" << (usesFloatingPointData ? "yes" : "no") << " length=" << lengthInSamples);
			return S_OK;
		}
		int64_t sampleToHns(int64_t spos) const
		{
			// return (int64_t)(spos * 10000000ll) / (int64_t)sampleRate;
			return (int64_t)((double)spos * 10000000.0 / sampleRate + 0.5);
		}
		int64_t hnsToSample(int64_t tpos) const
		{
			// return (int64_t)(tpos * (int64_t)sampleRate) / 10000000ll;
			return (int64_t)((double)tpos * sampleRate / 10000000.0 + 0.5);
		}
		bool seekStream(int64_t spos)
		{
			sampleBuffer = {};
			PROPVARIANT vpos = {}; InitPropVariantFromInt64(sampleToHns(spos), &vpos);
			return SUCCEEDED(mfSourceReader->SetCurrentPosition(GUID_NULL, vpos));
		}
		bool loadNextSampleBuffer(int64_t samplepos)
		{
			sampleBuffer = {};
			while(true)
			{
				IMFSamplePtr smpl;
				LONGLONG tstart = 0;
				DWORD flags = 0;
				HRESULT r = mfSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &tstart, &smpl);
				if(FAILED(r)) return false;
				if(flags & MF_SOURCE_READERF_ERROR) return false;
				if(flags & MF_SOURCE_READERF_ENDOFSTREAM) return false;
				if(flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) return false;
				if(!smpl) continue;
				int64_t rngstart = hnsToSample(tstart);
				if(samplepos < rngstart)
				{
					DBG("WARNING: advanced too far");
					return false;
				}
				LONGLONG tduration = 0; smpl->GetSampleDuration(&tduration);
				juce::Range<int64_t> rng{ rngstart, hnsToSample(tstart + tduration) };
				if(rng.contains(samplepos))
				{
					smpl->ConvertToContiguousBuffer(&sampleBuffer.mediaBuffer);
					sampleBuffer.sample = smpl;
					sampleBuffer.range = rng;
					return true;
				}
			}
			return false;
		}
		virtual bool readSamples(int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer, juce::int64 startSampleInFile, int numSamples) override
		{
			if(sampleRate <= 0) return false;
			clearSamplesBeyondAvailableLength(destChannels, numDestChannels, startOffsetInDestBuffer, startSampleInFile, numSamples, lengthInSamples);
			int ismp = 0; while(ismp < numSamples)
			{
				int64_t srcstroff = startSampleInFile + ismp;
				if(!sampleBuffer.range.contains(srcstroff))
				{
					if(sampleBuffer.range.getEnd() != srcstroff)
					{
						seekStream(srcstroff);
					}
					if(!loadNextSampleBuffer(srcstroff)) break;
				}
				int lseg = std::min((numSamples - ismp), (int)(sampleBuffer.range.getEnd() - srcstroff));
				int srcbufoff = (int)(srcstroff - sampleBuffer.range.getStart());
				int dstbufoff = startOffsetInDestBuffer + ismp;
				BYTE* psbuf = nullptr; sampleBuffer.mediaBuffer->Lock(&psbuf, NULL, NULL);
				jassert(bitsPerSample == 32);
				if(usesFloatingPointData)
				{
					const float* psrc = (const float*)psbuf + numChannels * srcbufoff;
					ReadHelper<juce::AudioData::Float32, juce::AudioData::Float32, juce::AudioData::LittleEndian>::read(destChannels, dstbufoff, numDestChannels, psrc, numChannels, lseg);
				}
				else
				{
					const int* psrc = (const int*)psbuf + numChannels * srcbufoff;
					ReadHelper<juce::AudioData::Int32, juce::AudioData::Int32, juce::AudioData::LittleEndian>::read(destChannels, dstbufoff, numDestChannels, psrc, numChannels, lseg);
				}
				sampleBuffer.mediaBuffer->Unlock();
				ismp += lseg;
			}
			if((ismp < numSamples) && (ismp < lengthInSamples))
			{
				for(int ich = 0; ich < numDestChannels; ++ich)
				{
					if(destChannels[ich]) juce::FloatVectorOperations::clear((float*)destChannels[ich] + ismp, numSamples - ismp);
				}
			}
			return true;
		}
	};

} // namespace MediaFoundationCodec

// ================================================================================
// MediaFoundationAudioFormat

MediaFoundationAudioFormat::MediaFoundationAudioFormat() : AudioFormat(TRANS(MediaFoundationCodec::MFFormatName), juce::StringArray(MediaFoundationCodec::MFExtensions))
{
}

juce::Array<int> MediaFoundationAudioFormat::getPossibleSampleRates()
{
	return {};
}

juce::Array<int> MediaFoundationAudioFormat::getPossibleBitDepths()
{
	return {};
}

bool MediaFoundationAudioFormat::canDoStereo()
{
	return true;
}

bool MediaFoundationAudioFormat::canDoMono()
{
	return true;
}

bool MediaFoundationAudioFormat::isCompressed()
{
	return true;
}

juce::AudioFormatReader* MediaFoundationAudioFormat::createReaderFor(juce::InputStream* sourceStream, bool deleteStreamIfOpeningFails)
{
	std::unique_ptr<MediaFoundationCodec::MFAudioFormatReader> afr(new MediaFoundationCodec::MFAudioFormatReader(sourceStream));
	if(afr->isValid()) return afr.release();
	if(!deleteStreamIfOpeningFails) afr->input = nullptr;
	return nullptr;
}

juce::AudioFormatWriter* MediaFoundationAudioFormat::createWriterFor(juce::OutputStream*, double, unsigned int, int, const juce::StringPairArray&, int)
{
	// not yet implemented
	jassertfalse;
	return nullptr;
}

#endif // JUCE_WINDOWS
