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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_DIRENT_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_DIRENT_H_

#include <sprt/gen/c/bits/__sprt_ssize_t.h>
#include <sprt/gen/c/cross/__sprt_fstypes.h>
#include <sprt/gen/c/cross/__sprt_dir_ptr.h>

// The Linux/BSD d_type codes, shared by every platform whose libc uses them.
// A libc that numbers them differently (Embox) defines its own set in its
// cross/<platform>/dir_ptr.h, included just above - hence the #ifndef.
#ifndef __SPRT_DT_UNKNOWN
#define __SPRT_DT_UNKNOWN 0
#define __SPRT_DT_FIFO 1
#define __SPRT_DT_CHR 2
#define __SPRT_DT_DIR 4
#define __SPRT_DT_BLK 6
#define __SPRT_DT_REG 8
#define __SPRT_DT_LNK 10
#define __SPRT_DT_SOCK 12
#define __SPRT_DT_WHT 14
#endif

#ifndef d_fileno
#define d_fileno d_ino
#endif

#include <sprt/gen/c/api/__sprt_dirent.h>

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_DIRENT_H_
