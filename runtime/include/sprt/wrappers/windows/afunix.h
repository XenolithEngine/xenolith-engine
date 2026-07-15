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

// <afunix.h>: AF_UNIX socket address (sockaddr_un). Used by llvm's raw_socket_stream.

#ifndef SPRT_WRAPPERS_WINDOWS_AFUNIX_H_
#define SPRT_WRAPPERS_WINDOWS_AFUNIX_H_

#include <sprt/c/cross/__sprt_socket.h>

#ifndef AF_UNIX
#define AF_UNIX __SPRT_AF_UNIX
#endif

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX __SPRT_UNIX_PATH_MAX
#endif

typedef struct sockaddr_un {
	unsigned short sun_family;
	char sun_path[UNIX_PATH_MAX];
} SOCKADDR_UN, *PSOCKADDR_UN;

#endif // SPRT_WRAPPERS_WINDOWS_AFUNIX_H_
