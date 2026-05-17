//doomgeneric for a bare Linux VirtualTerminal
// Copyright (C) 2025 Techflash
// Based on doomgeneric_sdl.c

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"
#include "i_system.h"

// XXX: HACK
// Linux's input-event-codes.h and doomkeys.h have many collisions.
// Redefine some of doomkeys.h's names here to work around this.
// I could try to redefine Linux's... but that sounds incredibly
// fragile, and is very likely not a good idea.
#undef KEY_TAB
#undef KEY_ENTER
#undef KEY_BACKSPACE
#undef KEY_MINUS
#undef KEY_F1
#undef KEY_F2
#undef KEY_F3
#undef KEY_F4
#undef KEY_F5
#undef KEY_F6
#undef KEY_F7
#undef KEY_F8
#undef KEY_F9
#undef KEY_F10
#undef KEY_F11
#define DOOM_KEY_TAB		9
#define DOOM_KEY_ENTER		13
#define DOOM_KEY_MINUS		0x2d
#define DOOM_KEY_BACKSPACE	0x7f
#define DOOM_KEY_F1		(0x80+0x3b)
#define DOOM_KEY_F2		(0x80+0x3c)
#define DOOM_KEY_F3		(0x80+0x3d)
#define DOOM_KEY_F4		(0x80+0x3e)
#define DOOM_KEY_F5		(0x80+0x3f)
#define DOOM_KEY_F6		(0x80+0x40)
#define DOOM_KEY_F7		(0x80+0x41)
#define DOOM_KEY_F8		(0x80+0x42)
#define DOOM_KEY_F9		(0x80+0x43)
#define DOOM_KEY_F10		(0x80+0x44)
#define DOOM_KEY_F11		(0x80+0x57)
#define DOOM_KEY_F12		(0x80+0x58)


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <linux/fb.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdbool.h>

#define KEYQUEUE_SIZE 16

#define MAX_INPUT_DEVS 16
#define INPUT_TYPE_KEYBOARD 0
#define INPUT_TYPE_JOYSTICK 1
#define INPUT_TYPE_MOUSE    2
#define INPUT_TYPE_TOUCH    3

// timing stuff
static struct timeval startTime;

// framebuffer stuff 
static uint8_t *fbPtr;
static int fbFd;
static unsigned int fbWidth, fbHeight, fbStride, fbBytesPerPixel, fbOffsetX, fbOffsetY;

// Color arrays. Darkest to brightest.
static const uint8_t redValues[] = {
	0x01,
	0xA1, 0xA2, 0xA3, 0xA4, 0x03,
};
static const uint8_t greenValues[] = {
	0x01,
	0xB1, 0xCA, 0xC7, 0xC8
};
static const uint8_t blueValues[] = {
	0x01,
	0xB1, 0xCD, 0xCC, 0x20
};
static const int redDiv = (256 / sizeof(redValues));
static const int greenDiv = (256 / sizeof(greenValues));
static const int blueDiv = (256 / sizeof(blueValues));

static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static pthread_t keyListenerThreadID;
static pthread_attr_t keyListenerThreadAttrs;
static pthread_mutex_t keyEventMutex;

static void* keyListener(void* arg)
{
	printf("---- KEY LISTENER THREAD STARTED ----\n");
	printf("sizeof(uint16_t) = %d\n", sizeof(uint16_t));

	int ret;
	int socketFD;
	struct sockaddr_in serverAddr;
	uint8_t key, pressed;
	uint8_t buf[32];

	socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFD < 0)
	{
		printf("- Error opening socket: %s -\n", strerror(errno));
		goto keyListener_exit;
	}
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(1337);
	serverAddr.sin_addr.s_addr = inet_addr("192.168.1.254");
	ret = bind(socketFD, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (ret < 0)
	{
		printf("- Error binding socket: %s -\n", strerror(errno));
		goto keyListener_exit;
	}

	while (1)
	{
		memset(buf, 0, sizeof(buf));
		ret = recv(socketFD, buf, sizeof(buf), MSG_DONTWAIT);
		if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
		{
			printf("- Error while receiving: %s -\n", strerror(errno));
			continue;
		}
		else if (ret == 6 && buf[0] == 0xA5 && buf[1] == 0xA5 && buf[4] == 0x5A && buf[5] == 0x5A)
		{
#if 0
			printf("- received key code: ", ret);
			for (int i = 0; i < sizeof(buf); i++)
			{
				printf("%02x ", buf[i]);
			}
			printf("-\n");
#endif

			// Keycode received - "key" should be the output of convertToDoomKey()!
			key = buf[2];
			pressed = buf[3];

			if (key == 0xFF) // unknown, don't process it
			{
				printf("- Bad key -\n");
				continue;
			}

			if (pressed > 1 || pressed < 0) // bogus value
			{
				printf("- Bad pressed value: %d -\n", pressed);
				continue;
			}

			uint16_t keyData = ((uint16_t)pressed << 8) | (uint16_t)key;
			//printf("- Key: %04x -\n", keyData);

			pthread_mutex_lock(&keyEventMutex);
			s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
			s_KeyQueueWriteIndex++;
			s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
			pthread_mutex_unlock(&keyEventMutex);
			
		}
		else if (ret > 0)
		{
			printf("- received %d unexpected bytes: ", ret);
			for (int i = 0; i < sizeof(buf); i++)
			{
				printf("%02x ", buf[i]);
			}
			printf("-\n");
		}
	}

keyListener_exit:
	return NULL;
}

void DG_Init() {
	int ret;
	struct fb_var_screeninfo info;
	struct fb_fix_screeninfo finfo;

	//
	// set up the framebuffer
	//
	fbFd = open("/dev/fb0", O_RDWR);
	if (fbFd < 0)
		I_Error("Failed to open /dev/fb0: %s", strerror(errno));

	// get info
	ret = ioctl(fbFd, FBIOGET_VSCREENINFO, &info);
	if (ret != 0)
		I_Error("Failed to get framebuffer info: %s", strerror(errno));

	// get other info (this can optionally fail, since we can guess the stride)
	ret = ioctl(fbFd, FBIOGET_FSCREENINFO, &finfo);
	if (ret != 0) {
		printf("Failed to get framebuffer info: %s", strerror(errno));
		fbStride = fbWidth * fbBytesPerPixel;
	}
	else {
		fbStride = finfo.line_length;
	}

	printf("ioctls = %d, %d\n", FBIOGET_VSCREENINFO, FBIOGET_FSCREENINFO);

	fbWidth = info.xres;
	fbHeight = info.yres;
	fbBytesPerPixel = info.bits_per_pixel / 8;
	printf("fbWidth = %d, fbHeight = %d\n", fbWidth, fbHeight);
	printf("fbBytesPerPixel = %d, info.bits_per_pixel = %d\n", fbBytesPerPixel, info.bits_per_pixel);

	// to center the image on screen
	fbOffsetX = 0;//((fbWidth - DOOMGENERIC_RESX) / 2) * fbBytesPerPixel;
	fbOffsetY = 0;//((fbHeight - DOOMGENERIC_RESY) / 2) * fbStride;
	printf("fbOffsetX = %d, fbOffsetY = %d\n", fbOffsetX, fbOffsetY);

	fbPtr = mmap(NULL, fbStride * fbHeight*2, PROT_READ | PROT_WRITE,
			MAP_SHARED, fbFd, 0);

	if (!fbPtr)
		I_Error("Failed to mmap /dev/fb0: %s", strerror(errno));

	// clear the screen
	memset(fbPtr, 0, fbStride * fbHeight);

	// get the start time
	gettimeofday(&startTime, NULL);

	// Create and start the listener thread.
	pthread_mutex_init(&keyEventMutex, NULL);
	ret = pthread_attr_init(&keyListenerThreadAttrs);
	if (ret != 0)
		I_Error("Failed to create key listener thread attributes: %s", strerror(errno));
	ret = pthread_create(&keyListenerThreadID, &keyListenerThreadAttrs, &keyListener, NULL);
	if (ret != 0)
		I_Error("Failed to create and start key listener thread: %s", strerror(errno));
}

pixel_t getPixel(int x, int line)
{
	return DG_ScreenBuffer[(DOOMGENERIC_RESX * line) + x];
}

void drawPixel(pixel_t pixel, int x, int y)
{
	uint8_t* curDstPixelAddr;
	uint8_t red;
	uint8_t green;
	uint8_t blue;

#ifdef CMAP256
	if (y > fbWidth || x > fbHeight)
		return;
	if (pixel == 0x00)
	{
		pixel = 0x01;
	}
	// Draw on the first canvas.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight] - (x*fbStride) + y;
	*curDstPixelAddr = pixel;
	// Now the second.
	curDstPixelAddr = &fbPtr[fbWidth*(fbHeight*2)] - (x*fbStride) + y;
	*curDstPixelAddr = pixel;
#else
	x *= 2;
	y *= 2;
	if (y > fbWidth || x > fbHeight)
		return;

	// Each game pixel is represented by 4 screen pixels: one black, and three different shades of red, green, and blue.
	// The framebuffer's palette is too limited to arbitrarily select the proper such colors, but we can emulate a screen this way.

	red = (uint8_t)((pixel >> 16) & 0xFF);
	red = (red + (redDiv/2)) / (uint8_t)redDiv;
	if (red > sizeof(redValues)-1) red = sizeof(redValues)-1;
	red = redValues[red];

	green = (uint8_t)((pixel >> 8) & 0xFF);
	green = (green + (greenDiv/2)) / (uint8_t)greenDiv;
	if (green > sizeof(greenValues)-1) green = sizeof(greenValues)-1;
	green = greenValues[green];

	blue = (uint8_t)((pixel >> 0) & 0xFF);
	blue = (blue + (blueDiv/2)) / (uint8_t)blueDiv;
	if (blue > sizeof(blueValues)-1) blue = sizeof(blueValues)-1;
	blue = blueValues[blue];

	// First canvas.
	// Upper left pixel.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight] - (x*fbStride) + y;
	*curDstPixelAddr = red;
	// Upper right pixel.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight] - ((x+1)*fbStride) + y;
	*curDstPixelAddr = green;
	// Lower left pixel.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight] - (x*fbStride) + (y+1);
	*curDstPixelAddr = blue;
	// Lower right pixel.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight] - ((x+1)*fbStride) + (y+1);
	*curDstPixelAddr = 0x01;	// Solid black.

	// Second canvas.
	// Upper left pixel.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight*2] - (x*fbStride) + y;
	*curDstPixelAddr = red;
	// Upper right pixel.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight*2] - ((x+1)*fbStride) + y;
	*curDstPixelAddr = green;
	// Lower left pixel.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight*2] - (x*fbStride) + (y+1);
	*curDstPixelAddr = blue;
	// Lower right pixel.
	curDstPixelAddr = &fbPtr[fbWidth*fbHeight*2] - ((x+1)*fbStride) + (y+1);
	*curDstPixelAddr = 0x01;	// Solid black.
#endif
}

void DG_DrawFrame() {
	pixel_t curPixel;

	// Draw pixel-by-pixel. The Y-coordinate starts at 12 for margin correction.
	for (int line = 12; line < DOOMGENERIC_RESY; line++)
	{
		for (int x = 0; x < DOOMGENERIC_RESX; x++)
		{
			curPixel = getPixel(x, line);
			drawPixel(curPixel, x, (line-12));
		}
	}
}

void DG_SleepMs(uint32_t ms) {
	usleep(ms * 1000);
}

uint32_t DG_GetTicksMs() {
	struct timeval curTime;
	long seconds, usec;

	gettimeofday(&curTime, NULL);
	seconds = curTime.tv_sec - startTime.tv_sec;
	usec = curTime.tv_usec - startTime.tv_usec;

	return (seconds * 1000) + (usec / 1000);
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
	int ret;

	pthread_mutex_lock(&keyEventMutex);
	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
		//key queue is empty

		ret = 0;
	}
	else {
		uint16_t keyData = s_KeyQueue[s_KeyQueueReadIndex];
		s_KeyQueueReadIndex++;
		s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

		*pressed = keyData >> 8;
		*doomKey = keyData & 0xFF;

		ret = 1;
	}
	pthread_mutex_unlock(&keyEventMutex);
	return ret;
}

void DG_SetWindowTitle(const char * title) {
	printf("Window Title: %s\n", title);
}

int main(int argc, char **argv) {
	doomgeneric_Create(argc, argv);

	while (1)
	{
		doomgeneric_Tick();
	}


	return 0;
}
