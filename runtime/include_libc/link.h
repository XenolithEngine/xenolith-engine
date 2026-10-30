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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_LINK_H_
#define CORE_RUNTIME_INCLUDE_LIBC_LINK_H_

/*
	Dispatch header for <link.h>:
	- hosted SPRT build -> forwards to the system <link.h> (#include_next)
	- otherwise         -> SPRT's own declarations: the 64-bit ELF types an unwinder walks
	                       (ElfW, Elf64_Phdr, the PT_* it reads), struct dl_phdr_info and
	                       dl_iterate_phdr
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <link.h>

#else

#include <sprt/c/__sprt_link.h>

#define ElfW(type) Elf64_##type

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_PHDR 6
#define PT_TLS 7
#define PT_GNU_EH_FRAME 0x6474e550
#define PT_GNU_RELRO 0x6474e552

__SPRT_BEGIN_DECL

typedef __SPRT_ID(uint64_t) Elf64_Addr;
typedef __SPRT_ID(uint64_t) Elf64_Off;
typedef __SPRT_ID(uint16_t) Elf64_Half;
typedef __SPRT_ID(uint32_t) Elf64_Word;
typedef __SPRT_ID(uint64_t) Elf64_Xword;
typedef __SPRT_ID(Elf64_Phdr) Elf64_Phdr;

#define dl_phdr_info __SPRT_ID(dl_phdr_info)

SPRT_UMBRELLA_FUNC
int dl_iterate_phdr(int (*cb)(struct dl_phdr_info *, __SPRT_ID(size_t), void *), void *data) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_dl_iterate_phdr(cb, data);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_LINK_H_
