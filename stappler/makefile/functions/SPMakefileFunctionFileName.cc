/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPMakefileVariable.h"

#include <stdlib.h> // realpath()

namespace STAPPLER_VERSIONIZED stappler::makefile {

// GNU make's $(dir)/$(notdir)/$(suffix)/$(basename) operate purely on the text of each
// word (last '/', last '.') rather than touching the filesystem or normalizing the path,
// so they are implemented here directly instead of via Stappler's filepath helpers
// (which canonicalize and strip the leading '.' of an extension).

// Index of the last occurrence of `c`, or maxOf<size_t>() if there is none.
static size_t fnLastIndexOf(StringView s, char c) {
	for (size_t i = s.size(); i > 0; --i) {
		if (s[i - 1] == c) {
			return i - 1;
		}
	}
	return maxOf<size_t>();
}

// Directory part: everything up to and including the last '/', or "./" when there is none.
static StringView fnDir(StringView w) {
	auto sl = fnLastIndexOf(w, '/');
	return sl == maxOf<size_t>() ? StringView("./") : w.sub(0, sl + 1);
}

// File-within-directory part: everything after the last '/', or the whole word when there
// is none (may be empty, e.g. for "a/" or "/").
static StringView fnNotdir(StringView w) {
	auto sl = fnLastIndexOf(w, '/');
	return sl == maxOf<size_t>() ? w : w.sub(sl + 1);
}

// Index of the '.' that starts the suffix (the last '.' lying in the last path component),
// or maxOf<size_t>() when the word has no suffix.
static size_t fnSuffixDot(StringView w) {
	auto dot = fnLastIndexOf(w, '.');
	if (dot == maxOf<size_t>()) {
		return maxOf<size_t>();
	}
	auto sl = fnLastIndexOf(w, '/');
	if (sl != maxOf<size_t>() && dot < sl) {
		return maxOf<size_t>(); // the only '.' is in the directory part
	}
	return dot;
}

static bool Function_dir(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool first = true;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			if (first) {
				first = false;
			} else {
				out << ' ';
			}
			out << fnDir(str);
		});
	}
	return true;
}

static bool Function_notdir(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool first = true;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			if (first) {
				first = false;
			} else {
				out << ' ';
			}
			out << fnNotdir(str);
		});
	}
	return true;
}

static bool Function_suffix(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool first = true;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			auto dot = fnSuffixDot(str);
			if (dot != maxOf<size_t>()) {
				// words without a suffix produce no output (and no separator)
				if (first) {
					first = false;
				} else {
					out << ' ';
				}
				out << str.sub(dot); // includes the leading '.'
			}
		});
	}
	return true;
}

static bool Function_basename(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool first = true;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			if (first) {
				first = false;
			} else {
				out << ' ';
			}
			auto dot = fnSuffixDot(str);
			if (dot != maxOf<size_t>()) {
				out << str.sub(0, dot); // drop the suffix, including the '.'
			} else {
				out << str;
			}
		});
	}

	return true;
}

static bool Function_addsuffix(const Callback<void(StringView)> &out, void *,
		VariableEngine &engine, SpanView<StmtValue *> args) {
	auto suffix = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	auto input = engine.resolve(args[1], 0, *engine.getCallContext()->err);

	bool first = true;
	input.split<StringView::WhiteSpace>([&](StringView word) {
		if (first) {
			first = false;
		} else {
			out << ' ';
		}
		out << word << suffix;
	});
	return true;
}

static bool Function_addprefix(const Callback<void(StringView)> &out, void *,
		VariableEngine &engine, SpanView<StmtValue *> args) {
	auto prefix = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	auto input = engine.resolve(args[1], 0, *engine.getCallContext()->err);

	bool first = true;
	input.split<StringView::WhiteSpace>([&](StringView word) {
		if (first) {
			first = false;
		} else {
			out << ' ';
		}
		out << prefix << word;
	});
	return true;
}

static bool Function_join(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto l1 = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	auto l2 = engine.resolve(args[1], 0, *engine.getCallContext()->err);

	Vector<StringView> a;
	Vector<StringView> b;
	l1.split<StringView::WhiteSpace>([&](StringView s) { a.emplace_back(s); });
	l2.split<StringView::WhiteSpace>([&](StringView s) { b.emplace_back(s); });

	// concatenate element-wise; surplus elements of the longer list pass through unchanged
	auto count = a.size() > b.size() ? a.size() : b.size();
	bool first = true;
	for (size_t i = 0; i < count; ++i) {
		if (first) {
			first = false;
		} else {
			out << ' ';
		}
		if (i < a.size()) {
			out << a[i];
		}
		if (i < b.size()) {
			out << b[i];
		}
	}
	return true;
}

// Shell-style glob match for a single path component: '*' matches any run (including empty),
// '?' matches one character, everything else (including '.') is literal. Iterative with
// backtracking on '*', so patterns like `*.*` or `xl_*_shadow.*` work — not just a single '*'.
static bool Function_wildcard_match(StringView pat, StringView str) {
	size_t p = 0, s = 0;
	size_t starP = maxOf<size_t>(), starS = 0;
	while (s < str.size()) {
		if (p < pat.size() && (pat[p] == '?' || pat[p] == str[s])) {
			++p;
			++s;
		} else if (p < pat.size() && pat[p] == '*') {
			starP = p++;
			starS = s;
		} else if (starP != maxOf<size_t>()) {
			p = starP + 1;
			s = ++starS;
		} else {
			return false;
		}
	}
	while (p < pat.size() && pat[p] == '*') { ++p; }
	return p == pat.size();
}

// Emit a resolved filesystem path in the form GNU make uses on each platform. On POSIX the path is
// emitted verbatim. On Windows, GNU make reports resolved paths as Windows paths but with '/' as the
// separator (e.g. `C:/dir/file`); our filesystem layer yields the internal posix form (`/c/dir`), so
// rewrite the drive prefix (`/c` -> `C:`) and flip any backslashes to forward slashes. This is the
// emit choke-point for the filesystem-resolving functions ($(wildcard)/$(realpath)/$(abspath)); the
// text-slicing functions ($(dir)/$(notdir)/...) intentionally pass their input through unchanged,
// exactly as GNU make does.
static void emitResolvedPath(const Callback<void(StringView)> &out, StringView path) {
#if SPRT_WINDOWS
	auto isDriveLetter = [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	};
	auto upper = [](char c) { return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c; };

	auto s = path.str<Interface>(); // mutable, same-length rewrite (kept alive across the emit)
	if (!s.empty()) {
		char *d = s.data();
		size_t n = s.size();
		// internal posix drive form `/c[/...]` -> `C:[/...]` (same length: "/c" -> "C:")
		if (n >= 2 && d[0] == '/' && isDriveLetter(d[1]) && (n == 2 || d[2] == '/')) {
			d[0] = upper(d[1]);
			d[1] = ':';
		}
		for (size_t i = 0; i < n; ++i) {
			if (d[i] == '\\') {
				d[i] = '/';
			} else if (d[i] == ' ') {
				// A space discovered on disk must survive the engine's whitespace word-splitting:
				// encode it to PathSpacePlaceholder (same-length, in-place). Decoded at OS boundaries.
				d[i] = PathSpacePlaceholder;
			}
		}
		// uppercase a native drive letter (e.g. from a path that was already `c:/...`)
		if (n >= 2 && isDriveLetter(d[0]) && d[1] == ':') {
			d[0] = upper(d[0]);
		}
	}
	out << StringView(s);
#else
	// Encode any space discovered on disk to PathSpacePlaceholder so the path stays one make word.
	memory::StandartInterface::StringType spaceStorage;
	out << encodePathSpaces(path, spaceStorage);
#endif
}

static bool Function_wildcard(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto patterns = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	bool first = true;
	patterns.split<StringView::WhiteSpace>([&](StringView pattern) {
		// A trailing '/' restricts the match to directories (e.g. `dir/*/`).
		bool wantDir = pattern.ends_with("/");
		StringView pat = wantDir ? pattern.sub(0, pattern.size() - 1) : pattern;

		// Split into the directory prefix (up to and including the last '/') and the filename
		// glob applied to each entry. The glob is the last path component; a glob inside the
		// directory part (e.g. `a/*/b`) is not expanded, matching the build's usage.
		size_t sl = pat.size();
		while (sl > 0 && pat[sl - 1] != '/') { --sl; }
		StringView dirPart = pat.sub(0, sl);
		StringView glob = pat.sub(sl);

		auto targetPath = engine.getAbsolutePath(dirPart.empty() ? StringView(".") : dirPart);
		if (targetPath.empty()) {
			return;
		}

		filesystem::ftw(FileInfo{targetPath}, [&](const FileInfo &info, FileType type) {
			if (info.path != targetPath) {
				if (wantDir && type != FileType::Dir) {
					return true;
				}
				if (Function_wildcard_match(glob, filepath::lastComponent(info.path))) {
					if (first) {
						first = false;
					} else {
						out << ' ';
					}
					emitResolvedPath(out, info.path);
					if (wantDir) {
						out << "/";
					}
				}
			}
			return true;
		}, 1);
	});

	return true;
}

static bool Function_realpath(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool start = false;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			auto path = engine.getAbsolutePath(str);
			if (path.empty()) {
				return;
			}
			// GNU make's $(realpath) canonicalizes AND resolves symlinks, returning the
			// empty string for a path that does not exist. The OS realpath() does exactly
			// this (and returns nullptr for a missing path).
			auto cpath = path.str<Interface>();
			char buf[4_KiB] = {0}; // realpath needs a PATH_MAX-sized buffer
			if (::realpath(cpath.data(), buf)) {
				if (!start) {
					start = true;
				} else {
					out << ' ';
				}
				emitResolvedPath(out, StringView(buf));
			}
		});
	}
	return true;
}

static bool Function_abspath(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool start = false;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);

		content.split<StringView::WhiteSpace>([&](StringView str) {
			auto path = engine.getAbsolutePath(str);
			if (!path.empty()) {
				if (!start) {
					start = true;
				} else {
					out << ' ';
				}
				emitResolvedPath(out, path);
			}
		});
	}

	return true;
}

} // namespace stappler::makefile
