// Tau module startup.
//
// Root enters the ELF entry with the module's StartInfo in the first argument register
// (x0, rdi) and the stack pointer at the top of its stack; there is no argv, environment
// or auxv. libtau reads the StartInfo first (it needs no TLS for that), then the TLS
// templates -- from libtau for a module root linked with shared libraries, else the
// program's PT_TLS found through the ELF header the image maps at its base -- give a
// block for the main thread, and only then is __libc constructed: errno is
// thread_local. A service thread for the module's rings, when the manifest asks for one,
// starts before any constructor can wait on a ring.
//
// As the shared runtime libsprt.so (SPRT_BUILD_SHARED_RUNTIME, RT-A-SharedRuntime-1) this file
// is in the library: the module's _start (libc_impl/app/tau/crt1.cpp) passes its main, the
// init_array here is the library's own, and libtau runs those of the other libraries and the
// module's after it.

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

// The static TLS block of a thread (SYS-A-StaticTls-1): the templates and where they go from the
// thread pointer, the alignment of the block and how far it reaches (above the thread pointer,
// TCB included, on aarch64; below it on x86_64).
constexpr int TAU_TLS_MAX = 8;
__sprt_tau_tls_image s_images[TAU_TLS_MAX];
int s_imageCount = 0;
__SPRT_ID(size_t) s_tlsAlign = 16;
__SPRT_ID(size_t) s_tlsSpan = 0;

#if __x86_64__
// TLS variant II: the TCB above the thread pointer starts with its own address, read as %fs:0.
constexpr __SPRT_ID(size_t) TAU_TCB_SIZE = 64;

__SPRT_ID(size_t) __tau_tls_bytes() { return s_tlsSpan + TAU_TCB_SIZE; }
#else
__SPRT_ID(size_t) __tau_tls_bytes() { return s_tlsSpan; }
#endif

// The templates from libtau, or the module's own PT_TLS where lld put it: variant I after a TCB of
// 16 bytes on aarch64, variant II below the thread pointer on x86_64.
void __tau_find_tls() {
	s_imageCount = __sprt_tau_tls_images(s_images, TAU_TLS_MAX);
	if (s_imageCount == 0) {
		auto eh = reinterpret_cast<const __tau_ehdr *>(__ehdr_start);
		auto base = reinterpret_cast<const unsigned char *>(eh) + eh->e_phoff;
		for (unsigned i = 0; i < eh->e_phnum; ++i) {
			auto ph = reinterpret_cast<const __tau_phdr *>(base + i * eh->e_phentsize);
			if (ph->p_type == TAU_PT_TLS && ph->p_memsz) {
				unsigned long a = ph->p_align ? ph->p_align : 1;
#if __x86_64__
				long offset = -(long)(ph->p_memsz + ((0 - ph->p_vaddr - ph->p_memsz) & (a - 1)));
#else
				long offset = (long)(16 + ((ph->p_vaddr - 16) & (a - 1)));
#endif
				s_images[0] = {reinterpret_cast<const void *>(ph->p_vaddr), (unsigned long)ph->p_filesz,
					(unsigned long)ph->p_memsz, a, offset};
				s_imageCount = 1;
				break;
			}
		}
	}
	for (int i = 0; i < s_imageCount; ++i) {
		const __sprt_tau_tls_image &t = s_images[i];
		if (!t.memsz) {
			continue;
		}
		s_tlsAlign = t.align > s_tlsAlign ? t.align : s_tlsAlign;
#if __x86_64__
		__SPRT_ID(size_t) reach = (__SPRT_ID(size_t))-t.offset;
#else
		__SPRT_ID(size_t) reach = (__SPRT_ID(size_t))t.offset + t.memsz;
#endif
		s_tlsSpan = reach > s_tlsSpan ? reach : s_tlsSpan;
	}
#if __x86_64__
	// The thread pointer is aligned to the block's alignment, so is every image below it.
	s_tlsSpan = (s_tlsSpan + s_tlsAlign - 1) & ~(s_tlsAlign - 1);
#endif
}

void __tau_copy_tls(unsigned char *tp) {
	for (int i = 0; i < s_imageCount; ++i) {
		if (s_images[i].filesz) {
			__builtin_memcpy(tp + s_images[i].offset, s_images[i].image, s_images[i].filesz);
		}
	}
}

} // namespace

extern "C" __attribute__((weak)) unsigned long __stack_chk_guard = 0;

// A fault, not an exit: root counts it against the module's restart limit, so guessing the
// canary stops the module (RT-A-StackProtector-1, SYS-A-RestartWindow-1).
extern "C" __attribute__((weak)) __SPRT_NORETURN void __stack_chk_fail(void) {
	static const char msg[] = "*** stack smashing detected ***\n";
	__sprt_tau_debug_write(msg, sizeof(msg) - 1);
	__builtin_trap();
}

// The program has thread-local data: a thread needs a block of it.
extern "C" bool __tau_tls_used(void) { return s_tlsSpan != 0; }

#if __x86_64__
// A TLS block for a thread: the images below the thread pointer, then the TCB at it.
extern "C" void *__tau_tls_alloc(void) {
	if (s_tlsSpan == 0) {
		return nullptr;
	}
	void *base = nullptr;
	if (__sprt_tau_mem_map(__tau_tls_bytes(), s_tlsAlign, 0, &base) != 0) {
		return nullptr;
	}
	auto tp = (unsigned char *)base + s_tlsSpan;
	__tau_copy_tls(tp);
	*(void **)tp = tp;
	return tp;
}

extern "C" void __tau_tls_free(void *tp) {
	if (tp) {
		__sprt_tau_mem_unmap((unsigned char *)tp - s_tlsSpan, __tau_tls_bytes());
	}
}
#else
// A TLS block for a thread: the thread pointer at its start, the TCB, then the images.
extern "C" void *__tau_tls_alloc(void) {
	if (s_tlsSpan == 0) {
		return nullptr;
	}
	void *tp = nullptr;
	if (__sprt_tau_mem_map(__tau_tls_bytes(), s_tlsAlign, 0, &tp) != 0) {
		return nullptr;
	}
	__tau_copy_tls((unsigned char *)tp);
	return tp;
}

extern "C" void __tau_tls_free(void *tp) {
	if (tp) {
		__sprt_tau_mem_unmap(tp, __tau_tls_bytes());
	}
}
#endif

extern "C" void __tau_main_stack(__sprt_uintptr_t *low, __sprt_uintptr_t *high) {
	*low = s_process.stack_top - s_process.stack_size;
	*high = s_process.stack_top;
}

extern "C" const char *__tau_module_name() { return s_process.name ? s_process.name : "tau"; }

extern "C" unsigned __tau_module_index() { return s_process.module; }

extern "C" unsigned __tau_module_cpus() { return s_process.cpus ? s_process.cpus : 1; }

extern "C" {
extern void (*__init_array_start[])(int, char **, char **) __attribute__((weak, visibility("hidden")));
extern void (*__init_array_end[])(int, char **, char **) __attribute__((weak, visibility("hidden")));
extern void (*__preinit_array_start[])(int, char **, char **) __attribute__((weak, visibility("hidden")));
extern void (*__preinit_array_end[])(int, char **, char **) __attribute__((weak, visibility("hidden")));
}

alignas(16) static unsigned char s_libcBuffer[sizeof(sprt::__libc)];

sprt::__libc *sprt::__libc::get() { return reinterpret_cast<__libc *>(s_libcBuffer); }

extern "C" __SPRT_ID(uint64_t) __libc_main_thread = 0;

using __tau_main_fn = int (*)(int, char **, char **);

static __SPRT_NORETURN void __tau_start(const void *startInfo, __tau_main_fn entry) {
	if (__sprt_tau_init(startInfo, &s_process) != 0) {
		static const char msg[] = "tau: the StartInfo is not usable\n";
		__sprt_tau_debug_write(msg, sizeof(msg) - 1);
		__sprt_tau_exit(126);
	}
	// Through a copy: __sprt_tau_random checks its own frame against the guard it entered with. The
	// zero byte ends string copies before the rest of the guard.
	unsigned long guard = 0;
	__sprt_tau_random(&guard, sizeof(guard));
	__stack_chk_guard = guard & ~0xffUL;

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

#if SPRT_BUILD_SHARED_RUNTIME
	for (auto f = __init_array_start; f != __init_array_end; ++f) {
		(*f)(1, s_argv, s_envp);
	}
	__sprt_tau_init_libraries(1, s_argv, s_envp, __ehdr_start);
#else
	for (auto f = __preinit_array_start; f != __preinit_array_end; ++f) {
		(*f)(1, s_argv, s_envp);
	}
	__sprt_tau_init_libraries(1, s_argv, s_envp, nullptr);
	for (auto f = __init_array_start; f != __init_array_end; ++f) {
		(*f)(1, s_argv, s_envp);
	}
#endif
	exit(entry ? entry(1, s_argv, s_envp) : 0);
}

#if SPRT_BUILD_SHARED_RUNTIME
// Entered from the module's _start with its main.
extern "C" SPRT_API __SPRT_NORETURN void __tau_start_shared(const void *startInfo, __tau_main_fn entry) {
	__tau_start(startInfo, entry);
}
#else
extern "C" int main(int argc, char **argv, char **envp) __attribute__((weak));

extern "C" __SPRT_NORETURN void __tau_start_main(const void *startInfo) { __tau_start(startInfo, &main ? main : nullptr); }

#if __x86_64__
// Entered with the StartInfo in rdi and rsp at the top of the stack.
__asm__(".text\n"
		".globl _start\n"
		".type _start, @function\n"
		"_start:\n"
		"	xor	%ebp, %ebp\n"
		"	and	$-16, %rsp\n"
		"	call	__tau_start_main\n"
		"	ud2\n"
		".size _start, . - _start\n");
#else
__asm__(".text\n"
		".globl _start\n"
		".type _start, %function\n"
		"_start:\n"
		"	mov	x29, #0\n"
		"	mov	x30, #0\n"
		"	b	__tau_start_main\n"
		".size _start, . - _start\n");
#endif
#endif
