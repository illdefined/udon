#define _XOPEN_SOURCE 600

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>

#define LOADAVG "/proc/loadavg"
#define ENGYNOW "/sys/class/power_supply/BAT0/energy_now"
#define ENGYFUL "/sys/class/power_supply/BAT0/energy_full"

static Display *dpy;
static Window root;

/**
 * \brief Issue error message and die
 *
 * \param fmt Format string
 */
static inline void die(const char *restrict fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

/**
 * \brief Read from file beginning
 *
 * This function reads a maximum number of len - 1 bytes from the beginning of
 * the file described by fd into the buffer buf. The buffer will always
 * be zero-terminated if len is larger than zero.
 *
 * \param fd file descriptor
 * \param buf buffer to read into
 * \param len size of buffer
 *
 * \return On success, the number of bytes read is returned.
 * On error, a negative number is returned and errno is set appropriately.
 */
static inline ssize_t cat(int fd, char *buf, size_t len) {
	/* Reject invalid length */
	if (len <= 0) {
		errno = EINVAL;
		return -1;
	}

	ssize_t ret = pread(fd, buf, len - 1, 0);

	/* Zero-terminate buffer */
	if (ret >= 0)
		buf[ret] = '\0';

	return ret;
}

/**
 * \brief Exit handler
 */
static void onexit() {
	/* Reset root window name and close X connection */
	XStoreName(dpy, root, "");
	XCloseDisplay(dpy);
}

/**
 * \brief Signal handler
 *
 * \param sig signal
 */
static void onsignal(int sig) {
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
	/* Connect to X server */
	dpy = XOpenDisplay((char *) 0);
	if (!dpy)
		die("Unable to open display “%s”\n", XDisplayName((char *) 0));

	/* Determine root window */
	root = RootWindow(dpy, DefaultScreen(dpy));

	/* Set exit handler */
	atexit(onexit);

	/* Configure signal handler */
	struct sigaction act = {
		.sa_handler = onsignal,
		.sa_flags = 0,
	};

	sigfillset(&act.sa_mask);

	if (sigaction(SIGHUP,  &act, (struct sigaction *) 0) ||
		sigaction(SIGINT,  &act, (struct sigaction *) 0) ||
		sigaction(SIGTERM, &act, (struct sigaction *) 0))
		die("Unable to set signal handler: %s\n", strerror(errno));

	int loadavg = open(LOADAVG, O_RDONLY);
	if (loadavg < 0)
		die("Failed to open “%s”: %s\n", LOADAVG, strerror(errno));

	int engynow = open(ENGYNOW, O_RDONLY);
	if (engynow < 0)
		die("Failed to open “%s”: %s\n", ENGYNOW, strerror(errno));

	int engyful = open(ENGYFUL, O_RDONLY);
	if (engyful < 0)
		die("Failed to open “%s”: %s\n", ENGYFUL, strerror(errno));

	for (;;) {
		char nam[128];
		char avg[32];
		char now[32];
		char ful[32];

		/* Determine current time */
		time_t tn = time((time_t *) 0);
		struct tm *tm = gmtime(&tn);

		/* Get load average */
		double avg1, avg5, avg15;

		if (cat(loadavg, avg, sizeof avg) < 0)
			die("Failed to read from “%s”: %s\n", LOADAVG, strerror(errno));

		sscanf(avg, "%.2f %.2f %.2f", &avg1, &avg5, &avg15);

		/* Read battery state */
		if (cat(engynow, now, sizeof now) < 0)
			die("Failed to read from “%s”: %s\n", ENGYNOW, strerror(errno));

		if (cat(engyful, ful, sizeof ful) < 0)
			die("Failed to read from “%s”: %s\n", ENGYFUL, strerror(errno));

		/* Format everything properly */
		int ret = snprintf(nam, sizeof nam, "%.2u:%.2u:%.2u  %.2f %.2f %.2f  %.2f",
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec,
			avg1,
			avg5,
			avg15,
			atof(now) / atof(ful));

		/* Zero-terminate string if truncated */
		if (ret >= sizeof nam)
			nam[sizeof nam] = '\0';

		/* Set root window name and flush output buffer */
		XStoreName(dpy, root, nam);
		XFlush(dpy);

		sleep(1);
	}
}
