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

// The POSIX environment vector. Defined here (the runtime does not provide a
// getenv/environ backend yet) and populated from the host snapshot in _start, so
// a future getenv/setenv implementation has it ready.
extern "C" char **environ = nullptr;

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

extern "C" void _start(void) {
	// 1. libc singleton (fd 0/1/2, locale, exceptions).
	auto libc = new (s_libcBuffer, sprt::nothrow) sprt::__libc;
	__libc_main_thread = libc->mainThread;

	// 2. C/C++ static constructors.
	__wasm_call_ctors();

	// 3. Attach the main thread.
	__sprt_pthread_self();

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
