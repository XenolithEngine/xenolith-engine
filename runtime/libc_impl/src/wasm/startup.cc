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

// WebAssembly process startup (wasm-port-draft.adoc §3.7).
//
// Replaces the MSVC mainCRTStartup / `.CRT$X*` machinery. The module exports
// `_start`; the launch protocol is:
//   1. placement-new the singleton __libc into a static buffer (__libc::get()),
//      which wires fd 0/1/2, the C locale and the exception state;
//   2. run C++ static constructors via the linker-emitted __wasm_call_ctors
//      (the wasm-native equivalent of __initterm over the CRT init arrays; the
//      C-before-C++ ordering is preserved by the linker's init_array layout);
//   3. attach the main thread to the pthread layer;
//   4. materialise argv/env from the host (args_copy / environ_copy imports),
//      WASI-style — a snapshot taken once at start;
//   5. call main() and exit() (which drains atexit and calls proc_exit).

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_errno.h>

extern "C" {

// Emitted by wasm-ld: runs the C/C++ static constructors (.init_array).
void __wasm_call_ctors(void);

// The application entry point. For wasm, clang renames a user `int main(...)` to
// one of these ABI symbols by signature: `int main(int, char**)` -> __main_argc_argv,
// `int main(void)` -> __main_void. Declared weak so whichever the program actually
// defines is called (the other resolves to null and is skipped).
int __main_argc_argv(int argc, char **argv) __attribute__((weak));
int __main_void(void) __attribute__((weak));

// argv/env snapshot imports (T1 SYNC). Sizes are packed as (count<<16 | bytes),
// matching the WASI args_sizes_get/environ_sizes_get split; the *_copy calls
// serialise the pointer table and the string block into wasm memory.
__attribute__((import_module("sprt"), import_name("args_sizes"))) int __sprt_host_args_sizes(void);
__attribute__((import_module("sprt"), import_name("args_copy"))) int __sprt_host_args_copy(
		char **argv, char *buf);
__attribute__((import_module("sprt"),
		import_name("environ_sizes"))) int __sprt_host_environ_sizes(void);
__attribute__((import_module("sprt"), import_name("environ_copy"))) int __sprt_host_environ_copy(
		char **envp, char *buf);

void exit(int) __SPRT_NOEXCEPT;
}

// The POSIX environment vector, populated from the host snapshot in _start
// (environ_copy import) and mutated in-process by the getenv/setenv family below.
extern "C" char **environ = nullptr;

// --- environment: getenv / setenv / unsetenv / putenv --------------------------
// `environ` starts as the host snapshot: one malloc holding the pointer table and
// the string block. Mutating calls migrate it to a wasm-owned growable table; only
// strings we allocated (via setenv) are tracked so a replace/unset can free them —
// host-snapshot strings and putenv() caller strings are left untouched.

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

// __libc singleton storage. Aligned for the mutexes/atomics it embeds.
alignas(16) static unsigned char s_libcBuffer[sizeof(sprt::__libc)];

sprt::__libc *sprt::__libc::get() { return reinterpret_cast<__libc *>(s_libcBuffer); }

extern "C" __sprt_uint64_t __libc_main_thread = 0;

namespace sprt {

// Build a NULL-terminated pointer table + string block for a (count, bytes)
// host vector. Returns the pointer table (owned, never freed — it lives for the
// whole process) or nullptr on failure/empty.
static char **__wasm_load_vector(int packed,
		int (*copy)(char **, char *), int *out_count) {
	int count = (packed >> 16) & 0xFFFF;
	int bytes = packed & 0xFFFF;
	if (out_count) {
		*out_count = count;
	}
	if (count <= 0) {
		return nullptr;
	}
	// One block: (count + 1) pointers followed by the string bytes.
	__sprt_size_t tableBytes = (__sprt_size_t)(count + 1) * sizeof(char *);
	char *block = (char *)__sprt_malloc(tableBytes + (__sprt_size_t)bytes);
	if (!block) {
		if (out_count) {
			*out_count = 0;
		}
		return nullptr;
	}
	char **table = (char **)block;
	char *strings = block + tableBytes;
	copy(table, strings);
	table[count] = nullptr;
	return table;
}

} // namespace sprt

extern "C" void __sprt_wasm_reinit_main_thread(void);
extern "C" void _start(void) {
	// 1. libc singleton (fd 0/1/2, locale, exceptions).
	auto libc = new (s_libcBuffer, sprt::nothrow) sprt::__libc;
	__libc_main_thread = libc->mainThread;

	// 2. C/C++ static constructors.
	__wasm_call_ctors();

	// 3. Attach the main thread. A ctor above may have attached it early (its first
	// malloc -> mimalloc thread-init -> pthread_setspecific -> self()), before the
	// pthread subsystem's s_handlePool global was constructed — whose construction then
	// clobbered the main thread's registration (see __sprt_wasm_reinit_main_thread). Now
	// that all ctors have run, (re)register the main thread cleanly.
	__sprt_wasm_reinit_main_thread();

	// 4. argv / env snapshot from the host.
	int argc = 0;
	char **argv = sprt::__wasm_load_vector(__sprt_host_args_sizes(), __sprt_host_args_copy, &argc);

	int envc = 0;
	environ = sprt::__wasm_load_vector(__sprt_host_environ_sizes(), __sprt_host_environ_copy,
			&envc);

	// 5. main + exit. Dispatch to whichever main variant the program defined.
	int ret = 0;
	if (&__main_argc_argv) {
		ret = __main_argc_argv(argc, argv);
	} else if (&__main_void) {
		ret = __main_void();
	}
	exit(ret);
}
