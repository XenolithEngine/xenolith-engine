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

namespace STAPPLER_VERSIONIZED stappler::makefile {

static bool Function_dir(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool start = false;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			if (!start) {
				start = true;
			} else {
				out << ' ';
			}
			auto d = filepath::root(str);
			if (!d.empty()) {
				out << d;
				if (!d.ends_with("/")) {
					out << '/';
				}
			} else {
				out << '/';
			}
		});
	}
	return true;
}

static bool Function_notdir(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool start = false;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			if (!start) {
				start = true;
			} else {
				out << ' ';
			}
			auto d = filepath::lastComponent(str);
			if (!d.empty()) {
				out << d;
			} else {
				out << '/';
			}
		});
	}
	return true;
}

static bool Function_suffix(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool start = false;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			auto d = filepath::lastExtension(str);
			if (!d.empty()) {
				if (!start) {
					start = true;
				} else {
					out << ' ';
				}
				out << d;
			}
		});
	}
	return true;
}

static bool Function_basename(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool start = false;
	for (auto &arg : args) {
		auto content = engine.resolve(arg, 0, *engine.getCallContext()->err);
		content.split<StringView::WhiteSpace>([&](StringView str) {
			auto d = filepath::lastExtension(str);
			if (!start) {
				start = true;
			} else {
				out << ' ';
			}
			if (!d.empty()) {
				out << str.sub(0, str.size() - d.size());
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
	engine.getCallContext()->err->reportError("Function not implemented");
	return false; // not implemented
}

static bool Function_wildcard(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto patterns = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	bool first = true;
	patterns.split<StringView::WhiteSpace>([&](StringView pattern) {
		//std::cout << "Pattern: " << pattern << "\n";

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
					//std::cout << info.path << "/" << "\n";
				} else if (info.path.ends_with(pathSuffix)) {
					if (first) {
						first = false;
					} else {
						out << ' ';
					}
					out << info.path;
					//std::cout << info.path << "\n";
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
			if (!path.empty()) {
				if (filesystem::exists(FileInfo{path})) {
					if (!start) {
						start = true;
					} else {
						out << ' ';
					}
					out << path;
				}
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
