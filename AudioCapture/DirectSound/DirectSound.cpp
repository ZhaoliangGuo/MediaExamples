// DirectSound.cpp : 定义控制台应用程序的入口点。
/*
References:
https://www.cnblogs.com/wangjzh/p/4243285.html
http://blog.csdn.net/leixiaohua1020/article/details/40540147
http://dev.yesky.com/246/2032246.shtml
*/

#include "stdafx.h"

#include <dsound.h>
#include <tchar.h>
#include <locale.h>
#include <vector>
#include <iostream>
using namespace std;

#define SAFE_DELETE(punk)  \
	if ((punk) != NULL)  \
    { delete [] (punk); (punk) = NULL; }

#define SAFE_RELEASE(punk)  \
	if ((punk) != NULL)  \
{ (punk)->Release(); (punk) = NULL; }

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")

#define MAX_AUDIO_BUF (64) 

int _tmain(int argc, _TCHAR* argv[])
{  
	HRESULT hr = NOERROR;

	LPDIRECTSOUNDCAPTURE8	    lpDSCapture8;
	LPDIRECTSOUNDCAPTUREBUFFER 	lpDSCaptureBuffer_Main;
	LPDIRECTSOUNDCAPTUREBUFFER8	lpDSCaptureBuffer8_Sub;
	IDirectSoundNotify8         *pIDSNotify = NULL; 

	DSBPOSITIONNOTIFY pDSPostionNotify[MAX_AUDIO_BUF];
	HANDLE            hEvent[MAX_AUDIO_BUF];

	WAVEFORMATEX sWaveFormat;
	memset(&sWaveFormat, 0, sizeof(WAVEFORMATEX));  
	sWaveFormat.wFormatTag      = WAVE_FORMAT_PCM;  
	sWaveFormat.nSamplesPerSec  = 48000;  
	sWaveFormat.nChannels       = 2;  
	sWaveFormat.wBitsPerSample  = 16;  
	sWaveFormat.nBlockAlign     = sWaveFormat.nChannels * sWaveFormat.wBitsPerSample / 8; 
	sWaveFormat.nAvgBytesPerSec = sWaveFormat.nSamplesPerSec * sWaveFormat.nBlockAlign;   

	int nBufferSizeInMS       = 500; // 500ms
	DWORD dwCaptureBufferSize = (sWaveFormat.nSamplesPerSec * sWaveFormat.nBlockAlign * nBufferSizeInMS) / 1000;

	int nOneFrameInMS      = 10;  // 10ms
	int nOneFrameInBytes   = (sWaveFormat.nSamplesPerSec * sWaveFormat.nBlockAlign * nOneFrameInMS) / 1000;
	
	DSCBUFFERDESC dsCBufferDesc;
	ZeroMemory(&dsCBufferDesc, sizeof(DSCBUFFERDESC));
	dsCBufferDesc.dwSize        = sizeof(DSCBUFFERDESC);
	dsCBufferDesc.dwBufferBytes = dwCaptureBufferSize;
	dsCBufferDesc.lpwfxFormat   = &sWaveFormat;
	dsCBufferDesc.dwFlags       = 0;
	dsCBufferDesc.lpDSCFXDesc   = NULL;

	DWORD  dwNotifySize     = nOneFrameInBytes;
	int    nNotifyCnt       = nBufferSizeInMS / nOneFrameInMS;

	HANDLE              g_hNotificationEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	DSBPOSITIONNOTIFY	*g_aPosNotify        = new DSBPOSITIONNOTIFY[nNotifyCnt];

	for( int i = 0; i < nNotifyCnt; i++ )
	{
		g_aPosNotify[i].dwOffset     = (dwNotifySize * i) + dwNotifySize - 1;   
		g_aPosNotify[i].hEventNotify = g_hNotificationEvent; 
	}

	SYSTEMTIME st;
	GetLocalTime(&st);
	char szFileName[_MAX_PATH] = {0};
	sprintf(szFileName, ".\\output_%04d%02d%02d_%02d%02d%02d_%03d.pcm", 
		    st.wYear, st.wMonth, st.wDay, 
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	FILE *fpOutputPCMFile = fopen(szFileName, "wb+"); // 这里必须是二进制格式的，否则保存的PCM文件存在异常

	do
	{
		hr = DirectSoundCaptureCreate8(NULL,  // NULL for the default device
			                           &lpDSCapture8, 
									   NULL);
		if (FAILED(hr))
		{
			hr = DirectSoundCaptureCreate8(NULL, &lpDSCapture8, NULL);
			if (FAILED(hr))
			{
				break;
			}
		}

		// To retrieve the capabilities of a capture device
		// 采集设备所支持的情况
		DSCCAPS DSCCaps;
		DSCCaps.dwSize = sizeof(DSCCaps);
		lpDSCapture8->GetCaps(&DSCCaps);
		printf("\nIDirectSoundCapture::GetCaps...\n");
		cout<<"dwFormats  : "<<DSCCaps.dwFormats<<endl
			<<"dwChannels : "<<DSCCaps.dwChannels<<endl
			<<"dwFlags    : "<<DSCCaps.dwFlags<<endl
			<<"dwSize     : "<<DSCCaps.dwSize<<endl<<endl;


		// 创建一个主缓冲对象
		hr = lpDSCapture8->CreateCaptureBuffer(&dsCBufferDesc, &lpDSCaptureBuffer_Main, NULL);
		if (FAILED(hr) || NULL == lpDSCaptureBuffer_Main)
		{
			break;
		}

		// 创建通知对象，通知应用程序指定播放位置已经达到。
		hr = lpDSCaptureBuffer_Main->QueryInterface(IID_IDirectSoundNotify, (LPVOID*)&pIDSNotify);
		if(FAILED(hr))
		{
			return FALSE;			
		}

		// 设置通知位置
		hr = pIDSNotify->SetNotificationPositions(nNotifyCnt, g_aPosNotify);
		if(FAILED(hr))
		{
			return S_FALSE;
		}
		
		// 创建一个副缓冲对象
		hr = lpDSCaptureBuffer_Main->QueryInterface(IID_IDirectSoundCaptureBuffer8, 
			                                        (LPVOID*)&lpDSCaptureBuffer8_Sub);
		if (FAILED(hr))
		{
			printf("创建副缓冲区失败.\n");
			break;
		}
	
		pIDSNotify->Release();

		hr = lpDSCaptureBuffer8_Sub->GetFormat(&sWaveFormat, sizeof(WAVEFORMATEX), NULL);
		if (FAILED(hr))
		{
			break;
		}

		printf("\nIDirectSoundCaptureBuffer8::GetFormat...\n");
		cout<<"wFormatTag      : "<<sWaveFormat.wFormatTag<<endl
			<<"nChannels       : "<<sWaveFormat.nChannels<<endl
			<<"nSamplesPerSec  : "<<sWaveFormat.nSamplesPerSec<<endl
			<<"nAvgBytesPerSec : "<<sWaveFormat.nAvgBytesPerSec<<endl
			<<"nBlockAlign     : "<<sWaveFormat.nBlockAlign<<endl
			<<"wBitsPerSample  : "<<sWaveFormat.wBitsPerSample<<endl
			<<"cbSize          : "<<sWaveFormat.cbSize<<endl<<endl;

		// retrieve the size of a capture buffer (获取采集buffer的大小)
		DSCBCAPS sDSCBCAPS;
		sDSCBCAPS.dwSize = sizeof(DSCBCAPS);
		lpDSCaptureBuffer8_Sub->GetCaps(&sDSCBCAPS);
		printf("\nIDirectSoundCaptureBuffer8::GetCaps ... \n");
		cout<<"dwBufferBytes : "<< sDSCBCAPS.dwBufferBytes<<endl
			<<"dwFlags       : "<< sDSCBCAPS.dwFlags<<endl<<endl;

		// Start Playing
		/*
		Start the buffer by calling the IDirectSoundCaptureBuffer8::Start method. 
		Normally you should pass DSCBSTART_LOOPING in the dwFlags parameter 
		so that the buffer will keep running continuously rather than stopping when it reaches the end. 
		Audio data from the input device begins filling the buffer from the beginning.
		*/
		lpDSCaptureBuffer8_Sub->Start(DSCBSTART_LOOPING);

		BOOL   bPlaying     = TRUE;
		LPVOID lpBuffer1    = NULL;
		DWORD  dwBufferLen1 = 0;

		LPVOID lpBuffer2    = NULL;
		DWORD  dwBufferLen2 = 0;

		DWORD dwReadPos           = 0;
		DWORD dwCapturePos        = 0;
		DWORD dwNextCaptureOffset = 0;

		while(bPlaying)
		{
			DWORD dwResult = 0;
			dwResult = WaitForMultipleObjects(1, &g_hNotificationEvent, FALSE, INFINITE);
			switch(dwResult)
			{
				case WAIT_OBJECT_0 + 0:
				
				/* 
					https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.idirectsoundcapturebuffer8.idirectsoundcapturebuffer8.getstatus(v=vs.85).aspx
					To find out what a capture buffer is currently doing
					
					This method fills a DWORD variable with a combination of flags 
					that indicate whether the buffer is busy capturing, 
					and if so, whether it is looping; 
					that is, whether the DSCBSTART_LOOPING flag was set in the last call to IDirectSoundCaptureBuffer8::Start.

					The status can be set to one or more of the following:
					         Value	                  Description
					DSCBSTATUS_CAPTURING	The buffer is capturing audio data.
					DSCBSTATUS_LOOPING	    The buffer is looping.
				*/
				DWORD dwStatus;
				if (FAILED(lpDSCaptureBuffer8_Sub->GetStatus(&dwStatus))) 
				{
					printf("lpDSBuffer8_Sub->GetStatus fail. \n");
					return -1;
				}

				if (!(dwStatus & DSCBSTATUS_CAPTURING) ) // 验证是否正在采集
				{
					printf("dwStatus & DSCBSTATUS_CAPTURING is false. dwStatus: 0x%d\n", dwStatus);
					return -1;
				}

				/*
						The CaptureBuffer.GetCurrentPosition method 
						returns the offsets of the read and capture cursors within the buffer. 
						The read cursor is at the end of the data 
						that has been fully captured into the buffer at this point. 
						The capture cursor is at the end of the block of data 
						that is currently being copied into the buffer. 
						You cannot safely retrieve data between the read cursor and the capture cursor.					
				*/
				hr = lpDSCaptureBuffer8_Sub->GetCurrentPosition(&dwCapturePos, &dwReadPos);
				if (SUCCEEDED(hr))
				{
					LONG lLockSize = dwReadPos - dwNextCaptureOffset;
					if( lLockSize < 0 ) // 循环buffer dwReadPos指针再次到了头部
					{
						lLockSize += dwCaptureBufferSize;   
					}

					if (lLockSize < dwNotifySize)
					{
						break;
					}

					// 计算本次最多读取多少个数据
					// 该数值为dwNotifySize的整数倍
					int nCnt      = lLockSize / dwNotifySize;
					int nLockSize = nCnt * dwNotifySize;

					/*{
						FILE *fp = fopen("C:\\POS.TXT", "a+");
						if (fp)
						{
							fprintf(fp, "dwCapturePos:  %8u  dwReadPos:  %8u nLockSize: %8d, dwNextCaptureOffset:%8u %8u\n", 
								dwCapturePos, dwReadPos, nLockSize, dwNextCaptureOffset, nLockSize + dwNextCaptureOffset);
							fclose(fp);
							fp = NULL;
						}
					}*/

					// 锁定副缓冲区
					/*
					    https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.idirectsoundcapturebuffer8.idirectsoundcapturebuffer8.lock(v=vs.85).aspx
						This method accepts an offset and a byte count, 
						and returns two read pointers and their associated sizes. 

						If the locked portion does not extend to the end of the buffer and wrap to the beginning, 
						the second pointer, ppvAudioBytes2, receives NULL. 
						If the lock does wrap, ppvAudioBytes2 points to the beginning of the buffer.

						If the application passes NULL for the ppvAudioPtr2 and pdwAudioBytes2 parameters, 
						the lock extends no further than the end of the buffer and does not wrap.

						The application should read data from the pointers returned by this method 
						and then immediately call Unlock. 
						The sound buffer should not remain locked while it is running; 
						if it does, the capture cursor will reach the locked bytes and audio problems may result.
					*/
					lpDSCaptureBuffer8_Sub->Lock(dwNextCaptureOffset, 
						                         nLockSize,
												 &lpBuffer1, &dwBufferLen1, 
												 &lpBuffer2, &dwBufferLen2, 0);

					printf("dwBufferLen : %u, dwBufferLen2 : %u\n", dwBufferLen1);

					if (fpOutputPCMFile && lpBuffer1)
					{	
						dwNextCaptureOffset += dwBufferLen1; 
						fwrite((BYTE *)lpBuffer1, sizeof(char), dwBufferLen1, fpOutputPCMFile);
						fflush(fpOutputPCMFile);	
					}

					if (fpOutputPCMFile && lpBuffer2)
					{	
						dwNextCaptureOffset += dwBufferLen2; 
						fwrite((BYTE *)lpBuffer2, sizeof(char), dwBufferLen2, fpOutputPCMFile);
						fflush(fpOutputPCMFile);	
					}
		
					dwNextCaptureOffset %= dwCaptureBufferSize; // Circular buffer

					// 解锁副缓冲区
					lpDSCaptureBuffer8_Sub->Unlock(lpBuffer1, dwBufferLen1, lpBuffer2, dwBufferLen2);		
				}			
				
				//ResetEvent(g_hNotificationEvent);

				break;
			}

		} // end of 'while(isPlaying)' 

	} while (0); // end of 'do'

	if (fpOutputPCMFile)
	{
		fclose(fpOutputPCMFile);
		fpOutputPCMFile = NULL;
	}

	if (lpDSCaptureBuffer8_Sub)
	{
		lpDSCaptureBuffer8_Sub->Stop();
	}
	
	/*if (g_hNotificationEvent)
	{
		CloseHandle(g_hNotificationEvent);  
	}*/
	 
	SAFE_RELEASE(lpDSCaptureBuffer8_Sub);
	SAFE_RELEASE(lpDSCaptureBuffer_Main);
	SAFE_RELEASE(lpDSCapture8);

	getchar();

	return 0;
}

