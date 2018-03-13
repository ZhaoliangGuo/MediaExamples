// WindowsAudioSession.cpp
// 基本的利用WAS采集音频的demo
/*
References:
https://msdn.microsoft.com/en-us/library/dd370800(v=vs.85).aspx
http://blog.csdn.net/leave_rainbow/article/details/50917043
http://blog.csdn.net/lwsas1/article/details/46862195?locationNum=1
WindowsSDK7-Samples-master\multimedia\audio\CaptureSharedEventDriven
*/

#include "stdafx.h"

#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <iostream>
using namespace std;

// ns(nanosecond) : 纳秒，时间单位。一秒的十亿分之一
// 1秒=1000毫秒; 1毫秒=1000微秒; 1微秒=1000纳秒

// The REFERENCE_TIME data type defines the units for reference times in DirectShow. 
// Each unit of reference time is 100 nanoseconds.(100纳秒为一个REFERENCE_TIME时间单位)

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC       (10000000)
#define REFTIMES_PER_MILLISEC  (10000)

#define EXIT_ON_ERROR(hres)  \
	if (FAILED(hres)) { goto Exit; }

#define SAFE_RELEASE(punk)  \
	if ((punk) != NULL)  \
				{ (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID   IID_IMMDeviceEnumerator  = __uuidof(IMMDeviceEnumerator);
const IID   IID_IAudioClient         = __uuidof(IAudioClient);
const IID   IID_IAudioCaptureClient  = __uuidof(IAudioCaptureClient);

#define MoveMemory RtlMoveMemory
#define CopyMemory RtlCopyMemory
#define FillMemory RtlFillMemory
#define ZeroMemory RtlZeroMemory

#define min(a,b)            (((a) < (b)) ? (a) : (b))

//
//  WAV file writer.
//
//  This is a VERY simple .WAV file writer.
//

//
//  A wave file consists of:
//
//  RIFF header:    8 bytes consisting of the signature "RIFF" followed by a 4 byte file length.
//  WAVE header:    4 bytes consisting of the signature "WAVE".
//  fmt header:     4 bytes consisting of the signature "fmt " followed by a WAVEFORMATEX 
//  WAVEFORMAT:     <n> bytes containing a waveformat structure.
//  DATA header:    8 bytes consisting of the signature "data" followed by a 4 byte file length.
//  wave data:      <m> bytes containing wave data.
//
//
//  Header for a WAV file - we define a structure describing the first few fields in the header for convenience.
//
struct WAVEHEADER
{
	DWORD   dwRiff;                     // "RIFF"
	DWORD   dwSize;                     // Size
	DWORD   dwWave;                     // "WAVE"
	DWORD   dwFmt;                      // "fmt "
	DWORD   dwFmtSize;                  // Wave Format Size
};

//  Static RIFF header, we'll append the format to it.
const BYTE WaveHeader[] = 
{
	'R',   'I',   'F',   'F',  0x00,  0x00,  0x00,  0x00, 'W',   'A',   'V',   'E',   'f',   'm',   't',   ' ', 0x00, 0x00, 0x00, 0x00
};

//  Static wave DATA tag.
const BYTE WaveData[] = { 'd', 'a', 't', 'a'};

//
//  Write the contents of a WAV file.  We take as input the data to write and the format of that data.
//
bool WriteWaveFile(HANDLE FileHandle, const BYTE *Buffer, const size_t BufferSize, const WAVEFORMATEX *WaveFormat)
{
	DWORD waveFileSize = sizeof(WAVEHEADER) + sizeof(WAVEFORMATEX) + WaveFormat->cbSize + sizeof(WaveData) + sizeof(DWORD) + static_cast<DWORD>(BufferSize);
	BYTE *waveFileData = new (std::nothrow) BYTE[waveFileSize];
	BYTE *waveFilePointer = waveFileData;
	WAVEHEADER *waveHeader = reinterpret_cast<WAVEHEADER *>(waveFileData);

	if (waveFileData == NULL)
	{
		printf("Unable to allocate %d bytes to hold output wave data\n", waveFileSize);
		return false;
	}

	//
	//  Copy in the wave header - we'll fix up the lengths later.
	//
	CopyMemory(waveFilePointer, WaveHeader, sizeof(WaveHeader));
	waveFilePointer += sizeof(WaveHeader);

	//
	//  Update the sizes in the header.
	//
	waveHeader->dwSize = waveFileSize - (2 * sizeof(DWORD));
	waveHeader->dwFmtSize = sizeof(WAVEFORMATEX) + WaveFormat->cbSize;

	//
	//  Next copy in the WaveFormatex structure.
	//
	CopyMemory(waveFilePointer, WaveFormat, sizeof(WAVEFORMATEX) + WaveFormat->cbSize);
	waveFilePointer += sizeof(WAVEFORMATEX) + WaveFormat->cbSize;


	//
	//  Then the data header.
	//
	CopyMemory(waveFilePointer, WaveData, sizeof(WaveData));
	waveFilePointer += sizeof(WaveData);
	*(reinterpret_cast<DWORD *>(waveFilePointer)) = static_cast<DWORD>(BufferSize);
	waveFilePointer += sizeof(DWORD);

	//
	//  And finally copy in the audio data.
	//
	CopyMemory(waveFilePointer, Buffer, BufferSize);

	//
	//  Last but not least, write the data to the file.
	//
	DWORD bytesWritten;
	if (!WriteFile(FileHandle, waveFileData, waveFileSize, &bytesWritten, NULL))
	{
		printf("Unable to write wave file: %d\n", GetLastError());
		delete []waveFileData;
		return false;
	}

	if (bytesWritten != waveFileSize)
	{
		printf("Failed to write entire wave file\n");
		delete []waveFileData;
		return false;
	}
	delete []waveFileData;
	return true;
}

//
//  Write the captured wave data to an output file so that it can be examined later.
//
void SaveWaveData(BYTE *CaptureBuffer, size_t BufferSize, const WAVEFORMATEX *WaveFormat)
{
	HRESULT hr = NOERROR;

	SYSTEMTIME st;
	GetLocalTime(&st);
	char waveFileName[_MAX_PATH] = {0};
	sprintf(waveFileName, ".\\WAS_%04d-%02d-%02d_%02d_%02d_%02d_%02d.wav", 
		st.wYear, st.wMonth, st.wDay, 
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	HANDLE waveHandle = CreateFile(waveFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 
		NULL);
	if (waveHandle != INVALID_HANDLE_VALUE)
	{
		if (WriteWaveFile(waveHandle, CaptureBuffer, BufferSize, WaveFormat))
		{
			printf("Successfully wrote WAVE data to %s\n", waveFileName);
		}
		else
		{
			printf("Unable to write wave file\n");
		}
		CloseHandle(waveHandle);
	}
	else
	{
		printf("Unable to open output WAV file %s: %d\n", waveFileName, GetLastError());
	}
					
}

//#define DEF_CAPTURE_MIC
/*
注1: 静音时 填充0

注2: 测试时 应该将录音设备中的麦克风设为默认设备

注3: 定义DEF_CAPTURE_MIC时仅测试采集麦克风
     否则测试采集声卡。

注4:
	 测试采集声卡:
	 Initialize时需要设置AUDCLNT_STREAMFLAGS_LOOPBACK
	 这种模式下，音频engine会将rending设备正在播放的音频流， 拷贝一份到音频的endpoint buffer
	 这样的话，WASAPI client可以采集到the stream.
	 此时仅采集到Speaker的声音

*/

int _tmain(int argc, _TCHAR* argv[])
{
	HRESULT hr;

	IMMDeviceEnumerator *pEnumerator    = NULL;
	IMMDevice           *pDevice        = NULL;
	IAudioClient        *pAudioClient   = NULL;
	IAudioCaptureClient *pCaptureClient = NULL;
	WAVEFORMATEX        *pwfx           = NULL;

	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	UINT32         bufferFrameCount;
	UINT32         numFramesAvailable;

	BYTE           *pData;
	UINT32         packetLength = 0;
	DWORD          flags;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		printf("Unable to initialize COM in render thread: %x\n", hr);
		return hr;
	}

	// 首先枚举你的音频设备
	// 你可以在这个时候获取到你机器上所有可用的设备，并指定你需要用到的那个设置
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	EXIT_ON_ERROR(hr)

#ifdef DEF_CAPTURE_MIC
	hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice); // 采集麦克风
#else 
	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);  // 采集声卡
#endif	

	//hr = pEnumerator->GetDefaultAudioEndpoint(eRender,  eMultimedia, &pDevice);
	EXIT_ON_ERROR(hr)

	// 创建一个管理对象，通过它可以获取到你需要的一切数据
	hr = pDevice->Activate(
		IID_IAudioClient, CLSCTX_ALL,
		NULL, (void**)&pAudioClient);
	EXIT_ON_ERROR(hr)

	hr = pAudioClient->GetMixFormat(&pwfx);
	EXIT_ON_ERROR(hr)

	/*
	typedef struct tWAVEFORMATEX
	{
		WORD        wFormatTag;         // format type 
		WORD        nChannels;          // number of channels (i.e. mono, stereo...) 
		DWORD       nSamplesPerSec;     // sample rate 
		DWORD       nAvgBytesPerSec;    // for buffer estimation 
		WORD        nBlockAlign;        // block size of data
		WORD        wBitsPerSample;     // number of bits per sample of mono data
		WORD        cbSize;             // the count in bytes of the size of  extra information (after cbSize) 
	} WAVEFORMATEX;
    */
	printf("\nGetMixFormat...\n");
	cout<<"wFormatTag      : "<<pwfx->wFormatTag<<endl
		<<"nChannels       : "<<pwfx->nChannels<<endl
		<<"nSamplesPerSec  : "<<pwfx->nSamplesPerSec<<endl
		<<"nAvgBytesPerSec : "<<pwfx->nAvgBytesPerSec<<endl
		<<"nBlockAlign     : "<<pwfx->nBlockAlign<<endl
		<<"wBitsPerSample  : "<<pwfx->wBitsPerSample<<endl
		<<"cbSize          : "<<pwfx->cbSize<<endl<<endl;

	// test for IsFormatSupported
	//////////////////////////////////////////////////////////////////////////
	/*WAVEFORMATEX *wf;
	hr = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, pwfx, &wf);
	if (FAILED(hr)) 
	{
		printf("IsFormatSupported fail.\n");
	}
	printf("\IsFormatSupported...\n");
	cout<<"wFormatTag      : "<<wf->wFormatTag<<endl
		<<"nChannels       : "<<wf->nChannels<<endl
		<<"nSamplesPerSec  : "<<wf->nSamplesPerSec<<endl
		<<"nAvgBytesPerSec : "<<wf->nAvgBytesPerSec<<endl
		<<"nBlockAlign     : "<<wf->nBlockAlign<<endl
		<<"wBitsPerSample  : "<<wf->wBitsPerSample<<endl
		<<"cbSize          : "<<wf->cbSize<<endl<<endl;*/
	//////////////////////////////////////////////////////////////////////////

	int nFrameSize = (pwfx->wBitsPerSample / 8) * pwfx->nChannels;

	cout<<"nFrameSize           : "<<nFrameSize<<" Bytes"<<endl
        <<"hnsRequestedDuration : "<<hnsRequestedDuration
		<<" REFERENCE_TIME time units. 即("<<hnsRequestedDuration/10000<<"ms)"<<endl;

	// 初始化管理对象，在这里，你可以指定它的最大缓冲区长度，这个很重要，
	// 应用程序控制数据块的大小以及延时长短都靠这里的初始化，具体参数大家看看文档解释
	// https://msdn.microsoft.com/en-us/library/dd370875(v=vs.85).aspx
#ifdef DEF_CAPTURE_MIC
	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		0,
		hnsRequestedDuration,
		0,
		pwfx,
		NULL);
#else
	/*
		The AUDCLNT_STREAMFLAGS_LOOPBACK flag enables loopback recording. 
		In loopback recording, the audio engine copies the audio stream 
		that is being played by a rendering endpoint device into an audio endpoint buffer 
		so that a WASAPI client can capture the stream. 
		If this flag is set, the IAudioClient::Initialize method attempts to open a capture buffer on the rendering device. 
		This flag is valid only for a rendering device 
		and only if the Initialize call sets the ShareMode parameter to AUDCLNT_SHAREMODE_SHARED. 
		Otherwise the Initialize call will fail. 
		If the call succeeds, 
		the client can call the IAudioClient::GetService method 
		to obtain an IAudioCaptureClient interface on the rendering device. 
		For more information, see Loopback Recording.
	*/
	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK, // 这种模式下，音频engine会将rending设备正在播放的音频流， 拷贝一份到音频的endpoint buffer
		                              // 这样的话，WASAPI client可以采集到the stream.
									  // 如果AUDCLNT_STREAMFLAGS_LOOPBACK被设置，IAudioClient::Initialize会尝试
									  // 在rending设备开辟一块capture buffer。
									  // AUDCLNT_STREAMFLAGS_LOOPBACK只对rending设备有效，
									  // Initialize仅在AUDCLNT_SHAREMODE_SHARED时才可以使用, 否则Initialize会失败。
						              // Initialize成功后，可以用IAudioClient::GetService可获取该rending设备的IAudioCaptureClient接口。
		hnsRequestedDuration,
		0,
		pwfx,
		NULL);
#endif
	EXIT_ON_ERROR(hr)

	// Get the size of the allocated buffer.
	// 这个buffersize，指的是缓冲区最多可以存放多少帧的数据量
	/*
		https://msdn.microsoft.com/en-us/library/dd370866(v=vs.85).aspx
		HRESULT GetBufferSize(
		[out] UINT32 *pNumBufferFrames
		);
	
		pNumBufferFrames [out]
		Pointer to a UINT32 variable into which the method writes the number of audio frames 
		that the buffer can hold.

		The IAudioClient::Initialize method allocates the buffer. 
		The client specifies the buffer length in the hnsBufferDuration parameter value 
		that it passes to the Initialize method. For rendering clients, 
		the buffer length determines the maximum amount of rendering data 
		that the application can write to the endpoint buffer during a single processing pass. 
		For capture clients, the buffer length determines the maximum amount of capture data 
		that the audio engine can read from the endpoint buffer during a single processing pass. 
	
		The client should always call GetBufferSize after calling Initialize 
		to determine the actual size of the allocated buffer, 
		which might differ from the requested size.

		Rendering clients can use this value to calculate the largest rendering buffer size 
		that can be requested from IAudioRenderClient::GetBuffer during each processing pass.
	*/
	hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	EXIT_ON_ERROR(hr)

	cout<<endl<<"bufferFrameCount : "<<bufferFrameCount<<endl;

	// 创建采集管理接口
	hr = pAudioClient->GetService(
		IID_IAudioCaptureClient,
		(void**)&pCaptureClient);
	EXIT_ON_ERROR(hr)

	// Calculate the actual duration of the allocated buffer.
	hnsActualDuration = (double)REFTIMES_PER_SEC * bufferFrameCount / pwfx->nSamplesPerSec;

	cout<<endl<<"hnsActualDuration : "<<hnsActualDuration<<endl;

	hr = pAudioClient->Start();  // Start recording.
	EXIT_ON_ERROR(hr)

	printf("\nAudio Capture begin...\n\n");

	BOOL bDone = FALSE;
	int  nCnt  = 0;

	size_t nCaptureBufferSize   = 8*1024*1024;
	size_t nCurrentCaptureIndex = 0;

	BYTE *pbyCaptureBuffer = new (std::nothrow) BYTE[nCaptureBufferSize];

	// Each loop fills about half of the shared buffer.
	while (bDone == FALSE)
	{
		// Sleep for half the buffer duration.
		Sleep(hnsActualDuration/REFTIMES_PER_MILLISEC/2);

		hr = pCaptureClient->GetNextPacketSize(&packetLength);
		printf("GetNextPacketSize #0 packetLength:%06u\n", packetLength);
		EXIT_ON_ERROR(hr)

		while (packetLength != 0)
		{
			// Get the available data in the shared buffer.
			// 锁定缓冲区，获取数据
			hr = pCaptureClient->GetBuffer(&pData,
				                           &numFramesAvailable,
				                           &flags, NULL, NULL);
			EXIT_ON_ERROR(hr)

			nCnt++;

			printf("%04d: GetBuffer Success!!! numFramesAvailable:%u\n", nCnt, numFramesAvailable);

			UINT32 framesToCopy = min(numFramesAvailable, static_cast<UINT32>((nCaptureBufferSize - nCurrentCaptureIndex) / nFrameSize));
			if (framesToCopy != 0)
			{
				//
				//  The flags on capture tell us information about the data.
				//
				//  We only really care about the silent flag since we want to put frames of silence into the buffer
				//  when we receive silence.  We rely on the fact that a logical bit 0 is silence for both float and int formats.
				//
				if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					//
					//  Fill 0s from the capture buffer to the output buffer.
					//
					ZeroMemory(&pbyCaptureBuffer[nCurrentCaptureIndex], framesToCopy*nFrameSize);
				}
				else
				{
					//
					//  Copy data from the audio engine buffer to the output buffer.
					//
					CopyMemory(&pbyCaptureBuffer[nCurrentCaptureIndex], pData, framesToCopy*nFrameSize);
				}
				//
				//  Bump the capture buffer pointer.
				//
				nCurrentCaptureIndex += framesToCopy*nFrameSize;
			}

			hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
			EXIT_ON_ERROR(hr)

			hr = pCaptureClient->GetNextPacketSize(&packetLength);
			EXIT_ON_ERROR(hr)

			// test
			//////////////////////////////////////////////////////////////////////////
			UINT32 ui32NumPaddingFrames;
			hr = pAudioClient->GetCurrentPadding(&ui32NumPaddingFrames);
			EXIT_ON_ERROR(hr)

			REFERENCE_TIME hnsStreamLatency;
			hr = pAudioClient->GetStreamLatency(&hnsStreamLatency);
			EXIT_ON_ERROR(hr)

			printf("GetNextPacketSize #0 packetLength:%06u, Padding:%6u, Latency:%I64d\n", 
			       packetLength, ui32NumPaddingFrames, hnsStreamLatency);
			//////////////////////////////////////////////////////////////////////////

			// 采集一定数目个buffer后退出
			if (nCnt == 1000)
			{
				bDone = TRUE;
				break;
			}
		}
	}

	//
	//  We've now captured our wave data.  Now write it out in a wave file.
	//
	SaveWaveData(pbyCaptureBuffer, nCurrentCaptureIndex, pwfx);

	printf("\nAudio Capture Done.\n");

	hr = pAudioClient->Stop();  // Stop recording.
	EXIT_ON_ERROR(hr)

Exit:
	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pDevice)
	SAFE_RELEASE(pAudioClient)
	SAFE_RELEASE(pCaptureClient)

	CoUninitialize();

	if (pbyCaptureBuffer)
	{
		delete [] pbyCaptureBuffer;
		pbyCaptureBuffer = NULL;
	}

	getchar();

	return 0;
}

