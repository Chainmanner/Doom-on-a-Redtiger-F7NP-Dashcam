//
// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).
//
#define KEY_RIGHTARROW	0xae
#define KEY_LEFTARROW	0xac
#define KEY_UPARROW		0xad
#define KEY_DOWNARROW	0xaf
#define KEY_STRAFE_L	0xa0
#define KEY_STRAFE_R	0xa1
#define KEY_USE			0xa2
#define KEY_FIRE		0xa3
#define KEY_ESCAPE		27
#define KEY_ENTER		13
#define KEY_TAB			9
#define KEY_F1			(0x80+0x3b)
#define KEY_F2			(0x80+0x3c)
#define KEY_F3			(0x80+0x3d)
#define KEY_F4			(0x80+0x3e)
#define KEY_F5			(0x80+0x3f)
#define KEY_F6			(0x80+0x40)
#define KEY_F7			(0x80+0x41)
#define KEY_F8			(0x80+0x42)
#define KEY_F9			(0x80+0x43)
#define KEY_F10			(0x80+0x44)
#define KEY_F11			(0x80+0x57)
#define KEY_F12			(0x80+0x58)

#define KEY_BACKSPACE	0x7f
#define KEY_PAUSE	0xff

#define KEY_EQUALS	0x3d
#define KEY_MINUS	0x2d

#define KEY_RSHIFT	(0x80+0x36)
#define KEY_RCTRL	(0x80+0x1d)
#define KEY_RALT	(0x80+0x38)

#define KEY_LALT	KEY_RALT

// new keys:

#define KEY_CAPSLOCK    (0x80+0x3a)
#define KEY_NUMLOCK     (0x80+0x45)
#define KEY_SCRLCK      (0x80+0x46)
#define KEY_PRTSCR      (0x80+0x59)

#define KEY_HOME        (0x80+0x47)
#define KEY_END         (0x80+0x4f)
#define KEY_PGUP        (0x80+0x49)
#define KEY_PGDN        (0x80+0x51)
#define KEY_INS         (0x80+0x52)
#define KEY_DEL         (0x80+0x53)

#define KEYP_0          0
#define KEYP_1          KEY_END
#define KEYP_2          KEY_DOWNARROW
#define KEYP_3          KEY_PGDN
#define KEYP_4          KEY_LEFTARROW
#define KEYP_5          '5'
#define KEYP_6          KEY_RIGHTARROW
#define KEYP_7          KEY_HOME
#define KEYP_8          KEY_UPARROW
#define KEYP_9          KEY_PGUP

#define KEYP_DIVIDE     '/'
#define KEYP_PLUS       '+'
#define KEYP_MINUS      '-'
#define KEYP_MULTIPLY   '*'
#define KEYP_PERIOD     0
#define KEYP_EQUALS     KEY_EQUALS
#define KEYP_ENTER      KEY_ENTER

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

#define KEYQUEUE_SIZE 16

#define MAX_INPUT_DEVS 16
#define INPUT_TYPE_KEYBOARD 0
#define INPUT_TYPE_JOYSTICK 1
#define INPUT_TYPE_MOUSE    2
#define INPUT_TYPE_TOUCH    3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

// Network stuff.
static int socketFD;
struct sockaddr_in serverAddr;

// input stuff
static int numInputFds = 0;
static int inputFds[MAX_INPUT_DEVS];
static bool shiftPressed = false;
static struct pollfd pollfds[MAX_INPUT_DEVS];

// XXX: HACK
// Linux's evdev system doesn't make it feasible to just use
// tolower(key) like the existing conversions did, so we
// use a few ranges of maps here to avoid an obscenely long switch/case
static char evdevKeysToASCII1[10] = {
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'
};
static char evdevKeysToASCII2[12] = {
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']'
};
static char evdevKeysToASCII3[12] = {
	'a', 's', 'd', 'f', 'g', 'h', 'i', 'j', 'k', 'l', ';', '\''
};
static char evdevKeysToASCII4[11] = {
	'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/'
};


static char evdevShiftKeysToASCII1[12] = {
	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '{', '}'
};
static char evdevShiftKeysToASCII2[12] = {
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}'
};
static char evdevShiftKeysToASCII3[12] = {
	'a', 's', 'd', 'f', 'g', 'h', 'i', 'j', 'k', 'l', ':', '"'
};
static char evdevShiftKeysToASCII4[11] = {
	'|', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?'
};

static unsigned char convertToDoomKey(unsigned int key){
	switch (key) {
		case KEY_ENTER:
			key = DOOM_KEY_ENTER;
			break;
		case KEY_ESC:
			key = KEY_ESCAPE;
			break;
		case KEY_LEFT:
			key = KEY_LEFTARROW;
			break;
		case KEY_RIGHT:
			key = KEY_RIGHTARROW;
			break;
		case KEY_UP:
			key = KEY_UPARROW;
			break;
		case KEY_DOWN:
			key = KEY_DOWNARROW;
			break;
		case KEY_LEFTCTRL:
		case KEY_RIGHTCTRL:
			key = KEY_FIRE;
			break;
		case KEY_SPACE:
			key = KEY_USE;
			break;
		case KEY_LEFTSHIFT:
		case KEY_RIGHTSHIFT:
			key = KEY_RSHIFT;
			break;
		case KEY_LEFTALT:
		case KEY_RIGHTALT:
			key = KEY_LALT;
			break;
		case KEY_F2:
			key = DOOM_KEY_F2;
			break;
		case KEY_F3:
			key = DOOM_KEY_F3;
			break;
		case KEY_F4:
			key = DOOM_KEY_F4;
			break;
		case KEY_F5:
			key = DOOM_KEY_F5;
			break;
		case KEY_F6:
			key = DOOM_KEY_F6;
			break;
		case KEY_F7:
			key = DOOM_KEY_F7;
			break;
		case KEY_F8:
			key = DOOM_KEY_F8;
			break;
		case KEY_F9:
			key = DOOM_KEY_F9;
			break;
		case KEY_F10:
			key = DOOM_KEY_F10;
			break;
		case KEY_F11:
			key = DOOM_KEY_F11;
			break;
		case KEY_EQUAL:
			key = KEY_EQUALS;
			break;
		case KEY_MINUS:
			key = DOOM_KEY_MINUS;
			break;
		case KEY_BACKSPACE:
			key = DOOM_KEY_BACKSPACE;
			break;
		case KEY_TAB:
			key = DOOM_KEY_TAB;
			break;

		// sadly, yes, we need to handle every single alphanumeric
		// key here, since evdev doesn't spit out keys in anything
		// remotely resembling ASCII.....
		// though, we can take many shortcuts
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
		case KEY_0:
			if (shiftPressed)
				key = evdevShiftKeysToASCII1[key - KEY_1];
			else
				key = evdevKeysToASCII1[key - KEY_1];
			break;
		case KEY_Q:
		case KEY_W:
		case KEY_E:
		case KEY_R:
		case KEY_T:
		case KEY_Y:
		case KEY_U:
		case KEY_I:
		case KEY_O:
		case KEY_P:
		case KEY_LEFTBRACE:
		case KEY_RIGHTBRACE:
			if (shiftPressed)
				key = evdevShiftKeysToASCII2[key - KEY_Q];
			else
				key = evdevKeysToASCII2[key - KEY_Q];
			break;
		case KEY_A:
		case KEY_S:
		case KEY_D:
		case KEY_F:
		case KEY_G:
		case KEY_H:
		case KEY_J:
		case KEY_K:
		case KEY_L:
		case KEY_SEMICOLON:
		case KEY_APOSTROPHE:
			if (shiftPressed)
				key = evdevShiftKeysToASCII3[key - KEY_A];
			else
				key = evdevKeysToASCII3[key - KEY_A];
			break;
		case KEY_BACKSLASH:
		case KEY_Z:
		case KEY_X:
		case KEY_C:
		case KEY_V:
		case KEY_B:
		case KEY_N:
		case KEY_M:
		case KEY_COMMA:
		case KEY_DOT:
		case KEY_SLASH:
			if (shiftPressed)
				key = evdevShiftKeysToASCII4[key - KEY_BACKSLASH];
			else
				key = evdevKeysToASCII4[key - KEY_BACKSLASH];
			break;
		default:
			key = 0xFF;
			break;
	}

	return key;
}

static void sendKey(int pressed, unsigned char keyCode)
{
	unsigned char key;
	int ret;
	uint8_t buf[6];

	if ((keyCode == KEY_LEFTSHIFT || keyCode == KEY_RIGHTSHIFT) &&
		(pressed == 1 || pressed == 0))
		shiftPressed = pressed;

	// Emergency exit!
	if (keyCode == KEY_G && shiftPressed)
		exit(1);

	key = convertToDoomKey(keyCode);
	if (key == 0xFF)
		return;
	if (pressed != 0 && pressed != 1)
		return;

	printf("Sending %02x, pressed = %02x\n", key, pressed);

	buf[0] = 0xA5;
	buf[1] = 0xA5;
	buf[2] = key;
	buf[3] = (uint8_t)pressed;
	buf[4] = 0x5A;
	buf[5] = 0x5A;
	ret = send(socketFD, buf, 6, 0);
	if (ret < 0)
	{
		printf("- Error sending key: %s -\n", strerror(errno));
	}
}

static void checkKeys() {
	int ret, i;
	bool keepGoing;
	struct input_event ev;

	keepGoing = true;
	while (keepGoing) {
		keepGoing = false; // exit if we haven't gotten anything
		ret = poll(pollfds, numInputFds, 0);  // don't wait at all, just give anything we've got

		if (ret < 0) {
			// borked
			return;
		}

		for (i = 0; i < MAX_INPUT_DEVS; i++) {
			if (pollfds[i].revents & POLLIN) {
				read(inputFds[i], &ev, sizeof(ev));
				keepGoing = true; // we read something, so keep trying
				//addKeyToQueue(ev.value, ev.code);
				sendKey(ev.value, ev.code);
			}
		}
	}

	// we read the entire backlog, get back to doom
	return;
}

#define TEST_KEY(k) (keybits[(k)/8] & (1 << ((k)%8)))
static int isKeyboard(const char *devPath) {
	unsigned long evbits;
	unsigned char keybits[KEY_MAX/8 + 1];
	int fd = open(devPath, O_RDONLY);
	if (fd < 0) {
		perror("Failed to open device");
		return 0;
	}

	evbits = 0;
	if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), &evbits) < 0) {
		close(fd);
		return 0;
	}

	/* Must support EV_KEY */
	if (!(evbits & (1 << EV_KEY))) {
		close(fd);
		return 0;
	}


	memset(keybits, 0, sizeof(keybits));
	if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) < 0) {
		close(fd);
		return 0;
	}

	if (TEST_KEY(KEY_A) && TEST_KEY(KEY_ENTER)) {
		close(fd);
		return 1;  /* looks like a keyboard */
	}

	close(fd);
	return 0;
}

static void checkInputDevs() {
	struct dirent *dp;
	DIR *dir;

	/* check for devices */
	dir = opendir("/dev/input");

	while ((dp = readdir(dir)) != NULL) {
		char fullpath[268]; /* d_name is 256 bytes */
		int fd;

		if (numInputFds >= MAX_INPUT_DEVS)
			printf("Out of room in input devices array");

		printf("checking %s\n", dp->d_name);
		if (strncmp(dp->d_name, "event", 5) != 0)
			continue;

		sprintf(fullpath, "/dev/input/%s", dp->d_name);

		printf("%s is a valid event device\n", fullpath);
		if (!isKeyboard(fullpath)) {
			printf("%s is not a keyboard, moving on\n", fullpath);
			continue;
		}

		fd = open(fullpath, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			// not necessarily fatal
			perror("Failed to open device");
			continue;
		}

		pollfds[numInputFds].fd = fd;
		pollfds[numInputFds].events = POLLIN;
		inputFds[numInputFds++] = fd;
		printf("adding %s keyboard\n", fullpath);
		ioctl(fd, EVIOCGRAB, 1); // grab exclusive access to the device
	}
	closedir(dir);
}

int main(int argc, char** argv)
{
	socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFD < 0)
	{
		printf("- Error opening socket: %s -\n", strerror(errno));
		return 0;
	}
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(1337);
	serverAddr.sin_addr.s_addr = inet_addr("192.168.1.254");
	connect(socketFD, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

	checkInputDevs();
	while (1)
	{
		checkKeys();
	}

	return 0;
}
