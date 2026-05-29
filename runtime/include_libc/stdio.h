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

#ifndef CORE_RUNTIME_INCLUDE_C_LIBC_STDIO_H_
#define CORE_RUNTIME_INCLUDE_C_LIBC_STDIO_H_

/*
	Dispatch header for <stdio.h>:
	- hosted SPRT build -> forwards to the system <stdio.h> (#include_next)
	- otherwise         -> SPRT's own declarations via sprt/wrappers/libc/stdio.h
	                       (which in turn routes C++ into sprt::_cstdio, and into
	                       std:: under __SPRT_AS_STD)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	A function tagged [gate: X] is declared only when __SPRT_CONFIG_HAVE_X is set for
	the target (or when __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS forces all of them):

	Macros:  EOF, NULL, _IOFBF/_IOLBF/_IONBF (buffering modes),
	         SEEK_SET/SEEK_CUR/SEEK_END, BUFSIZ, FILENAME_MAX, L_tmpnam, UNGET
	         (each when available), _TRUNCATE, fopen64/freopen64 aliases, and MSVC
	         locale scanf macros _scanf_l/_fscanf_l/_sscanf_l/_vscanf_l/_vfscanf_l/
	         _vsscanf_l

	Types:   FILE

	Streams: stdin, stdout, stderr

	ISO C functions:
	  fopen       - open a named file as a stream
	  freopen     - reopen a stream on a new file/mode
	  fclose      - close a stream
	  remove      - delete a file by name
	  rename      - rename or move a file
	  tmpnam      - generate a unique temporary file name
	  tmpfile     - create an anonymous temporary file
	  feof        - test a stream's end-of-file indicator
	  ferror      - test a stream's error indicator
	  clearerr    - clear a stream's EOF/error indicators
	  perror      - print an errno-based error message
	  fseek       - set the stream position
	  ftell       - report the stream position
	  rewind      - reset the stream position to the start
	  fflush      - flush buffered output
	  setvbuf     - set a stream's buffering mode and buffer
	  setbuf      - set a stream's buffer (simple form)
	  fread       - read blocks of data from a stream
	  fwrite      - write blocks of data to a stream
	  fgetc/getc  - read one character from a stream
	  getchar     - read one character from stdin
	  ungetc      - push a character back onto a stream
	  fputc/putc  - write one character to a stream
	  putchar     - write one character to stdout
	  fgets       - read a line into a buffer
	  fputs       - write a string to a stream
	  puts        - write a string plus newline to stdout
	  printf      - formatted output to stdout
	  fprintf     - formatted output to a stream
	  sprintf     - formatted output to a buffer
	  snprintf    - formatted output to a buffer with size limit
	  vprintf/vfprintf/vsprintf/vsnprintf - va_list forms of the printf family
	  scanf       - formatted input from stdin
	  fscanf      - formatted input from a stream
	  sscanf      - formatted input from a string
	  vscanf/vfscanf/vsscanf - va_list forms of the scanf family

	POSIX / extensions:
	  asprintf/vasprintf  - printf into a newly allocated buffer
	  fmemopen            - open a memory buffer as a stream
	  open_memstream      - stream backed by a growing heap buffer
	                        [gate: STDIO_OPEN_MEMSTREAM]
	  fdopen              - wrap an existing file descriptor as a stream
	  popen               - open a pipe to/from a shell command
	  pclose              - close a popen() pipe
	  fileno              - get the file descriptor behind a stream
	  fseeko/ftello       - fseek/ftell using off_t offsets
	  dprintf/vdprintf    - formatted output to a file descriptor
	  flockfile/ftrylockfile/funlockfile - explicit per-stream locking
	  getc_unlocked/getchar_unlocked/putc_unlocked/putchar_unlocked
	                      - unlocked (non-thread-safe) character I/O
	  getdelim            - read until a delimiter, growing the buffer
	  getline             - read a full line, growing the buffer
	  renameat            - rename relative to directory file descriptors
	  ctermid             - name of the controlling terminal

	Locale-aware extensions:
	  scanf_l/fscanf_l/sscanf_l/vscanf_l/vfscanf_l/vsscanf_l,
	  snprintf_l/vsnprintf_l/asprintf_l/vasprintf_l

	MSVC bounds-checked extensions:
	  sscanf_s   - bounds-checked sscanf
	  _snprintf_s - bounds-checked snprintf
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <stdio.h>

#else

#include <sprt/wrappers/libc/stdio.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_C_LIBC_STDIO_H_
