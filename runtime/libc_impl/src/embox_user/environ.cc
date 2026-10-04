/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
    10|    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

// The environment of an EL0 process without a platform libc (EmboxUser, Tau): `environ`
// and getenv/setenv/unsetenv/putenv over it. Startup points `environ` at the first table.

// The POSIX environment vector.
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
