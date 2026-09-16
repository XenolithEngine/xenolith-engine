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

/*
	Windows <termios.h> backend, over the Win32 console mode.

	A console is a real terminal, so unlike the wasm stubs these are not vacuous: the
	part of the termios interface a console can express is mapped onto
	Get/SetConsoleMode, and the rest is remembered so that the interface round-trips.

	  c_lflag ECHO   <-> ENABLE_ECHO_INPUT
	  c_lflag ICANON <-> ENABLE_LINE_INPUT       (line editing / cooked input)
	  c_lflag ISIG   <-> ENABLE_PROCESSED_INPUT  (Ctrl-C raises SIGINT)
	  c_oflag OPOST  <-> ENABLE_PROCESSED_OUTPUT

	That subset is exactly what a raw-mode switch needs, which is what the callers who
	reach for termios are after - cfmakeraw() followed by tcsetattr() puts the console
	into character-at-a-time, no-echo, no-Ctrl-C input, and restoring the struct saved
	by the earlier tcgetattr() puts it back.

	Everything else (c_cflag, c_cc, the speeds, and the c_iflag/c_oflag bits with no
	console equivalent) has no effect on the console, but is stored per direction so a
	read after a write returns what was written. Console settings are process-global,
	so the cache is too; concurrent reconfiguration of the same terminal from two
	threads is as unordered here as it is on a POSIX tty.

	Included by builtin_termios.cpp on Windows. Full declarations live in <termios.h>.
*/

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/windows.h>

#include "../../include/__impl_libc.h"
#include "specific.h"

#include <errno.h>
#include <termios.h>

namespace sprt {

// The console-mode bits this backend owns. Every other bit of the mode word (virtual
// terminal input, quick-edit, mouse and window input, line wrapping) belongs to
// whoever set it and is preserved across tcsetattr.
static constexpr DWORD __termios_input_bits =
		ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
static constexpr DWORD __termios_output_bits = ENABLE_PROCESSED_OUTPUT;

// The part of the interface the console cannot represent. Seeded to what a Linux tty
// reports for a freshly opened terminal, then overwritten by whatever tcsetattr is
// given, so that a get/set/get sequence is stable.
struct __termios_shadow {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
	speed_t c_ispeed;
	speed_t c_ospeed;
	bool initialized;
};

static __termios_shadow __termios_shadow_in;
static __termios_shadow __termios_shadow_out;

static void __termios_shadow_reset(__termios_shadow *sh) {
	// Values a Linux tty reports right after open(): cooked input with CR->NL
	// translation and flow control, post-processed output, 8N1 at 38400 baud.
	sh->c_iflag = ICRNL | IXON | BRKINT | IGNPAR;
	sh->c_oflag = ONLCR;
	sh->c_cflag = CS8 | CREAD | CLOCAL;
	sh->c_lflag = ECHOE | ECHOK | IEXTEN;

	for (int i = 0; i < NCCS; ++i) { sh->c_cc[i] = 0; }

	sh->c_cc[VINTR] = 3; // ^C
	sh->c_cc[VQUIT] = 28; // ^\ .
	sh->c_cc[VERASE] = 127; // DEL
	sh->c_cc[VKILL] = 21; // ^U
	sh->c_cc[VEOF] = 4; // ^D
	sh->c_cc[VMIN] = 1;
	sh->c_cc[VTIME] = 0;
	sh->c_cc[VSTART] = 17; // ^Q
	sh->c_cc[VSTOP] = 19; // ^S
	sh->c_cc[VSUSP] = 26; // ^Z
	sh->c_cc[VREPRINT] = 18; // ^R
	sh->c_cc[VDISCARD] = 15; // ^O
	sh->c_cc[VWERASE] = 23; // ^W
	sh->c_cc[VLNEXT] = 22; // ^V

	sh->c_ispeed = B38400;
	sh->c_ospeed = B38400;

	sh->initialized = true;
}

/*
	Resolve a descriptor to its console, and decide which direction it is.

	GetConsoleScreenBufferInfo is the discriminator: it only succeeds on a screen
	buffer, so a handle it accepts is an output console and anything else that answers
	GetConsoleMode is an input console. There is no direct "is this an input handle"
	query, and comparing against GetStdHandle would miss a console opened by other
	means (CONIN$, a duplicated handle).

	Fails with ENOTTY for anything that is not a console, exactly as a POSIX tc*
	function does for a non-terminal descriptor.
*/
static HANDLE __termios_console(int fd, DWORD *mode, bool *isOutput) {
	auto slot = __libc::get()->get_fd_slot(fd);
	if (!slot || !slot->handle) {
		__sprt_errno = EBADF;
		return nullptr;
	}

	auto handle = static_cast<HANDLE>(slot->handle);
	if (!GetConsoleMode(handle, mode)) {
		__sprt_errno = ENOTTY;
		return nullptr;
	}

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	*isOutput = GetConsoleScreenBufferInfo(handle, &csbi) == TRUE;
	return handle;
}

static __termios_shadow *__termios_get_shadow(bool isOutput) {
	auto sh = isOutput ? &__termios_shadow_out : &__termios_shadow_in;
	if (!sh->initialized) {
		__termios_shadow_reset(sh);
	}
	return sh;
}

extern "C" {

int tcgetattr(int fd, struct termios *tio) {
	if (!tio) {
		__sprt_errno = EINVAL;
		return -1;
	}

	DWORD mode = 0;
	bool isOutput = false;
	if (!__termios_console(fd, &mode, &isOutput)) {
		return -1;
	}

	auto sh = __termios_get_shadow(isOutput);

	tio->c_iflag = sh->c_iflag;
	tio->c_oflag = sh->c_oflag;
	tio->c_cflag = sh->c_cflag;
	tio->c_lflag = sh->c_lflag;
	tio->c_line = 0;
	for (int i = 0; i < NCCS; ++i) { tio->c_cc[i] = sh->c_cc[i]; }
	tio->__c_ispeed = sh->c_ispeed;
	tio->__c_ospeed = sh->c_ospeed;

	// The mapped bits come from the live console rather than the shadow: another
	// component (or the user) may have changed the mode behind our back.
	if (isOutput) {
		if (mode & ENABLE_PROCESSED_OUTPUT) {
			tio->c_oflag |= OPOST;
		} else {
			tio->c_oflag &= ~(tcflag_t)OPOST;
		}
	} else {
		tcflag_t lflag = tio->c_lflag & ~(tcflag_t)(ECHO | ICANON | ISIG);
		if (mode & ENABLE_ECHO_INPUT) {
			lflag |= ECHO;
		}
		if (mode & ENABLE_LINE_INPUT) {
			lflag |= ICANON;
		}
		if (mode & ENABLE_PROCESSED_INPUT) {
			lflag |= ISIG;
		}
		tio->c_lflag = lflag;
	}

	return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *tio) {
	if (!tio) {
		__sprt_errno = EINVAL;
		return -1;
	}

	switch (optional_actions) {
	case TCSANOW:
	case TCSADRAIN:
	case TCSAFLUSH: break;
	default: __sprt_errno = EINVAL; return -1;
	}

	DWORD mode = 0;
	bool isOutput = false;
	auto handle = __termios_console(fd, &mode, &isOutput);
	if (!handle) {
		return -1;
	}

	DWORD wanted = 0;
	if (isOutput) {
		if (tio->c_oflag & OPOST) {
			wanted |= ENABLE_PROCESSED_OUTPUT;
		}
		mode = (mode & ~__termios_output_bits) | wanted;
	} else {
		if (tio->c_lflag & ECHO) {
			wanted |= ENABLE_ECHO_INPUT;
		}
		if (tio->c_lflag & ICANON) {
			wanted |= ENABLE_LINE_INPUT;
		}
		if (tio->c_lflag & ISIG) {
			wanted |= ENABLE_PROCESSED_INPUT;
		}
		mode = (mode & ~__termios_input_bits) | wanted;
	}

	if (!SetConsoleMode(handle, mode)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto sh = __termios_get_shadow(isOutput);

	sh->c_iflag = tio->c_iflag;
	sh->c_oflag = tio->c_oflag;
	sh->c_cflag = tio->c_cflag;
	sh->c_lflag = tio->c_lflag;
	for (int i = 0; i < NCCS; ++i) { sh->c_cc[i] = tio->c_cc[i]; }
	sh->c_ispeed = tio->__c_ispeed;
	sh->c_ospeed = tio->__c_ospeed;
	sh->initialized = true;

	return 0;
}

// A console has no serial line to interrupt, so there is nothing to send. POSIX lets
// an implementation ignore the request on a terminal that cannot break.
int tcsendbreak(int fd, int) {
	DWORD mode = 0;
	bool isOutput = false;
	return __termios_console(fd, &mode, &isOutput) ? 0 : -1;
}

// Console writes are not buffered by the driver, so output is already drained by the
// time write() returns. FlushFileBuffers keeps the promise for a redirected handle.
int tcdrain(int fd) {
	DWORD mode = 0;
	bool isOutput = false;
	auto handle = __termios_console(fd, &mode, &isOutput);
	if (!handle) {
		return -1;
	}
	if (isOutput && !FlushFileBuffers(handle)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	return 0;
}

int tcflush(int fd, int queue_selector) {
	DWORD mode = 0;
	bool isOutput = false;
	auto handle = __termios_console(fd, &mode, &isOutput);
	if (!handle) {
		return -1;
	}

	switch (queue_selector) {
	case TCIFLUSH:
	case TCIOFLUSH:
		// Only an input console has a queue to discard; on an output handle the input
		// half of the request is vacuous.
		if (!isOutput && !FlushConsoleInputBuffer(handle)) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
		break;
	case TCOFLUSH: break; // no driver-side output queue to discard
	default: __sprt_errno = EINVAL; return -1;
	}

	return 0;
}

// Suspending and resuming transmission is a serial-line notion with no console
// equivalent; the request is accepted on a terminal and rejected elsewhere.
int tcflow(int fd, int action) {
	switch (action) {
	case TCOOFF:
	case TCOON:
	case TCIOFF:
	case TCION: break;
	default: __sprt_errno = EINVAL; return -1;
	}

	DWORD mode = 0;
	bool isOutput = false;
	return __termios_console(fd, &mode, &isOutput) ? 0 : -1;
}

speed_t cfgetospeed(const struct termios *tio) { return tio ? tio->__c_ospeed : 0; }
speed_t cfgetispeed(const struct termios *tio) { return tio ? tio->__c_ispeed : 0; }

int cfsetospeed(struct termios *tio, speed_t speed) {
	if (tio) {
		tio->__c_ospeed = speed;
	}
	return 0;
}

int cfsetispeed(struct termios *tio, speed_t speed) {
	if (tio) {
		tio->__c_ispeed = speed;
	}
	return 0;
}

int cfsetspeed(struct termios *tio, speed_t speed) {
	if (tio) {
		tio->__c_ispeed = speed;
		tio->__c_ospeed = speed;
	}
	return 0;
}

// The classic BSD raw-mode recipe. Unlike the wasm stub this has to do the real work:
// it is how callers build the struct they then hand to tcsetattr, and the ICANON/ECHO
// /ISIG bits it clears are precisely the ones that reach the console.
void cfmakeraw(struct termios *tio) {
	if (!tio) {
		return;
	}

	tio->c_iflag &= ~(tcflag_t)(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tio->c_oflag &= ~(tcflag_t)OPOST;
	tio->c_lflag &= ~(tcflag_t)(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tio->c_cflag &= ~(tcflag_t)(CSIZE | PARENB);
	tio->c_cflag |= CS8;
	tio->c_cc[VMIN] = 1;
	tio->c_cc[VTIME] = 0;
}

/*
	Session id of the terminal's foreground process group.

	Windows consoles have no session or process groups, and a process is attached to
	exactly one console, so the only meaningful answer for a terminal descriptor is
	this process itself.
*/
__SPRT_ID(pid_t) tcgetsid(int fd) {
	DWORD mode = 0;
	bool isOutput = false;
	if (!__termios_console(fd, &mode, &isOutput)) {
		return -1;
	}
	return static_cast<__SPRT_ID(pid_t)>(GetCurrentProcessId());
}

} // extern "C"

} // namespace sprt
