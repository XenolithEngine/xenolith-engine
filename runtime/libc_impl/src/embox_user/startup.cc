
// Embox EL0 process startup.
//
// The kernel erets straight to the ELF entry point with SP_EL0 on a SysV startup
// stack (ABI doc section 3.2) and every general register zeroed. So unlike the
// Windows and wasm backends -- which are handed argv by a host -- everything is
// read off that stack, and _start must not touch a register before saving it.
//
// The order in __el0_start_main is the one ABI doc section 3.4 fixes, and it is
// not arbitrary: the stack guard has to be seeded before any function with a
// canary returns, TLS has to be live before the first thread_local access (which
// happens inside __libc's own constructor, through errno), and .init_array has
// to run after the libc exists because static constructors allocate and print.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_mman.h>

#include "../../../core/include/__el0_syscall.h"

extern "C" {
// Declared noreturn here as well as in <stdlib.h>: __el0_start_main is itself
// noreturn, and without this the compiler cannot see that its tail is
// unreachable.
__SPRT_NORETURN void exit(int) __SPRT_NOEXCEPT;
}

// The POSIX environment vector, pointed at the kernel's copy in _start.
extern "C" char **environ = nullptr;

// --- environment: getenv / setenv / unsetenv / putenv ------------------------
//
// `environ` starts pointing INTO THE STARTUP STACK -- the kernel put the strings
// and the pointer table there and they live for the whole process, so nothing is
// copied. The first mutating call migrates the table (not the strings) to a
// heap-owned growable one; only strings this code allocated are tracked, so a
// replace or unset frees ours and leaves the kernel's and putenv()'s callers'
// alone.
//
// Same shape as the wasm backend, which starts from a host snapshot instead --
// after the first assignment to `environ` neither knows where it came from.

static char **s_env_owned = nullptr; // strings we malloc'd, freeable on replace/unset
static __sprt_size_t s_env_owned_n = 0, s_env_owned_cap = 0;
static char **s_env_table = nullptr; // the table we own (for realloc reuse)

// Length of the variable name in a "name" or "name=value" string (up to '=' / NUL).
static __sprt_size_t __env_namelen(const char *s) {
	__sprt_size_t i = 0;
	while (s[i] && s[i] != '=') {
		++i;
	}
	return i;
}

static void __env_track(char *s) {
	if (s_env_owned_n == s_env_owned_cap) {
		__sprt_size_t nc = s_env_owned_cap ? s_env_owned_cap * 2 : 8;
		char **p = (char **)__sprt_realloc(s_env_owned, nc * sizeof(char *));
		if (!p) {
			return; // best-effort: on OOM the string simply isn't tracked (leaks on replace)
		}
		s_env_owned = p;
		s_env_owned_cap = nc;
	}
	s_env_owned[s_env_owned_n++] = s;
}

static void __env_free_if_owned(char *s) {
	for (__sprt_size_t i = 0; i < s_env_owned_n; ++i) {
		if (s_env_owned[i] == s) {
			__sprt_free(s);
			s_env_owned[i] = s_env_owned[--s_env_owned_n];
			return;
		}
	}
}

// Insert or replace the "name=value" string `s` (name length `l`). `owned` marks
// `s` as ours (track + free on later replace); putenv passes false (caller-owned).
static int __env_put(char *s, __sprt_size_t l, bool owned) {
	__sprt_size_t i = 0;
	if (environ) {
		for (char **e = environ; *e; ++e, ++i) {
			if (__builtin_strncmp(*e, s, l) == 0 && (*e)[l] == '=') {
				char *old = *e;
				*e = s;
				__env_free_if_owned(old);
				if (owned) {
					__env_track(s);
				}
				return 0;
			}
		}
	}
	// Append: grow our table (realloc if we already own it, else malloc + copy).
	char **newenv;
	if (environ && environ == s_env_table) {
		newenv = (char **)__sprt_realloc(s_env_table, (i + 2) * sizeof(char *));
	} else {
		newenv = (char **)__sprt_malloc((i + 2) * sizeof(char *));
		if (newenv && environ) {
			__builtin_memcpy(newenv, environ, i * sizeof(char *));
		}
	}
	if (!newenv) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	newenv[i] = s;
	newenv[i + 1] = nullptr;
	environ = newenv;
	s_env_table = newenv;
	if (owned) {
		__env_track(s);
	}
	return 0;
}

extern "C" char *getenv(const char *name) __SPRT_NOEXCEPT {
	if (!name || !*name) {
		return nullptr;
	}
	__sprt_size_t l = __env_namelen(name);
	if (name[l] == '=') {
		return nullptr; // a name containing '=' can never match
	}
	for (char **e = environ; e && *e; ++e) {
		if (__builtin_strncmp(*e, name, l) == 0 && (*e)[l] == '=') {
			return *e + l + 1;
		}
	}
	return nullptr;
}

extern "C" int setenv(const char *var, const char *value, int overwrite) __SPRT_NOEXCEPT {
	__sprt_size_t l = var ? __env_namelen(var) : 0;
	if (!var || l == 0 || var[l] == '=') {
		__sprt_errno = EINVAL; // null / empty name, or a name containing '='
		return -1;
	}
	if (!value) {
		value = "";
	}
	if (!overwrite) {
		for (char **e = environ; e && *e; ++e) {
			if (__builtin_strncmp(*e, var, l) == 0 && (*e)[l] == '=') {
				return 0; // already set, keep it
			}
		}
	}
	__sprt_size_t vl = __builtin_strlen(value);
	char *s = (char *)__sprt_malloc(l + vl + 2);
	if (!s) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	__builtin_memcpy(s, var, l);
	s[l] = '=';
	__builtin_memcpy(s + l + 1, value, vl + 1);
	if (__env_put(s, l, true) != 0) {
		__sprt_free(s);
		return -1;
	}
	return 0;
}

extern "C" int unsetenv(const char *name) __SPRT_NOEXCEPT {
	__sprt_size_t l = name ? __env_namelen(name) : 0;
	if (!name || l == 0 || name[l] == '=') {
		__sprt_errno = EINVAL;
		return -1;
	}
	if (!environ) {
		return 0;
	}
	char **e = environ, **w = environ;
	while (*e) {
		if (__builtin_strncmp(*e, name, l) == 0 && (*e)[l] == '=') {
			__env_free_if_owned(*e);
		} else {
			*w++ = *e;
		}
		++e;
	}
	*w = nullptr;
	return 0;
}

extern "C" int putenv(char *s) __SPRT_NOEXCEPT {
	__sprt_size_t l = s ? __env_namelen(s) : 0;
	if (!s || l == 0) {
		__sprt_errno = EINVAL;
		return -1;
	}
	if (s[l] != '=') {
		return unsetenv(s); // "name" with no '=' removes the variable
	}
	return __env_put(s, l, false); // caller owns `s`; never tracked/freed
}


// --- ELF program headers and the auxiliary vector ---------------------------
//
// Only the pieces this backend reads. There is no <elf.h> in the runtime, and
// pulling one in for four fields would be a dependency for its own sake.

namespace {

struct __el0_phdr {
	__SPRT_ID(uint32_t) p_type;
	__SPRT_ID(uint32_t) p_flags;
	__SPRT_ID(uint64_t) p_offset;
	__SPRT_ID(uint64_t) p_vaddr;
	__SPRT_ID(uint64_t) p_paddr;
	__SPRT_ID(uint64_t) p_filesz;
	__SPRT_ID(uint64_t) p_memsz;
	__SPRT_ID(uint64_t) p_align;
};

static_assert(sizeof(__el0_phdr) == 56, "Elf64_Phdr is 56 bytes");

constexpr __SPRT_ID(uint32_t) EL0_PT_TLS = 7;

// auxv types this backend reads (ABI doc section 3.3). The kernel may omit any
// of the optional ones, so every read has a default.
constexpr unsigned long EL0_AT_NULL = 0;
constexpr unsigned long EL0_AT_PHDR = 3;
constexpr unsigned long EL0_AT_PHENT = 4;
constexpr unsigned long EL0_AT_PHNUM = 5;
constexpr unsigned long EL0_AT_PAGESZ = 6;
constexpr unsigned long EL0_AT_RANDOM = 25;

struct __el0_auxv {
	unsigned long a_type;
	unsigned long a_val;
};

} // namespace

// --- stack protector --------------------------------------------------------
//
// Defined here rather than left to the compiler: the canary has to be seeded
// from AT_RANDOM before any guarded function returns, and _start is the only
// place that can do it. Both symbols are weak so that a build which links a
// toolchain-provided stack protector keeps that one.

extern "C" __attribute__((weak)) unsigned long __stack_chk_guard = 0;

extern "C" __attribute__((weak)) __SPRT_NORETURN void __stack_chk_fail(void) {
	// Deliberately not abort(): a smashed stack means the signal machinery's own
	// frames may be gone. Say so on stderr with one raw write and leave.
	static const char msg[] = "*** stack smashing detected ***\n";
	__el0_write(2, msg, sizeof(msg) - 1);
	__el0_exit_group(127);
}

// --- TLS --------------------------------------------------------------------
//
// aarch64 uses TLS variant I: TPIDR_EL0 points at the thread pointer, the first
// 16 bytes above it are the reserved TCB, and the executable's PT_TLS image
// starts at the next address aligned to the segment's own p_align. That is the
// offset the linker bakes into every :tprel_hi12:/:tprel_lo12_nc: pair, so
// getting it wrong does not fault -- it silently reads the wrong object.
//
// The block is taken straight from mmap(222) rather than from malloc: this runs
// before the allocator exists, and a syscall needs no libc state.

#include "tls_layout.h"

namespace {

__SPRT_ID(size_t) __el0_align_up(__SPRT_ID(size_t) v, __SPRT_ID(size_t) a) {
	return a > 1 ? (v + a - 1) & ~(a - 1) : v;
}

// Returns false only if there is a PT_TLS segment that could not be honoured;
// a program with no thread_local data has none and that is a success.
bool __el0_setup_tls(const __el0_phdr *phdr, unsigned long phnum, unsigned long phent) {
	if (!phdr || phnum == 0 || phent < sizeof(__el0_phdr)) {
		return true; // no program headers handed over: nothing to set up
	}

	const __el0_phdr *tls = nullptr;
	auto base = (const unsigned char *)phdr;
	for (unsigned long i = 0; i < phnum; ++i) {
		auto ph = (const __el0_phdr *)(base + i * phent);
		if (ph->p_type == EL0_PT_TLS) {
			tls = ph;
			break;
		}
	}
	if (!tls) {
		return true;
	}

	auto align = (__SPRT_ID(size_t))(tls->p_align ? tls->p_align : 1);
	// The gap between the thread pointer and the image. This IS the linker's
	// TPREL offset for a variable at offset 0 of the segment, which is why the
	// formula lives in a header a test can compile against (tls_layout.h).
	auto gap = (__SPRT_ID(size_t))__el0_tls_gap(align);
	auto total = gap + (__SPRT_ID(size_t))tls->p_memsz;

	// Over-allocate by `align` so the thread pointer itself can be aligned; the
	// gap arithmetic above only lands correctly if TP is align-aligned.
	auto raw = __el0_mmap(nullptr, total + align, __SPRT_PROT_READ | __SPRT_PROT_WRITE,
			__SPRT_MAP_PRIVATE | __SPRT_MAP_ANONYMOUS, -1, 0);
	if (__el0_is_err(raw)) {
		return false;
	}

	auto tp = (unsigned char *)__el0_align_up((__SPRT_ID(size_t))raw, align);
	__builtin_memcpy(tp + gap, (const void *)tls->p_vaddr, (__SPRT_ID(size_t))tls->p_filesz);
	// mmap hands back zeroed pages, so .tbss beyond p_filesz is already zero --
	// but only for the pages we just took, and the alignment slack at the front
	// belongs to nobody. Clearing explicitly costs nothing and does not depend on
	// that.
	__builtin_memset(tp, 0, __SPRT_EL0_TCB_SIZE);

	__asm__ __volatile__("msr tpidr_el0, %0" : : "r"(tp) : "memory");
	return true;
}

} // namespace

// --- .init_array ------------------------------------------------------------
//
// The static-constructor table the linker emits. Weak so that a link which
// produced none (nothing to construct) still resolves; the two symbols then
// compare equal and the loop does nothing.

extern "C" {
extern void (*__init_array_start[])(int, char **, char **) __attribute__((weak));
extern void (*__init_array_end[])(int, char **, char **) __attribute__((weak));
extern void (*__preinit_array_start[])(int, char **, char **) __attribute__((weak));
extern void (*__preinit_array_end[])(int, char **, char **) __attribute__((weak));
}

// --- __libc singleton -------------------------------------------------------
//
// Static storage, not a heap allocation: __libc::get() is called from places
// that run before any allocator is usable (and from the allocator itself).

alignas(16) static unsigned char s_libcBuffer[sizeof(sprt::__libc)];

sprt::__libc *sprt::__libc::get() { return reinterpret_cast<__libc *>(s_libcBuffer); }

extern "C" __SPRT_ID(uint64_t) __libc_main_thread = 0;

// --- entry ------------------------------------------------------------------

extern "C" int main(int argc, char **argv, char **envp) __attribute__((weak));

extern "C" __SPRT_NORETURN void __el0_start_main(unsigned long *sp) {
	// 1. argc / argv / envp / auxv, straight off the startup stack.
	auto argc = (int)sp[0];
	auto argv = (char **)(sp + 1);
	auto envp = argv + argc + 1; // past the argv NULL terminator
	environ = envp;

	auto env_end = envp;
	while (*env_end) {
		++env_end;
	}
	auto auxv = (const __el0_auxv *)(env_end + 1);

	const __el0_phdr *phdr = nullptr;
	unsigned long phnum = 0;
	unsigned long phent = sizeof(__el0_phdr);
	const unsigned char *random = nullptr;
	for (auto a = auxv; a->a_type != EL0_AT_NULL; ++a) {
		switch (a->a_type) {
		case EL0_AT_PHDR: phdr = (const __el0_phdr *)a->a_val; break;
		case EL0_AT_PHNUM: phnum = a->a_val; break;
		case EL0_AT_PHENT: phent = a->a_val; break;
		case EL0_AT_RANDOM: random = (const unsigned char *)a->a_val; break;
		case EL0_AT_PAGESZ: break; // read for completeness; sysconf answers 4096
		default: break; // every other entry is optional and ignorable
		}
	}

	// 2. Seed the stack canary before anything guarded returns. Without
	// AT_RANDOM there is nothing better than a fixed value -- and a fixed
	// canary is still a canary against an accidental overrun, which is what it
	// catches in practice on a system with no adversary.
	if (random) {
		unsigned long g = 0;
		__builtin_memcpy(&g, random, sizeof(g));
		__stack_chk_guard = g;
	} else {
		__stack_chk_guard = 0x00'0a'ff'0d'de'ad'be'efUL;
	}

	// 3. TLS, before the libc: __libc's constructor touches errno, and errno is
	// a thread_local.
	__el0_setup_tls(phdr, phnum, phent);

	// 4. The libc singleton: fd 0/1/2, the C locale, the fd tables.
	auto libc = new (s_libcBuffer, sprt::nothrow) sprt::__libc;
	__libc_main_thread = libc->mainThread;

	// 5. Static constructors. preinit first, as the ELF ABI requires.
	for (auto f = __preinit_array_start; f != __preinit_array_end; ++f) {
		(*f)(argc, argv, envp);
	}
	for (auto f = __init_array_start; f != __init_array_end; ++f) {
		(*f)(argc, argv, envp);
	}

	// 6. main, then exit -- which drains atexit, flushes stdio and issues
	// exit_group(94). A program with no main links (the runtime is also built as
	// a library) and exits cleanly.
	int ret = 0;
	if (&main) {
		ret = main(argc, argv, envp);
	}
	exit(ret);
}

// The ELF entry point. Written as a top-level asm block rather than a naked
// function so that nothing the compiler emits can run before SP is read: on
// entry x0..x30 are zero (ABI doc section 3.1) and the ONLY input is SP_EL0.
//
// x29/x30 are zeroed to terminate any frame-pointer walk cleanly -- an unwinder
// that reaches here has reached the bottom of the process.
__asm__(".text\n"
		".globl _start\n"
		".type _start, %function\n"
		"_start:\n"
		"	mov	x29, #0\n"
		"	mov	x30, #0\n"
		"	mov	x0, sp\n"
		"	b	__el0_start_main\n"
		".size _start, . - _start\n");
