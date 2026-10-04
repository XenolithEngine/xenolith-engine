// Tau module startup.
//
// Root enters the ELF entry with x0 pointing at the module's StartInfo and sp at the top
// of its stack; there is no argv, environment or auxv. libtau reads the StartInfo first
// (it needs no TLS for that), then the program's PT_TLS image, found through the ELF
// header the image maps at its base, gets a block for the main thread, and only then is
// __libc constructed: errno is thread_local. A service thread for the module's rings,
// when the manifest asks for one, starts before any constructor can wait on a ring.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

extern "C" {
__SPRT_NORETURN void exit(int) __SPRT_NOEXCEPT;
}

#include "../embox_user/environ.cc"
#include "../embox_user/tls_layout.h"

// The linker's symbol for the ELF header, mapped at the image base with the program headers.
extern "C" const unsigned char __ehdr_start[] __attribute__((visibility("hidden")));

namespace {

struct __tau_ehdr {
	unsigned char e_ident[16];
	__SPRT_ID(uint16_t) e_type;
	__SPRT_ID(uint16_t) e_machine;
	__SPRT_ID(uint32_t) e_version;
	__SPRT_ID(uint64_t) e_entry;
	__SPRT_ID(uint64_t) e_phoff;
	__SPRT_ID(uint64_t) e_shoff;
	__SPRT_ID(uint32_t) e_flags;
	__SPRT_ID(uint16_t) e_ehsize;
	__SPRT_ID(uint16_t) e_phentsize;
	__SPRT_ID(uint16_t) e_phnum;
	__SPRT_ID(uint16_t) e_shentsize;
	__SPRT_ID(uint16_t) e_shnum;
	__SPRT_ID(uint16_t) e_shstrndx;
};

struct __tau_phdr {
	__SPRT_ID(uint32_t) p_type;
	__SPRT_ID(uint32_t) p_flags;
	__SPRT_ID(uint64_t) p_offset;
	__SPRT_ID(uint64_t) p_vaddr;
	__SPRT_ID(uint64_t) p_paddr;
	__SPRT_ID(uint64_t) p_filesz;
	__SPRT_ID(uint64_t) p_memsz;
	__SPRT_ID(uint64_t) p_align;
};

static_assert(sizeof(__tau_ehdr) == 64, "Elf64_Ehdr is 64 bytes");
static_assert(sizeof(__tau_phdr) == 56, "Elf64_Phdr is 56 bytes");

constexpr __SPRT_ID(uint32_t) TAU_PT_TLS = 7;
constexpr unsigned long TAU_SERVICE_STACK = 64 * 1'024;

__sprt_tau_process s_process;

struct {
	const void *image;
	__SPRT_ID(size_t) filesz;
	__SPRT_ID(size_t) memsz;
	__SPRT_ID(size_t) align;
} s_tls;

__SPRT_ID(size_t) __tau_tls_bytes() {
	return (__SPRT_ID(size_t))__el0_tls_gap(s_tls.align) + s_tls.memsz;
}

void __tau_find_tls() {
	auto eh = reinterpret_cast<const __tau_ehdr *>(__ehdr_start);
	auto base = reinterpret_cast<const unsigned char *>(eh) + eh->e_phoff;
	for (unsigned i = 0; i < eh->e_phnum; ++i) {
		auto ph = reinterpret_cast<const __tau_phdr *>(base + i * eh->e_phentsize);
		if (ph->p_type == TAU_PT_TLS) {
			s_tls.image = reinterpret_cast<const void *>(ph->p_vaddr);
			s_tls.filesz = (__SPRT_ID(size_t))ph->p_filesz;
			s_tls.memsz = (__SPRT_ID(size_t))ph->p_memsz;
			s_tls.align = (__SPRT_ID(size_t))(ph->p_align > 16 ? ph->p_align : 16);
			return;
		}
	}
}

} // namespace

extern "C" __attribute__((weak)) unsigned long __stack_chk_guard = 0;

extern "C" __attribute__((weak)) __SPRT_NORETURN void __stack_chk_fail(void) {
	static const char msg[] = "*** stack smashing detected ***\n";
	__sprt_tau_debug_write(msg, sizeof(msg) - 1);
	__sprt_tau_exit(127);
}

// A TLS block for a thread: the thread pointer at its start, the TCB, then the image.
extern "C" void *__tau_tls_alloc(void) {
	if (s_tls.memsz == 0) {
		return nullptr;
	}
	void *tp = nullptr;
	if (__sprt_tau_mem_map(__tau_tls_bytes(), s_tls.align, 0, &tp) != 0) {
		return nullptr;
	}
	auto gap = (__SPRT_ID(size_t))__el0_tls_gap(s_tls.align);
	__builtin_memcpy((unsigned char *)tp + gap, s_tls.image, s_tls.filesz);
	return tp;
}

extern "C" void __tau_tls_free(void *tp) {
	if (tp) {
		__sprt_tau_mem_unmap(tp, __tau_tls_bytes());
	}
}

extern "C" void __tau_main_stack(__sprt_uintptr_t *low, __sprt_uintptr_t *high) {
	*low = s_process.stack_top - s_process.stack_size;
	*high = s_process.stack_top;
}

extern "C" const char *__tau_module_name() { return s_process.name ? s_process.name : "tau"; }

extern "C" unsigned __tau_module_index() { return s_process.module; }

extern "C" {
extern void (*__init_array_start[])(int, char **, char **) __attribute__((weak));
extern void (*__init_array_end[])(int, char **, char **) __attribute__((weak));
extern void (*__preinit_array_start[])(int, char **, char **) __attribute__((weak));
extern void (*__preinit_array_end[])(int, char **, char **) __attribute__((weak));
}

alignas(16) static unsigned char s_libcBuffer[sizeof(sprt::__libc)];

sprt::__libc *sprt::__libc::get() { return reinterpret_cast<__libc *>(s_libcBuffer); }

extern "C" __SPRT_ID(uint64_t) __libc_main_thread = 0;

extern "C" int main(int argc, char **argv, char **envp) __attribute__((weak));

extern "C" __SPRT_NORETURN void __tau_start_main(const void *startInfo) {
	if (__sprt_tau_init(startInfo, &s_process) != 0) {
		static const char msg[] = "tau: the StartInfo is not usable\n";
		__sprt_tau_debug_write(msg, sizeof(msg) - 1);
		__sprt_tau_exit(126);
	}
	__sprt_tau_random(&__stack_chk_guard, sizeof(__stack_chk_guard));

	__tau_find_tls();
	void *tp = __tau_tls_alloc();
	if (!tp) {
		static const char msg[] = "tau: no memory for the TLS of the main thread\n";
		__sprt_tau_debug_write(msg, sizeof(msg) - 1);
		__sprt_tau_exit(126);
	}
	__sprt_tau_attach_main(tp);

	if (s_process.service_thread) {
		void *stack = nullptr;
		void *serviceTp = __tau_tls_alloc();
		if (__sprt_tau_mem_map(TAU_SERVICE_STACK, 4'096, __SPRT_TAU_MEM_GUARD, &stack) != 0 || !serviceTp
				|| __sprt_tau_service_start((char *)stack + TAU_SERVICE_STACK, serviceTp) != 0) {
			static const char msg[] = "tau: the service thread did not start\n";
			__sprt_tau_debug_write(msg, sizeof(msg) - 1);
		}
	}

	static char *s_argv[2] = {nullptr, nullptr};
	static char *s_envp[1] = {nullptr};
	s_argv[0] = const_cast<char *>(__tau_module_name());
	environ = s_envp;

	auto libc = new (s_libcBuffer, sprt::nothrow) sprt::__libc;
	__libc_main_thread = libc->mainThread;

	for (auto f = __preinit_array_start; f != __preinit_array_end; ++f) {
		(*f)(1, s_argv, s_envp);
	}
	for (auto f = __init_array_start; f != __init_array_end; ++f) {
		(*f)(1, s_argv, s_envp);
	}
	int ret = 0;
	if (&main) {
		ret = main(1, s_argv, s_envp);
	}
	exit(ret);
}

__asm__(".text\n"
		".globl _start\n"
		".type _start, %function\n"
		"_start:\n"
		"	mov	x29, #0\n"
		"	mov	x30, #0\n"
		"	b	__tau_start_main\n"
		".size _start, . - _start\n");
