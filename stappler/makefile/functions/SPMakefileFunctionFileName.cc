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

static bool Function_wildcard(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto patterns = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	bool first = true;
	patterns.split<StringView::WhiteSpace>([&](StringView pattern) {
		//sprt::cout << "Pattern: " << pattern << "\n";

		StringView path = pattern.readUntil<StringView::Chars<'*'>>();
		StringView pathSuffix;
		if (pattern.is('*')) {
			++pattern;
			pathSuffix = pattern;
		}

		auto targetPath = engine.getAbsolutePath(path);

		filesystem::ftw(FileInfo{targetPath}, [&](const FileInfo &info, FileType type) {
			if (info.path != targetPath) {
				if (pathSuffix == "/" && type == FileType::Dir) {
					if (first) {
						first = false;
					} else {
						out << ' ';
					}
					out << info.path << "/";
					//sprt::cout << info.path << "/" << "\n";
				} else if (info.path.ends_with(pathSuffix)) {
					if (first) {
						first = false;
					} else {
						out << ' ';
					}
					out << info.path;
					//sprt::cout << info.path << "\n";
				}
			}
			return true;
		}, 1);
	});

	return true; // not implemented
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
				out << StringView(buf);
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
				out << path;
			}
		});
	}

	return true;
}

} // namespace stappler::makefile
