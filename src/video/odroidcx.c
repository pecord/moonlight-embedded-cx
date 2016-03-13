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
#include <sys/ioctl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <amcodec/codec.h>

#include <string.h>

//static FILE* fd;
//static const char* fileName = "fake.h264";
static codec_para_t codecParam = { 0 };
const size_t EXTERNAL_PTS = (1);
const size_t SYNC_OUTSIDE = (2);


fbdev_window window;

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


void SetupDisplay()
{
	int ret = -1;

	int fd_fb0 = open("/dev/fb0", O_RDWR);
	printf("file handle: %x\n", fd_fb0);


	struct fb_var_screeninfo info;
	ret = ioctl(fd_fb0, FBIOGET_VSCREENINFO, &info);
	if (ret < 0)
	{
		printf("FBIOGET_VSCREENINFO failed.\n");
		exit(1);
	}


	int width = info.xres;
	int height = info.yres;
	int bpp = info.bits_per_pixel;
	int dataLen = width * height * (bpp / 8);

	printf("screen info: width=%d, height=%d, bpp=%d\n", width, height, bpp);



	// Set the EGL window size
	window.width = width;
	window.height = height;


	// Get the EGL display (fb0)
	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY)
	{
		printf("eglGetDisplay failed.\n");
		exit(1);
	}


	// Initialize EGL
	EGLint major;
	EGLint minor;

	EGLBoolean success = eglInitialize(display, &major, &minor);
	if (success != EGL_TRUE)
	{
		EGLint error = eglGetError();
		printf("eglGetError(): %i (0x%.4x)\n", (int)error, (int)error);
		printf("Failed eglInitialize at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}


	// Display info
	//printf("EGL: Initialized - major=%d minor=%d\n", major, minor);
	//printf("EGL: Vendor=%s\n", eglQueryString(display, EGL_VENDOR));
	//printf("EGL: Version=%s\n", eglQueryString(display, EGL_VERSION));
	//printf("EGL: ClientAPIs=%s\n", eglQueryString(display, EGL_CLIENT_APIS));
	//printf("EGL: Extensions=%s\n\n", eglQueryString(display, EGL_EXTENSIONS));


	// Find a config
	int redSize;
	int greenSize;
	int blueSize;
	int alphaSize;
	int depthSize = 24;
	int stencilSize = 8;

	if (bpp < 32)
	{
		redSize = 5;
		greenSize = 6;
		blueSize = 5;
		alphaSize = 0;
	}
	else
	{
		redSize = 8;
		greenSize = 8;
		blueSize = 8;
		alphaSize = 8;
	}


	EGLint configAttributes[] =
	{
		EGL_RED_SIZE,            redSize,
		EGL_GREEN_SIZE,          greenSize,
		EGL_BLUE_SIZE,           blueSize,
		EGL_ALPHA_SIZE,          alphaSize,

		EGL_DEPTH_SIZE,          depthSize,
		EGL_STENCIL_SIZE,        stencilSize,

		EGL_SURFACE_TYPE,        EGL_WINDOW_BIT ,

		EGL_NONE
	};


	int num_configs;
	success = eglChooseConfig(display, configAttributes, NULL, 0, &num_configs);
	if (success != EGL_TRUE)
	{
		EGLint error = eglGetError();
		printf("eglGetError(): %i (0x%.4x)\n", (int)error, (int)error);
		printf("Failed eglChooseConfig at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}


	//EGLConfig* configs = new EGLConfig[num_configs];
	EGLConfig* configs = (EGLConfig*)malloc(sizeof(EGLConfig) * num_configs);
	success = eglChooseConfig(display, configAttributes, configs, num_configs, &num_configs);
	if (success != EGL_TRUE)
	{
		EGLint error = eglGetError();
		printf("eglGetError(): %i (0x%.4x)\n", (int)error, (int)error);
		printf("Failed eglChooseConfig at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}


	EGLConfig match = 0;

	for (int i = 0; i < num_configs; ++i)
	{
		EGLint configRedSize;
		EGLint configGreenSize;
		EGLint configBlueSize;
		EGLint configAlphaSize;
		EGLint configDepthSize;
		EGLint configStencilSize;

		eglGetConfigAttrib(display, configs[i], EGL_RED_SIZE, &configRedSize);
		eglGetConfigAttrib(display, configs[i], EGL_GREEN_SIZE, &configGreenSize);
		eglGetConfigAttrib(display, configs[i], EGL_BLUE_SIZE, &configBlueSize);
		eglGetConfigAttrib(display, configs[i], EGL_ALPHA_SIZE, &configAlphaSize);
		eglGetConfigAttrib(display, configs[i], EGL_DEPTH_SIZE, &configDepthSize);
		eglGetConfigAttrib(display, configs[i], EGL_STENCIL_SIZE, &configStencilSize);

		if (configRedSize == redSize &&
			configBlueSize == blueSize &&
			configGreenSize == greenSize &&
			configAlphaSize == alphaSize &&
			configDepthSize == depthSize &&
			configStencilSize == stencilSize)
		{
			match = configs[i];
			break;
		}
	}

	//delete[] configs;
	free((void*)configs);


	if (match == 0)
	{
		printf("No eglConfig match found.\n");
		exit(1);
	}

	printf("EGLConfig match found: (%p)\n", match);
	printf("\n");


	EGLint windowAttr[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
		EGL_NONE };

	EGLSurface surface = eglCreateWindowSurface(display, match, (NativeWindowType)&window, windowAttr);

	if (surface == EGL_NO_SURFACE)
	{
		EGLint error = eglGetError();
		printf("eglGetError(): %i (0x%.4x)\n", (int)error, (int)error);
		printf("Failed eglCreateWindowSurface at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}


	// Create a context
	eglBindAPI(EGL_OPENGL_ES_API);

	EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE };

	EGLContext context = eglCreateContext(display, match, EGL_NO_CONTEXT, contextAttributes);
	if (context == EGL_NO_CONTEXT)
	{
		EGLint error = eglGetError();
		printf("eglGetError(): %i (0x%.4x)\n", (int)error, (int)error);
		printf("Failed eglCreateContext at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}

	success = eglMakeCurrent(display, surface, surface, context);
	if (success != EGL_TRUE)
	{
		EGLint error = eglGetError();
		printf("eglGetError(): %i (0x%.4x)\n", (int)error, (int)error);
		printf("Failed eglMakeCurrent at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}


	// Display GLES info
	//printf("GL: Renderer=%s\n", glGetString(GL_RENDERER));
	//printf("GL: Vendor=%s\n", glGetString(GL_VENDOR));
	//printf("GL: Version=%s\n", glGetString(GL_VERSION));
	//printf("GL: Extensions=%s\n\n", glGetString(GL_EXTENSIONS));


	// VSYNC
	success = eglSwapInterval(display, 1);
	if (success != EGL_TRUE)
	{
		EGLint error = eglGetError();
		printf("eglGetError(): %i (0x%.4x)\n", (int)error, (int)error);
		printf("Failed eglSwapInterval at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}


	// Render
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT |
		GL_DEPTH_BUFFER_BIT |
		GL_STENCIL_BUFFER_BIT);

	success = eglSwapBuffers(display, surface);
	if (success != EGL_TRUE)
	{
		EGLint error = eglGetError();
		printf("eglGetError(): %i (0x%.4x)\n", (int)error, (int)error);
		printf("Failed eglSwapBuffers at %s:%i\n", __FILE__, __LINE__);
		exit(1);
	}

}

void decoder_renderer_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
	printf("videoFormat=%x, width=%d, height=%d, redrawRate=%d, context=%p, drFlags=%x\n",
		videoFormat, width, height, redrawRate, context, drFlags);


	SetupDisplay();


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

	struct buf_status videoBufferStatus;

	while(1)
	{
		int ret = codec_get_vbuf_state(&codecParam, &videoBufferStatus);
		if (ret != 0)
		{
			printf("codec_get_vbuf_state error: %x\n", -ret);
			break;
		}
		
		if (videoBufferStatus.data_len < 0x100)
			break;

		sleep(100);
	}

	int api = codec_close(&codecParam);
	//printf("codec_close=%x\n", api);
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
