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

// WebAssembly <termios.h> backend. There is no controlling terminal in the browser
// sandbox, so the query/modify calls fail with ENOTTY and the rest are vacuous.
// Consumers such as OpenSSL's console UI check the return value and fall back.
// Full declarations live in <termios.h>. Included by builtin_termios.cpp on wasm.

#include <termios.h>
#include <errno.h>

extern "C" {

int tcgetattr(int, struct termios *) { errno = ENOTTY; return -1; }
int tcsetattr(int, int, const struct termios *) { errno = ENOTTY; return -1; }
int tcsendbreak(int, int) { errno = ENOTTY; return -1; }
int tcdrain(int) { errno = ENOTTY; return -1; }
int tcflush(int, int) { errno = ENOTTY; return -1; }
int tcflow(int, int) { errno = ENOTTY; return -1; }

speed_t cfgetospeed(const struct termios *__tio) { return __tio ? __tio->__c_ospeed : 0; }
speed_t cfgetispeed(const struct termios *__tio) { return __tio ? __tio->__c_ispeed : 0; }
int cfsetospeed(struct termios *__tio, speed_t __speed) {
	if (__tio) {
		__tio->__c_ospeed = __speed;
	}
	return 0;
}
int cfsetispeed(struct termios *__tio, speed_t __speed) {
	if (__tio) {
		__tio->__c_ispeed = __speed;
	}
	return 0;
}
int cfsetspeed(struct termios *__tio, speed_t __speed) {
	if (__tio) {
		__tio->__c_ispeed = __speed;
		__tio->__c_ospeed = __speed;
	}
	return 0;
}
void cfmakeraw(struct termios *) { }

__sprt_pid_t tcgetsid(int) { errno = ENOTTY; return -1; }

} // extern "C"
