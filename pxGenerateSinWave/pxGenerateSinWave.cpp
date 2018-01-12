// pxGenerateSinWave.cpp
// 生成正弦波的小demo
// 按照pcm 16bit位深的方式生成
#include "stdafx.h"
#include <windows.h>
#include <iostream>
using namespace std;
#include "ToneGen.h"

struct RenderBuffer
{
	RenderBuffer *  _Next;
	UINT32          _BufferLength;
	BYTE *          _Buffer;

	RenderBuffer() :
		_Next(NULL),
		_BufferLength(0),
		_Buffer(NULL)
	{
	}

	~RenderBuffer()
	{
		delete [] _Buffer;
		_Buffer = NULL;
	}
};

int main( void)
{
	int    nChannels          = 2;
	int    nSamplesPerSecond  = 48000;
	double theta              = 0;

	int    TargetFrequency    = 200;

	UINT32 renderBufferSizeInBytes = nSamplesPerSecond * 10 * 2 * 2 / 1000;

	RenderBuffer *renderBuffer = new (std::nothrow) RenderBuffer();
	if (renderBuffer == NULL)
	{
		printf("Unable to allocate render buffer\n");
		return -1;
	}

	renderBuffer->_BufferLength = renderBufferSizeInBytes;
	renderBuffer->_Buffer = new (std::nothrow) BYTE[renderBufferSizeInBytes];
	if (renderBuffer->_Buffer == NULL)
	{
		printf("Unable to allocate render buffer\n");
		return -1;
	}

	GenerateSineSamples <short> (renderBuffer->_Buffer, 
		renderBuffer->_BufferLength, 
		TargetFrequency,
		nChannels, 
		nSamplesPerSecond, 
		&theta);

	int nSampleCnt = renderBuffer->_BufferLength / 2;

	short *psPCMBuffer = new short[nSampleCnt];

	memcpy(psPCMBuffer, renderBuffer->_Buffer, renderBuffer->_BufferLength);

	SYSTEMTIME st;
	GetLocalTime(&st);
	char szFileName[_MAX_PATH] = {0};
	sprintf(szFileName, ".\\data_%04d-%02d-%02d_%02d_%02d_%02d_%03d.txt", 
		st.wYear, st.wMonth, st.wDay, 
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	FILE *fp = fopen(szFileName, "a+");

	for (int i = 0; i < nSampleCnt; i++)
	{
		if (fp)
		{
			fprintf(fp, "%d\n", psPCMBuffer[i]);
			fflush(fp);
		}	
	}

	if (fp)
	{
		fclose(fp);
		fp = NULL;
	}

	if (psPCMBuffer)
	{
		delete [] psPCMBuffer;
		psPCMBuffer = NULL;
	}

	printf("Done!!!\n");

	getchar();

	return 0;
}
