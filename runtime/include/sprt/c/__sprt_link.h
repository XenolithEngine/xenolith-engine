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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_LINK_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_LINK_H_

// The loaded objects of the process, one call of the callback each: what an unwinder needs to find
// the tables of an address (dl_iterate_phdr).

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/gen/c/bits/__sprt_size_t.h>
#include <sprt/gen/c/bits/__sprt_uint16_t.h>
#include <sprt/gen/c/bits/__sprt_uint32_t.h>
#include <sprt/gen/c/bits/__sprt_uint64_t.h>

__SPRT_BEGIN_DECL

typedef struct {
	__SPRT_ID(uint32_t) p_type;
	__SPRT_ID(uint32_t) p_flags;
	__SPRT_ID(uint64_t) p_offset;
	__SPRT_ID(uint64_t) p_vaddr;
	__SPRT_ID(uint64_t) p_paddr;
	__SPRT_ID(uint64_t) p_filesz;
	__SPRT_ID(uint64_t) p_memsz;
	__SPRT_ID(uint64_t) p_align;
} __SPRT_ID(Elf64_Phdr);

struct __SPRT_ID(dl_phdr_info) {
	__SPRT_ID(uint64_t) dlpi_addr;
	const char *dlpi_name;
	const __SPRT_ID(Elf64_Phdr) * dlpi_phdr;
	__SPRT_ID(uint16_t) dlpi_phnum;
	unsigned long long dlpi_adds;
	unsigned long long dlpi_subs;
	__SPRT_ID(size_t) dlpi_tls_modid;
	void *dlpi_tls_data;
};

SPRT_API int __SPRT_ID(dl_iterate_phdr)(
		int (*)(struct __SPRT_ID(dl_phdr_info) *, __SPRT_ID(size_t), void *), void *);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_LINK_H_
