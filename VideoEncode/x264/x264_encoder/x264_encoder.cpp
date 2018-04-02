#include "stdafx.h"

/*
References:
https://github.com/leixiaohua1020/simplest_encoder/tree/master/simplest_x264_encoder
x264官方提供的example.c
*/
#include <Windows.h>

#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
	#include "x264.h"
};
#else
	#include "x264.h"
#endif

// 生成.264文件可以用Elecard StreamEye观看

int _tmain(int argc, _TCHAR* argv[])
{
	printf("x264_encoder ...\n");

	SYSTEMTIME st;
	GetLocalTime(&st);

	char szYUVFile[256] = {0};
	char sz264File[256] = {0};

	sprintf(szYUVFile, ".\\sample_640x360_yuv420p.yuv");
	sprintf(sz264File, ".\\output_%04d%02d%02d_%02d%02d%02d.264", 
		    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	FILE *fpYUV = fopen(szYUVFile, "rb");
	if (!fpYUV)
	{
		printf("fopen %s fail.\n", szYUVFile);
		return -1;
	}

	FILE *fp264 = fopen(sz264File, "wb");
	if (!fp264)
	{
		printf("fopen %s fail.\n", sz264File);
		return -1;
	}

	int nWidth  = 640;
	int nHeight = 360;

	x264_picture_t pic;
	x264_picture_t pic_out;

	x264_picture_init(&pic);

	int nRet = x264_picture_alloc(&pic, X264_CSP_I420, nWidth, nHeight);
	if (nRet < 0)
	{
		printf("x264_picture_alloc fail\n");
		return -1;
	}

	/*x264_param_t default_264_param;
	x264_param_default(&default_264_param);*/

	x264_param_t param;

	/* Get default params for preset/tuning */
	if( x264_param_default_preset( &param, "medium", NULL ) < 0 )
	{
		printf("x264_param_default_preset fail. \n");

		return -1;
	}

	/* Configure non-default params */
	//param.i_bitdepth  = 8;
	param.i_csp            = X264_CSP_I420;
	param.i_width          = nWidth;
	param.i_height         = nHeight;
	param.b_vfr_input      = 0;
	param.b_repeat_headers = 1;
	param.b_annexb         = 1;

	/* Apply profile restrictions. */
	if( x264_param_apply_profile( &param, "high" ) < 0 )
	{
		printf("x264_param_apply_profile fail. \n");
		return -1;
	}

	int luma_size   = nWidth * nHeight;
	int chroma_size = luma_size / 4;

	x264_t     *p264_t = NULL;
	x264_nal_t *pnal_t = NULL;
	int        iNAL    = 0;

	int i_frame = 0;
	int i_frame_size;

	// 打开编码器
	p264_t = x264_encoder_open(&param);
	if (NULL == p264_t)
	{
		printf("x264_encoder_open fail.\n");
		return -1;
	}

	// 返回SPS PPS信息
	/*int nNALsInBytes = x264_encoder_headers(p264_t, &pnal_t, &iNAL);
	if (nNALsInBytes < 0)
	{
	printf("x264_encoder_headers fail. \m");
	return -1;
	}*/

	x264_nal_t *nal;
	int i_nal;

	/* Encode frames */
	for( ;; i_frame++ )
	{
		/* Read input frame */
		if( fread( pic.img.plane[0], 1, luma_size, fpYUV ) != luma_size )
		{
			break;
		}

		if( fread( pic.img.plane[1], 1, chroma_size, fpYUV ) != chroma_size )
		{
			break;
		}

		if( fread( pic.img.plane[2], 1, chroma_size, fpYUV ) != chroma_size )
		{
			break;
		}

		pic.i_pts = i_frame;
		i_frame_size = x264_encoder_encode( p264_t, &nal, &i_nal, &pic, &pic_out );
		if( i_frame_size < 0 )
		{
			break;
		}

		if (i_frame_size > 0)
		{
			if( !fwrite( nal->p_payload, i_frame_size, 1, fp264 ) )
			{
				return -1;
			}
		}
	}

	/* Flush delayed frames */
	while( x264_encoder_delayed_frames( p264_t ) )
	{
		i_frame_size = x264_encoder_encode( p264_t, &nal, &i_nal, NULL, &pic_out );
		if( i_frame_size < 0 )
		{
			break;
		}
		
		if (i_frame_size > 0)
		{
			if( !fwrite( nal->p_payload, i_frame_size, 1, fp264 ) )
			{
				return -1;
			}
		}
	}

	// 关闭解码器
	x264_encoder_close(p264_t);
	x264_picture_clean(&pic);

	if (fpYUV)
	{
		fclose(fpYUV);
		fpYUV = NULL;
	}
	
	if (fp264)
	{
		fclose(fp264);
		fp264 = NULL;
	}

	printf("x264_encoder Done.\n");

	getchar();

	return 0;
} 

