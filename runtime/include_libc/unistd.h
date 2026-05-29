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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_
#define CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_

/*
	Dispatch header for the POSIX <unistd.h>:
	- hosted SPRT build -> forwards to the system <unistd.h> (#include_next)
	- otherwise         -> SPRT's own declarations from two wrappers:
	                         sprt/wrappers/unistd/unistd.h  (POSIX surface)
	                         sprt/wrappers/unistd/io.h      (Windows <io.h> emulation)
	                       (see include/sprt/wrappers/unistd/README.adoc)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	Functions tagged [gate: X] are declared only when __SPRT_CONFIG_HAVE_X is set for
	the target (or when __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS forces all of them);
	untagged functions are always declared.

	=== POSIX wrapper (sprt/wrappers/unistd/unistd.h) ===

	Macros:
	  SEEK_SET/SEEK_CUR/SEEK_END  - lseek whence values
	  R_OK/W_OK/X_OK/F_OK         - access()/faccessat() mode bits
	  AT_FDCWD and the AT_ flag family - for the *at() functions
	  the sysconf SC_ selector family and pathconf PC_ selector family
	    (only those the platform defines)
	  LFS aliases: lseek64, pread64, pwrite64, truncate64, ftruncate64, lockf64

	Types:
	  size_t, ssize_t, off_t, time_t, uid_t, gid_t, pid_t, intptr_t

	Descriptor I/O:
	  access/eaccess  - test file accessibility (real / effective ids)
	  lseek           - reposition a descriptor's offset
	  close           - close a descriptor
	  read/write      - read from / write to a descriptor
	  pread/pwrite    - positioned read/write without moving the offset
	  dup/dup2        - duplicate a descriptor
	  dup3            - duplicate a descriptor with flags  [gate: UNISTD_DUP3]
	  pipe/pipe2      - create a pipe
	  fsync/fdatasync - flush a descriptor to storage
	  sync            - flush all filesystem buffers
	  truncate/ftruncate - set a file's length
	  lockf           - apply or test advisory record locks
	  copy_file_range - copy data between two descriptors in-kernel
	                    [gate: UNISTD_COPY_FILE_RANGE]
	  isatty          - test whether a descriptor is a terminal
	  ttyname/ttyname_r - terminal pathname of a descriptor

	Process control / sleep:
	  sleep           - suspend execution for whole seconds
	  usleep          - suspend execution for microseconds
	  fork/vfork      - create a child process  [gate: UNISTD_FORK]
	  execve/fexecve/execv/execvp/execvpe/execle/execl/execlp
	                  - replace the current process image  [gate: UNISTD_EXEC]
	  nice            - adjust the process scheduling priority  [gate: UNISTD_NICE]
	  getpid          - process id
	  getppid         - parent-process id  [gate: UNISTD_GETPPID]
	  gettid          - calling thread id

	Credentials:
	  getuid/geteuid/getgid/getegid - real and effective user/group ids
	  getresuid/getresgid           - real, effective and saved ids
	  getgroups                     - supplementary group list
	  setuid/seteuid/setreuid/setresuid - set user ids  [gate: UNISTD_SETUIDGID]
	  setgid/setegid/setregid/setresgid - set group ids [gate: UNISTD_SETUIDGID]
	  getlogin/getlogin_r           - controlling-terminal login name
	  setlogin                      - set the login name  [gate: UNISTD_SETLOGIN]

	Filesystem / paths:
	  chdir/fchdir    - change the working directory
	  getcwd          - read the working directory
	  chown           - change file ownership  [gate: UNISTD_CHOWN]
	  fchown          - change ownership by descriptor
	  link/symlink    - create hard / symbolic links
	  readlink        - read a symbolic link's target
	  unlink          - remove a file name
	  rmdir           - remove a directory
	  symlinkat/readlinkat/fchownat/faccessat/linkat/unlinkat
	                  - directory-fd-relative variants of the above

	System info / configuration:
	  pathconf/fpathconf - per-file configurable limits
	  sysconf            - system-wide configurable limits
	  getpagesize        - memory page size
	  getdtablesize      - descriptor-table size limit
	  gethostname        - read the host name
	  sethostname        - set the host name  [gate: UNISTD_SETHOSTNAME]
	  getdomainname/setdomainname - NIS/YP domain name  [gate: UNISTD_DOMAINNAME]
	  gethostid          - numeric host identifier

	Memory / misc:
	  brk/sbrk        - adjust the program break  [gate: UNISTD_BRK]
	  swab            - copy bytes swapping adjacent pairs
	  getentropy      - fill a buffer with high-quality random bytes

	=== Windows <io.h> emulation (sprt/wrappers/unistd/io.h) ===

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

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <unistd.h>

#else

// See include/sprt/wrappers/unistd/README.adoc

#include <sprt/wrappers/unistd/io.h>
#include <sprt/wrappers/unistd/unistd.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_UNISTD_H_
