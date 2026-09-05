/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef CORE_RUNTIME_INCLUDE_LIBC_TERMIOS_H_
#define CORE_RUNTIME_INCLUDE_LIBC_TERMIOS_H_

/*
	POSIX <termios.h> - terminal I/O interface.

	On platforms without a controlling terminal (notably freestanding wasm) there is
	no TTY to configure, so the tc* functions are implemented as no-op stubs in the
	sprt libc that report failure (return -1, errno = ENOTTY) or succeed vacuously;
	callers such as OpenSSL's UI console fall back gracefully. On Windows there IS a
	terminal, and the tc* functions are backed by the console mode (see
	libc_impl/src/windows/termios.cc) - the subset of the interface a Win32 console can
	express (echo, canonical input, signal generation, output post-processing) maps onto
	SetConsoleMode, and the rest is accepted and stored. The struct layout and constant
	values mirror the Linux/musl asm-generic termbits so that the header, the backends,
	and any musl-provided symbol all agree on the ABI.

	- hosted SPRT build -> forwards to the system <termios.h> (#include_next)
	- otherwise         -> SPRT-own declarations below

	Unlike the rest of the libc surface there is no umbrella here: the declarations
	below are the plain public names, resolved straight out of the sprt libc. That
	limits them to the targets whose libc carries a terminal backend - wasm,
	Windows and Embox EL0 - and leaves the header empty on the hosted targets,
	where an sprt application does not get <termios.h> at all. Adding it there means giving
	__sprt_tc* a wrapper over the platform libc (libc_wrapper/c/common), and a
	conversion for the platforms whose struct termios is not the asm-generic one
	this header describes (notably Darwin: 20 control characters, long flags and
	literal baud values).
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <termios.h>

#else

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/cross/__sprt_sysid.h> // pid_t

#if SPRT_WASM || SPRT_WINDOWS || SPRT_EMBOX_USER

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 32

struct termios {
	tcflag_t c_iflag; // input modes
	tcflag_t c_oflag; // output modes
	tcflag_t c_cflag; // control modes
	tcflag_t c_lflag; // local modes
	cc_t c_line; // line discipline
	cc_t c_cc[NCCS]; // control characters
	speed_t __c_ispeed; // input speed
	speed_t __c_ospeed; // output speed
};

// c_cc indices
#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSWTC    7
#define VSTART   8
#define VSTOP    9
#define VSUSP   10
#define VEOL    11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT  15
#define VEOL2   16

// c_iflag bits
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IUCLC   0001000
#define IXON    0002000
#define IXANY   0004000
#define IXOFF   0010000
#define IMAXBEL 0020000
#define IUTF8   0040000

// c_oflag bits
#define OPOST   0000001
#define OLCUC   0000002
#define ONLCR   0000004
#define OCRNL   0000010
#define ONOCR   0000020
#define ONLRET  0000040
#define OFILL   0000100
#define OFDEL   0000200

// c_cflag baud rates, and the field they occupy. Kept for source compatibility and
// for cfsetspeed round-trips: neither backend drives a serial line, so the value is
// stored and reported back rather than acted on.
#define B0      0000000
#define B50     0000001
#define B75     0000002
#define B110    0000003
#define B134    0000004
#define B150    0000005
#define B200    0000006
#define B300    0000007
#define B600    0000010
#define B1200   0000011
#define B1800   0000012
#define B2400   0000013
#define B4800   0000014
#define B9600   0000015
#define B19200  0000016
#define B38400  0000017

#define CBAUD   0010017

// c_cflag bits
#define CSIZE   0000060
#define CS5     0000000
#define CS6     0000020
#define CS7     0000040
#define CS8     0000060
#define CSTOPB  0000100
#define CREAD   0000200
#define PARENB  0000400
#define PARODD  0001000
#define HUPCL   0002000
#define CLOCAL  0004000

// c_lflag bits
#define ISIG    0000001
#define ICANON  0000002
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define IEXTEN  0100000

// tcsetattr optional actions
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

// tcflush queue selectors
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

// tcflow actions
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

__SPRT_BEGIN_DECL

SPRT_API int tcgetattr(int __fd, struct termios *__tio);
SPRT_API int tcsetattr(int __fd, int __optional_actions, const struct termios *__tio);
SPRT_API int tcsendbreak(int __fd, int __duration);
SPRT_API int tcdrain(int __fd);
SPRT_API int tcflush(int __fd, int __queue_selector);
SPRT_API int tcflow(int __fd, int __action);

SPRT_API speed_t cfgetospeed(const struct termios *__tio);
SPRT_API speed_t cfgetispeed(const struct termios *__tio);
SPRT_API int cfsetospeed(struct termios *__tio, speed_t __speed);
SPRT_API int cfsetispeed(struct termios *__tio, speed_t __speed);
SPRT_API int cfsetspeed(struct termios *__tio, speed_t __speed);
SPRT_API void cfmakeraw(struct termios *__tio);

SPRT_API __SPRT_ID(pid_t) tcgetsid(int __fd);

__SPRT_END_DECL

#endif // SPRT_WASM || SPRT_WINDOWS || SPRT_EMBOX_USER

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_TERMIOS_H_
