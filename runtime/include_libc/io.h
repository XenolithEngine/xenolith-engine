/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_IO_H_
#define CORE_RUNTIME_INCLUDE_LIBC_IO_H_

/*
	Compatibility header for the Windows <io.h> (low-level/descriptor I/O).
	Unlike the other libc shims this has no __SPRT_BUILD dispatch: it always pulls in
	  <sys/stat.h>                     - stat() and the struct stat / mode bits
	  sprt/wrappers/unistd/io.h        - the MSVC <io.h> emulation surface
	  sprt/wrappers/unistd/unistd.h    - the POSIX descriptor-I/O surface
	(see include/sprt/wrappers/unistd/README.adoc)

	Public surface contributed by the <io.h> wrapper (the POSIX surface from the
	unistd wrapper is documented in include_libc/unistd.h):

	Macros / structs:
	  struct _stat (+ _stati64/_fstati64/_wstati64 aliases)
	  _O_BINARY/O_BINARY, _O_TEXT/O_TEXT  - text/binary translation modes
	  S_IFREG, _S_IFMT, _S_IFDIR, _S_IREAD, _S_IWRITE - stat mode bits
	  _MAX_PATH/_MAX_DRIVE/_MAX_DIR/_MAX_FNAME/_MAX_EXT - path-split limits (Win32)
	  _SH_DENYRW/_SH_DENYWR/_SH_DENYRD/_SH_DENYNO/_SH_SECURE - share modes
	  st_atime/st_mtime - timespec field aliases

	Types:
	  off_t, wchar_t (in C), onexit_t

	Functions (MSVC-named):
	  _wopen/_wsopen/_wsopen_s/_sopen/_sopen_s - open by name, wide/shared forms
	  _fullpath/_wfullpath   - resolve an absolute path
	  _wfopen/_fsopen/_wfsopen - fopen variants (wide / share flag)
	  fopen_s/freopen_s/_wfreopen_s - bounds-checked fopen/freopen
	  _stat/_fstat/_wstat    - stat into a struct _stat
	  setmode/_setmode       - set a descriptor's translation mode
	  _lseek/_lseeki64       - reposition a descriptor (32/64-bit)
	  _fseeki64/_ftelli64    - large-file fseek/ftell
	  _fdopen/_fileno        - bridge between descriptors and FILE*
	  _read/_write/_close    - low-level descriptor I/O
	  _unlink                - remove a file
	  _isatty                - terminal test
	  _exit                  - immediate process termination
	  _onexit                - register an exit handler
	  _vsnprintf             - bounded formatted output
	  _chmod                 - change a file's mode bits
*/

// See include/sprt/wrappers/unistd/README.adoc

#include <sys/stat.h>
#include <sprt/wrappers/unistd/io.h>
#include <sprt/wrappers/unistd/unistd.h>

#endif // CORE_RUNTIME_INCLUDE_LIBC_IO_H_
