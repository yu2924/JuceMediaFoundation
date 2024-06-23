//
//  TestMFSourceReader.cpp
//  TestMFSourceReader
//
//  created by yu2924 on 2024-06-15
//

#define WINVER _WIN32_WINNT_WIN7
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>
#include <mferror.h>
#include <propvarutil.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "shlwapi.lib")
#include <comdef.h>
#include <iostream>

_COM_SMARTPTR_TYPEDEF(IStream, __uuidof(IStream));
_COM_SMARTPTR_TYPEDEF(IMFByteStream, __uuidof(IMFByteStream));
_COM_SMARTPTR_TYPEDEF(IMFSourceReader, __uuidof(IMFSourceReader));
_COM_SMARTPTR_TYPEDEF(IMFMediaType, __uuidof(IMFMediaType));
_COM_SMARTPTR_TYPEDEF(IMFSample, __uuidof(IMFSample));
_COM_SMARTPTR_TYPEDEF(IMFMediaBuffer, __uuidof(IMFMediaBuffer));

struct Range64
{
	int64_t start;
	int64_t end;
	bool contains(int64_t v) const { return (start <= v) && (v < end); }
	int64_t getLength() const { return end - start; }
};

struct TestMFSourceReader
{
	IMFSourceReaderPtr mfSourceReader;
	int64_t lengthInSamples = 0;
	double sampleRate = 0;
	unsigned int bitsPerSample = 0;
	unsigned int numChannels = 0;
	bool usesFloatingPointData = false;
	void close()
	{
		mfSourceReader = nullptr;
		lengthInSamples = 0;
		sampleRate = 0;
		bitsPerSample = 0;
		numChannels = 0;
		usesFloatingPointData = false;
	}
	HRESULT open(LPCWSTR path)
	{
		close();
		HRESULT r = createSourceReader(path);
		if(SUCCEEDED(r)) r = configureSourceReader();
		if(SUCCEEDED(r)) r = determineSourceAttributes();
		// TODO: collect metadata
		if(FAILED(r)) std::wcout << "ERROR: failed to open the media file" << std::endl;
		return r;
	}
	HRESULT createSourceReader(LPCWSTR path)
	{
		return MFCreateSourceReaderFromURL(path, NULL, &mfSourceReader);
	}
	HRESULT configureSourceReader()
	{
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
		assert(bitsPerSample == 32); // this implementation assumes that the output sample type is either float32 or int32
		// length
		PROPVARIANT vdur = {}; // means PropVariantInit()
		r = mfSourceReader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &vdur);
		if(FAILED(r)) { std::wcout << "ERROR: faild to get MF_PD_DURATION " << std::endl; return r; }
		LONGLONG durhns = 0; PropVariantToInt64(vdur, &durhns);
		// NOTE: PropVariantClear() is not needed, because I know that the PROPVARIANT is a simple scalar value
		lengthInSamples = hnsToSample(durhns);
		std::wcout << "sampleRate=" << sampleRate << std::endl;
		std::wcout << "numChannels=" << (int)numChannels << std::endl;
		std::wcout << "bitsPerSample=" << (int)bitsPerSample << " isfloat=" << (usesFloatingPointData ? "yes" : "no") << std::endl;
		std::wcout << "lengthInSamples = " << lengthInSamples << std::endl;
		std::wcout << std::endl;
		return S_OK;
	}
	bool isValid() const
	{
		if(!mfSourceReader) { std::wcout << "source reader is null" << std::endl; return false; }
		if((sampleRate <= 0) || (numChannels == 0) || (bitsPerSample == 0)) { std::wcout << "invalid format" << std::endl; return false; }
		return true;
	}
	int64_t sampleToHns(int64_t spos) const
	{
		//return (int64_t)(spos * 10000000ll) / (int64_t)sampleRate;
		return (int64_t)((double)spos * 10000000.0 / sampleRate + 0.5);
	}
	int64_t hnsToSample(int64_t tpos) const
	{
		//return (int64_t)(tpos * (int64_t)sampleRate) / 10000000ll;
		return (int64_t)((double)tpos * sampleRate / 10000000.0 + 0.5);
	}
	bool seekStream(int64_t spos)
	{
		PROPVARIANT vpos = {}; InitPropVariantFromInt64(sampleToHns(spos), &vpos);
		return SUCCEEDED(mfSourceReader->SetCurrentPosition(GUID_NULL, vpos));
	}
	bool loadNextSampleBuffer(IMFSample** ppsample, IMFMediaBuffer** ppmb, Range64* prange)
	{
		while(true)
		{
			IMFSamplePtr smpl;
			LONGLONG tstart = 0;
			DWORD flags = 0;
			HRESULT r = mfSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &tstart, &smpl);
			if(FAILED(r)) { std::wcout << "ReadSample() failed" << std::endl; return false; }
			if(flags & MF_SOURCE_READERF_ERROR) { std::wcout << "MF_SOURCE_READERF_ERROR" << std::endl; return false; }
			if(flags & MF_SOURCE_READERF_ENDOFSTREAM) { std::wcout << "MF_SOURCE_READERF_ENDOFSTREAM" << std::endl; return false; }
			if(flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) { std::wcout << "MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED" << std::endl; return false; }
			if(flags & MF_SOURCE_READERF_STREAMTICK) { std::wcout << "MF_SOURCE_READERF_STREAMTICK" << std::endl; }
			if(!smpl) continue;
			LONGLONG tduration = 0; smpl->GetSampleDuration(&tduration);
			*ppsample = smpl; smpl->AddRef();
			smpl->ConvertToContiguousBuffer(ppmb);
			*prange = { hnsToSample(tstart), hnsToSample(tstart + tduration) };
			return true;
		}
		std::wcout << "never reached" << std::endl;
		return false;
	}
	void testReadThrough()
	{
		std::wcout << "-- read through test --" << std::endl;
		int64_t totalnumsamples = 0;
		int64_t prevend = 0;
		while(1)
		{
			IMFSamplePtr smpl;
			IMFMediaBufferPtr medbuf;
			Range64 range;
			if(loadNextSampleBuffer(&smpl, &medbuf, &range))
			{
				bool discontinuity = prevend != range.start;
				std::wcout << "read: range={ " << range.start << ", " << range.end << " } (" << range.getLength() << ")";
				if(discontinuity) std::wcout << " **DISCONTINUITY**";
				std::wcout << std::endl;
				totalnumsamples += range.getLength();
				prevend = range.end;
			}
			else break;
		}
		std::wcout << "totalnumsamples=" << totalnumsamples;
		bool lengthmismatch = totalnumsamples != lengthInSamples;
		if(lengthmismatch) std::wcout << " **LENGTHMISMATCH**";
		std::wcout << std::endl;
		std::wcout << std::endl;
	}
	void testSeeking()
	{
		std::wcout << "-- seeking test --" << std::endl;
		for(int iseek = 0; iseek < 4; ++iseek)
		{
			int64_t newpos = 4000 * iseek;
			std::wcout << "seek: newpos=" << newpos << std::endl;
			seekStream(newpos);
			int64_t prevend = 0;
			for(int iread = 0; iread < 8; ++iread)
			{
				IMFSamplePtr smpl;
				IMFMediaBufferPtr medbuf;
				Range64 range;
				if(loadNextSampleBuffer(&smpl, &medbuf, &range))
				{
					if(iread == 0) prevend = range.start;
					std::wcout << "read: range={ " << range.start << ", " << range.end << " } (" << range.getLength() << ")";
					bool discontinuity = (0 < iread) && (prevend != range.start);
					bool improperseek = (iread == 0) && (newpos < range.start);
					if(discontinuity) std::wcout << " **DISCONTINUITY**";
					if(improperseek) std::wcout << " **IMPROPERSEEK**";
					std::wcout << std::endl;
					prevend = range.end;
				}
				else break;
			}
		}
		std::wcout << std::endl;
	}
};

struct MFScopedInit
{
	MFScopedInit()
	{
		HRESULT r = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if(FAILED(r)) std::wcout << "ERROR: CoInitializeEx() " << _com_error(r).ErrorMessage() << std::endl;
		MFStartup(MF_VERSION);
	}
	~MFScopedInit()
	{
		MFShutdown();
		CoUninitialize();
	}
};

void showUsage()
{
	std::wcout << "usage: TestMFSourceReader [-h] inputfile" << std::endl;
}

int wmain(int argc, wchar_t* argv[])
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	std::wstring inputfile;
	bool showusage = false;
	for(int argi = 1; argi < argc; ++argi)
	{
		if((argv[argi][0] == '/') || (argv[argi][0] == '-'))
		{
			if(argv[argi][1] == 'h') showusage = true;
			continue;
		}
		WCHAR path[1024] = {}; GetFullPathNameW(argv[argi], _countof(path), path, NULL);
		if(!PathFileExistsW(path)) continue;
		inputfile = path;
		break;
	}
	if(inputfile.empty() || showusage)
	{
		showUsage();
		return 0;
	}
	MFScopedInit mfinit;
	std::wcout << "inputfile=" << inputfile << std::endl;
	TestMFSourceReader test;
	if(FAILED(test.open(inputfile.c_str()))) return 0;
	if(!test.isValid()) return 0;
	test.testReadThrough();
	test.testSeeking();
	return 0;
}

