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

//static FILE* fd;
//static const char* fileName = "fake.h264";
static codec_para_t codecParam = { 0 };
const int EXTERNAL_PTS = (1);
const int SYNC_OUTSIDE = (2);


fbdev_window window;

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
	printf("EGL: Initialized - major=%d minor=%d\n", major, minor);
	printf("EGL: Vendor=%s\n", eglQueryString(display, EGL_VENDOR));
	printf("EGL: Version=%s\n", eglQueryString(display, EGL_VERSION));
	printf("EGL: ClientAPIs=%s\n", eglQueryString(display, EGL_CLIENT_APIS));
	printf("EGL: Extensions=%s\n\n", eglQueryString(display, EGL_EXTENSIONS));


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
	printf("GL: Renderer=%s\n", glGetString(GL_RENDERER));
	printf("GL: Vendor=%s\n", glGetString(GL_VENDOR));
	printf("GL: Version=%s\n", glGetString(GL_VERSION));
	printf("GL: Extensions=%s\n\n", glGetString(GL_EXTENSIONS));


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
	//fd = fopen(fileName, "w");
	SetupDisplay();

	//codec_para_t codecParam = { 0 };
	codecParam.stream_type = STREAM_TYPE_ES_VIDEO;
	codecParam.has_video = 1;
	codecParam.noblock = 0;
	codecParam.video_type = VFORMAT_H264;

	codecParam.am_sysinfo.format = VIDEO_DEC_FORMAT_H264;  ///< video format, such as H264, MPEG2...
	//codecParam.am_sysinfo.width = 1920;   //< video source width
	//codecParam.am_sysinfo.height = 1080;  //< video source height
	//codecParam.am_sysinfo.rate = 96000 / 24;    //< video source frame duration
	//codecParam.am_sysinfo.extra;   //< extra data information of video stream
	//codecParam.am_sysinfo.status;  //< status of video stream
	//codecParam.am_sysinfo.ratio;   //< aspect ratio of video source
	codecParam.am_sysinfo.param = (void *)(EXTERNAL_PTS | SYNC_OUTSIDE);   //< other parameters for video decoder
	//codecParam.am_sysinfo.ratio64;   //< aspect ratio of video source

	int api = codec_init(&codecParam);
	printf("codec_init=%x\n", api);

	if (api != 0)
	{
		printf("codec_init failed.\n");
		exit(1);
	}

}

void decoder_renderer_cleanup() {
	//fclose(fd);
	struct buf_status videoBufferStatus;

	do
	{
		int ret = codec_get_vbuf_state(&codecParam, &videoBufferStatus);
		if (ret != 0)
		{
			printf("codec_get_vbuf_state error: %x\n", -ret);
			break;
		}
	} while (videoBufferStatus.data_len > 0x100);

	int api = codec_close(&codecParam);
	printf("codec_close=%x\n", api);
}

int decoder_renderer_submit_decode_unit(PDECODE_UNIT decodeUnit) {
	PLENTRY entry = decodeUnit->bufferList;
	while (entry != NULL) {
		//fwrite(entry->data, entry->length, 1, fd);
		int api = codec_write(&codecParam, entry->data, entry->length);
		entry = entry->next;
	}
	return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_odroidcx = {
  .setup = decoder_renderer_setup,
  .cleanup = decoder_renderer_cleanup,
  .submitDecodeUnit = decoder_renderer_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT,
};
