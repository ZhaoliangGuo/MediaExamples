#include "stdafx.h"
#include <iostream>
using namespace std;
#include <mmstream.h>
#pragma comment(lib,"Winmm.lib")

/*
References:
https://msdn.microsoft.com/en-us/library/dd743847
http://www.th7.cn/Program/cp/201307/143626.shtml
*/

int _tmain(int argc, _TCHAR* argv[])
{
	UINT uiNumDevs = waveInGetNumDevs();
	printf("waveInGetNumDevs: %3u\n", uiNumDevs);

	if (uiNumDevs < 1)
	{
		printf("no audio capture device.\n");
		return -1;
	}

	HWAVEIN hWaveIn;

	WAVEFORMATEX wfmt;
	memset(&wfmt, 0, sizeof(WAVEFORMATEX));

	wfmt.wFormatTag      = WAVE_FORMAT_PCM;
	wfmt.nChannels       = 2;
	wfmt.wBitsPerSample  = 16; 
	wfmt.nSamplesPerSec  = 44100;
	wfmt.nBlockAlign     = wfmt.nChannels * wfmt.wBitsPerSample / 8;
	wfmt.nAvgBytesPerSec = wfmt.nSamplesPerSec * wfmt.nBlockAlign;
	wfmt.cbSize          = 0;

	#define HEADER_BUF_SIZE (3)

    //#define PCM_BUF_SIZE (10*1024)

	int nFrameInMS = 10;

	const int PCM_BUF_SIZE = wfmt.nSamplesPerSec * wfmt.nBlockAlign * nFrameInMS / 1000;

	WAVEHDR waveHeader[HEADER_BUF_SIZE];
	MMRESULT hr;

	SYSTEMTIME st;
	GetLocalTime(&st);
	char szPCMFileName[_MAX_PATH] = {0};
	sprintf(szPCMFileName, "%04d-%02d-%02d_%02d%02d%02d_%03d.pcm", st.wYear, st.wMonth, st.wDay, 
		    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	FILE *fpPCM = fopen(szPCMFileName, "ab+");

	do 
	{
		hr = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfmt, NULL, 0, CALLBACK_NULL);
		if (MMSYSERR_NOERROR != hr)
		{
			printf("waveInOpen fail. hr = %08x", hr);
			break;
		}

		printf("waveInOpen Success.\n");

		for (int i = 0; i < HEADER_BUF_SIZE; i++)
		{
			memset(&waveHeader[i], 0, sizeof(WAVEHDR));
			//waveHeader[i].lpData = (LPSTR)VirtualAlloc(NULL, PCM_BUF_SIZE + sizeof(WAVEHDR), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			waveHeader[i].lpData = (LPSTR) new (std::nothrow) BYTE[PCM_BUF_SIZE];
			if(NULL == waveHeader[i].lpData)
			{
				printf("%02d new fail. \n", i);

				break;
			}
		
			//waveHeader[i].lpData          = (LPSTR)(pWaveHeader[i] + 1);
			waveHeader[i].dwBufferLength  = PCM_BUF_SIZE;
			waveHeader[i].dwBytesRecorded = 0;
			waveHeader[i].dwUser          = 0;
			waveHeader[i].dwFlags         = 0;
			waveHeader[i].dwLoops         = 1;

			// prepares a buffer for waveform-audio input.
			hr = waveInPrepareHeader(hWaveIn, &waveHeader[i], sizeof(WAVEHDR));
			if (MMSYSERR_NOERROR != hr)
			{
				printf("waveInPrepareHeader fail. hr = %08x\n", hr);
				break;
			}

			printf("%02d waveInPrepareHeader Success.\n", i);

			// The waveInAddBuffer function sends an input buffer to the given waveform-audio input device. 
			// When the buffer is filled, the application is notified.
			// When the buffer is filled, the WHDR_DONE bit is set in the dwFlags member of the WAVEHDR structure.
			hr = waveInAddBuffer(hWaveIn, &waveHeader[i], sizeof(WAVEHDR));
			if (MMSYSERR_NOERROR != hr)
			{
				printf("waveInAddBuffer fail. hr = %08x\n", hr);
				break;
			}

			printf("%02d waveInAddBuffer Success.\n", i);
		}


		hr = waveInStart(hWaveIn);
		if (MMSYSERR_NOERROR != hr)
		{
			printf("waveInStart fail. hr = %08x\n", hr);
			break;
		}

		printf("waveInStart Success.\n");

		int nhdr = 0;

		int nCnt = 0;
		for(;;)
		{
			nCnt++;
			Sleep(10);
			while (waveHeader[nhdr].dwFlags & WHDR_DONE)
			{
				printf("%02d dwBytesRecorded: %u\n", nhdr, waveHeader[nhdr].dwBytesRecorded);

				if (fpPCM)
				{
					fwrite(waveHeader[nhdr].lpData, 1, waveHeader[nhdr].dwBytesRecorded, fpPCM);
					fflush(fpPCM);
				}

				hr = waveInAddBuffer(hWaveIn, &waveHeader[nhdr], sizeof(WAVEHDR));            
				if (hr != MMSYSERR_NOERROR)
				{
					printf("waveInAddBuffer fail. hr = %08x\n", hr);
					break;
				}

				nhdr = (nhdr + 1) % HEADER_BUF_SIZE;
			}

			if (1000 == nCnt)
			{
				printf("******************************\n");
				break;
			}
		}
	} while (0);

	// stops waveform-audio input.
	// If there are any buffers in the queue, the current buffer will be marked as done 
	// (the dwBytesRecorded member in the header will contain the length of data), 
	// but any empty buffers in the queue will remain there.
	hr = waveInStop(hWaveIn);
	if (MMSYSERR_NOERROR != hr)
	{
		printf("waveInStop fail. hr = %08x\n", hr);
		return -1;
	}

	/*
		The waveInReset function stops input on the given waveform-audio input device 
		and resets the current position to zero. 
		All pending buffers are marked as done and returned to the application.
	*/
	hr = waveInReset(hWaveIn);
	if (MMSYSERR_NOERROR != hr)
	{
		printf("waveInReset fail. hr = %08x\n", hr);
		return -1;
	}
	printf("waveInReset Success.\n");

	for (int i = 0; i < HEADER_BUF_SIZE; i++)
	{
		hr = waveInUnprepareHeader(hWaveIn, &waveHeader[i], sizeof(WAVEHDR));
		if (MMSYSERR_NOERROR != hr)
		{
			printf("waveInUnprepareHeader fail. hr = %08x\n", hr);
			return -1;
		}

		printf("%02d waveInUnprepareHeader Success.\n", i);
	}

	for (int i = 0; i < HEADER_BUF_SIZE; i++)
	{
		if (waveHeader[i].lpData)
		{
			delete [] waveHeader[i].lpData;
			//VirtualFree(waveHeader[i].lpData, 0, MEM_RESERVE);
			waveHeader[i].lpData = NULL;

			printf("%02d VirtualFree Success.\n", i);
		}
	}

	/* 
	    The waveInClose function closes the given waveform-audio input device.

	    If there are input buffers that have been sent with the waveInAddBuffer function 
		and that haven't been returned to the application, the close operation will fail. 
		Call the waveInReset function to mark all pending buffers as done.
	*/
	hr = waveInClose(hWaveIn);
	if (MMSYSERR_NOERROR != hr)
	{
		printf("waveInClose fail. hr = %08x\n", hr);
		return -1;
	}
	printf("waveInClose Success.\n");

	if (fpPCM)
	{
		fclose(fpPCM);
		fpPCM = NULL;
	}

	getchar();

	return 0;
}

