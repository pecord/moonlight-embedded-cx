/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 * Copyrgiht (C) 2016 OtherCrashOverride
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "limelight-common/Limelight.h"

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <amcodec/codec.h>

#include <string.h>

//static FILE* fd;
//static const char* fileName = "fake.h264";
static codec_para_t codecParam = { 0 };
const size_t EXTERNAL_PTS = (1);
const size_t SYNC_OUTSIDE = (2);


// DEBUGGING
#define TESTBUFFER_LENGTH  (32 * 1024 * 10)
static unsigned char testBuffer[TESTBUFFER_LENGTH];
static int testBufferCount = 0;
unsigned long pts = 0;
int captureStream = 1;
int captureCount = 0;
#define CAPTURE_LENGTH ( 1024 * 10 )
static FILE* captureFileHandle = 0;
// END


int osd_blank(char *path,int cmd)
{
    int fd;
    char  bcmd[16];
    fd = open(path, O_CREAT|O_RDWR | O_TRUNC, 0644);

    if(fd>=0) {
        sprintf(bcmd,"%d",cmd);
        if (write(fd,bcmd,strlen(bcmd)) < 0) {
			printf("osd_blank error during write.\n");
		}
        close(fd);
        return 0;
    }

    return -1;
}

void init_display()
{
	osd_blank("/sys/class/graphics/fb0/blank",1);
    osd_blank("/sys/class/graphics/fb1/blank",0);
}

void restore_display()
{
	osd_blank("/sys/class/graphics/fb0/blank",0);
    osd_blank("/sys/class/graphics/fb1/blank",0);
}

void decoder_renderer_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
	printf("videoFormat=%x, width=%d, height=%d, redrawRate=%d, context=%p, drFlags=%x\n",
		videoFormat, width, height, redrawRate, context, drFlags);


	init_display();


	codecParam.stream_type = STREAM_TYPE_ES_VIDEO;
	codecParam.has_video = 1;
	codecParam.noblock = 0;

	switch (videoFormat)
	{
	case VIDEO_FORMAT_H264:	//1
		if (width > 1920 || height > 1080)
		{
			codecParam.video_type = VFORMAT_H264_4K2K; 
			codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_H264_4K2K; ///< video format, such as H264, MPEG2...
		}
		else
		{
			codecParam.video_type = VFORMAT_H264;
			codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_H264;  ///< video format, such as H264, MPEG2...
		}

		printf("Decoding H264 video.\n");
		break;

	case VIDEO_FORMAT_H265: //2
		codecParam.video_type = VFORMAT_HEVC;
		codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_HEVC;  ///< video format, such as H264, MPEG2...

		printf("Decoding HEVC video.\n");
		break;

	default:
		printf("Unsupported video format.\n");
		exit(1);
	}

	//codecParam.vbuf_size = 64 * 1024;

	codecParam.am_sysinfo.width = width;   //< video source width
	codecParam.am_sysinfo.height = height;  //< video source height
	codecParam.am_sysinfo.rate = (96000 / (redrawRate));    //< video source frame duration
	//codecParam.am_sysinfo.extra;   //< extra data information of video stream
	//codecParam.am_sysinfo.status;  //< status of video stream
	//codecParam.am_sysinfo.ratio;   //< aspect ratio of video source
	codecParam.am_sysinfo.param = (void*)(EXTERNAL_PTS | SYNC_OUTSIDE);   //< other parameters for video decoder
	//codecParam.am_sysinfo.ratio64;   //< aspect ratio of video source

	int api = codec_init(&codecParam);
	//printf("codec_init=%x\n", api);

	if (api != 0)
	{
		printf("codec_init failed (%x).\n", api);
		exit(1);
	}

	//int codec_set_syncenable(codec_para_t *pcodec, int enable);
	//api = codec_set_syncenable(&codecParam, 0);
	//printf("codec_set_syncenable=%x\n", api);


	//int codec_get_freerun_mode(codec_para_t *pcodec);
	//api = codec_get_freerun_mode(&codecParam);
	//printf("codec_get_freerun_mode=%x\n", api);

	//int codec_set_freerun_mode(codec_para_t *pcodec, unsigned int mode);

	/*if (captureStream)
	{
		captureFileHandle = fopen("capture.h26x", "w");
	}*/

	//codec_checkin_pts(&codecParam, 0);
}

void decoder_renderer_cleanup() {
	
	//if (captureStream)
	//{
	//	fclose(captureFileHandle);
	//}

	int api = codec_close(&codecParam);
	//printf("codec_close=%x\n", api);

	restore_display();
}

int decoder_renderer_submit_decode_unit(PDECODE_UNIT decodeUnit)
{
	//int entryCount = 0;

	testBufferCount	= 0;
	int result = DR_OK;

	PLENTRY entry = decodeUnit->bufferList;
	while (entry != NULL) 
	{
		//fwrite(entry->data, entry->length, 1, fd);

		//if (testBufferCount + entry->length > TESTBUFFER_LENGTH)
		//{
		//	// Buffer too small
		//	printf("Buffer too small.\n");
		//	break;
		//}
		//else
		//{
		//	memcpy(testBuffer + testBufferCount, entry->data, entry->length);
		//	testBufferCount += entry->length;
		//}
		
		int api = codec_write(&codecParam, entry->data, entry->length);
		if (api != entry->length)
		{
			printf("codec_write error: %x\n", api);
			
			codec_reset(&codecParam);
			result = DR_NEED_IDR;
		}

		entry = entry->next;
		//++entryCount;
	}

	//printf ("entryCount=%d\n", entryCount);
	
	//if (captureStream &&
	//	(captureCount <= CAPTURE_LENGTH))
	//{		
	//	fwrite(testBuffer, testBufferCount, 1, captureFileHandle);
	//	captureCount += testBufferCount;
	//}

	//int api = codec_write(&codecParam, testBuffer, testBufferCount);
	//if (api != testBufferCount)
	//{
	//	printf("codec_write error: %x\n", api);

	//	codec_reset(&codecParam);
	//	result = DR_NEED_IDR;
	//}

	//testBufferCount = 0;

	// int codec_checkin_pts(codec_para_t *pcodec, unsigned long pts);
	//codec_checkin_pts(&codecParam, pts);
	//pts += 96000 / 60;

	return result;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_odroidcx = {
  .setup = decoder_renderer_setup,
  .cleanup = decoder_renderer_cleanup,
  .submitDecodeUnit = decoder_renderer_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SLICES_PER_FRAME(8),	//
};
