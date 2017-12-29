// pxPCMExamples.cpp : 定义控制台应用程序的入口点。
#include "stdafx.h"
#include <fstream>
#include <iostream>
using namespace std;

int _tmain(int argc, _TCHAR* argv[])
{
	// Read file to buffer
	char szFileName[256] = {0};
	sprintf_s(szFileName, ".\\44100_stereo_s16le.pcm");

	ifstream inFile(szFileName, ios::in | ios::binary | ios::ate);
	long nFileSizeInBytes = inFile.tellg(); 

	char *pszPCMBuffer = new char[nFileSizeInBytes];  

	inFile.seekg(0, ios::beg);  
	inFile.read(pszPCMBuffer, nFileSizeInBytes);  
	inFile.close();  

	cout<<"FileName: " << szFileName <<", nFileSizeInBytes: " << nFileSizeInBytes <<endl;
	cout << "Read file to a buffer Done!!!\n\n";  

	// 从文件中读取采样点并打印
	short szItem[1024 * 16] = {0};
	memcpy(szItem, pszPCMBuffer, 1024);

	for (int i = 0; i < 512; i++)
	{
		printf("%6d ", szItem[i]);

		if ((i + 1) % 8 == 0)
		{
			cout<<endl;
		}
	}

	// 将双声道分离成左声道和右声道
	FILE *fpLeft  = fopen("44100_mono_s16le_l.pcm","wb+");  
	FILE *fpRight = fopen("44100_mono_s16le_r.pcm","wb+");

	printf("\n Split stereo to left and right ...\n");

	for (int nPos = 0; nPos < nFileSizeInBytes; )
	{
		if ((nPos / 2) % 2 == 0) // 偶数
		{
			fwrite(pszPCMBuffer + nPos, 1, 2, fpLeft); 
		}
		else
		{
			fwrite(pszPCMBuffer + nPos, 1, 2, fpRight); 
		}

		nPos += 2;
	}

	fclose(fpLeft);  
	fclose(fpRight);  

	printf("\n Split stereo to left and right Done. \n");

	if (pszPCMBuffer)
	{
		delete [] pszPCMBuffer;
		pszPCMBuffer = NULL;
	}

	getchar();

	return 0;
}

