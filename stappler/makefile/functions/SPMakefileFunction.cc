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
#include "SPMakefileVariable.h"

#include <stdio.h>

namespace STAPPLER_VERSIONIZED stappler::makefile {

static bool Function_foreach(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto callContext = engine.getCallContext();
	auto varName = engine.resolve(args[0], 0, *callContext->err);
	auto list = engine.resolve(args[1], 0, *callContext->err);

	StringView oldValue;
	auto it = callContext->contextVars->find(varName);
	if (it != callContext->contextVars->end()) {
		oldValue = it->second;
		it->second = StringView();
	} else {
		it = callContext->contextVars->emplace(varName, StringView()).first;
	}

	bool first = true;
	list.split<StringView::WhiteSpace>([&](StringView str) {
		if (first) {
			first = false;
		} else {
			auto b = engine.getCurrentBuffer();
			if (b && !b->empty()) {
				char last = *reinterpret_cast<const char *>(b->data() + b->size() - 1);
				if (!isspace(last)) {
					out << ' ';
				}
			} else {
				out << ' ';
			}
		}

		it->second = str;

		engine.resolve(out, args[2], 0, *callContext->err);
	});

	return true;
}

static bool Function_let(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto callContext = engine.getCallContext();
	auto names = engine.resolve(args[0], 0, *callContext->err);
	names.trimChars<StringView::WhiteSpace>();

	auto list = engine.resolve(args[1], 0, *callContext->err);
	list.trimChars<StringView::WhiteSpace>();

	StringView *lastValue = nullptr;

	names.split<StringView::WhiteSpace>([&](StringView name) {
		list.skipChars<StringView::WhiteSpace>();

		auto val = list.readUntil<StringView::WhiteSpace>();

		auto it = callContext->contextVars->find(name);
		if (it == callContext->contextVars->end()) {
			it = callContext->contextVars->emplace(name, val).first;
		} else {
			it->second = val;
		}

		lastValue = &it->second;
	});

	if (!list.empty() && lastValue) {
		*lastValue = StringView(lastValue->data(), (list.data() + list.size()) - lastValue->data());
	}

	engine.resolve(out, args[2], 0, *callContext->err);

	return true;
}

// Runs an arbitrary command via popen(), matching GNU make's $(shell ...). This is
// intentional and only safe under the module's trusted-makefile model (see the note
// on the Makefile class in SPMakefile.h); never evaluate untrusted makefile text.
static bool Function_shell(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	String cmd;
	engine.resolve([&](StringView s) {
		if (s == "\n" || s == "\r" || s == "\r\n") {
			cmd.push_back(' ');
		} else {
			cmd += s.str<Interface>();
		}
	}, args[0], ' ', *engine.getCallContext()->err);

	FILE *fp = popen(cmd.data(), "r");
	if (fp == NULL) {
		engine.getCallContext()->err->reportError(toString("Failed to run command: '", cmd, '\''));
		return false;
	}

	// Read the whole output verbatim. fgets/fread chunk boundaries are NOT line boundaries:
	// a logical line longer than the buffer would otherwise be split and emitted with a
	// spurious separator in the middle (the previous fgets loop did exactly that).
	String result;
	char buf[4_KiB];
	while (size_t n = fread(buf, 1, sizeof(buf), fp)) { result.append(buf, n); }
	pclose(fp);

	// GNU make's $(shell): strip every trailing newline, then convert each remaining
	// (internal) newline -- and CR/LF pair -- into a single space. Leading and internal
	// non-newline whitespace is preserved verbatim.
	StringView sv(result.data(), result.size());
	while (sv.ends_with("\n") || sv.ends_with("\r")) {
		sv = sv.sub(0, sv.size() - 1);
	}
	while (!sv.empty()) {
		auto seg = sv.readUntil<StringView::Chars<'\n'>>();
		if (sv.is('\n')) {
			if (seg.ends_with("\r")) {
				seg = seg.sub(0, seg.size() - 1);
			}
			out << seg;
			out << " ";
			++sv;
		} else {
			out << seg;
		}
	}

	return true;
}

static bool Function_origin(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto name = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	if (auto v = engine.get(name)) {
		out << StringView(getOriginName(v->origin));
	} else {
		out << StringView("undefined");
	}
	return true;
}

static bool Function_flavor(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto name = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	if (auto v = engine.get(name)) {

		switch (v->type) {
		case Variable::Type::String: out << StringView("simple"); break;
		case Variable::Type::Stmt: out << StringView("recursive"); break;
		case Variable::Type::Function: out << StringView("function"); break;
		}
	} else {
		out << StringView("undefined");
	}
	return true;
}

static bool Function_error(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	StringStream str;
	for (auto &it : args) {
		if (!str.empty()) {
			str << " ";
			out << " ";
		}
		engine.resolve([&](StringView s) {
			str << s;
			out << s;
		}, it, 0, *engine.getCallContext()->err);
	}
	engine.getCallContext()->err->reportError(str.weak(), nullptr, nullptr, false);
	return true;
}

static bool Function_warning(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	StringStream str;
	for (auto &it : args) {
		if (!str.empty()) {
			str << " ";
			out << " ";
		}
		engine.resolve([&](StringView s) {
			str << s;
			out << s;
		}, it, 0, *engine.getCallContext()->err);
	}
	engine.getCallContext()->err->reportWarning(str.weak(), nullptr, nullptr, false);
	return true;
}

static bool Function_info(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	StringStream str;
	for (auto &it : args) {
		if (!str.empty()) {
			str << " ";
			out << " ";
		}
		engine.resolve([&](StringView s) {
			str << s;
			out << s;
		}, it, 0, *engine.getCallContext()->err);
	}

	auto result = str.weak();
	engine.getCallContext()->err->reportInfo(result, nullptr, nullptr, false);
	return true;
}

static const FileLocation *Function_getCallSite(VariableEngine &engine, Stmt *stmt,
		ErrorReporter &err) {
	if (stmt->type == StmtType::WordList && stmt->value->isStmt
			&& stmt->value->stmt->type == StmtType::Word) {
		auto firstWord = stmt->value->stmt;
		if (firstWord->value->isStmt && firstWord->value->stmt->type == StmtType::ArgumentList
				&& firstWord->value->stmt->value->isStmt) {
			auto argumentList = firstWord->value->stmt->value;

			auto name = engine.resolve(argumentList->stmt->value, 0, err);
			if (name == "call" && argumentList->next) {
				auto value = engine.resolve(argumentList->next, 0, err);
				if (!value.empty()) {
					if (auto v = engine.get(value)) {
						if (v->type == Variable::Type::Stmt) {
							return &v->stmt->loc;
						}
					}
				}
			}
		}
	}
	return nullptr;
}

static bool Function_eval(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	// Expand the argument, then parse the result as makefile text. Whitespace chaining
	// (' ') keeps multi-token expansions separated, matching how $(shell) joins them.
	auto text = engine.resolve(args[0], ' ', *engine.getCallContext()->err);

	const FileLocation *loc = nullptr;
	for (auto &it : args) {
		if (it->isStmt) {
			loc = Function_getCallSite(engine, it->stmt, *engine.getCallContext()->err); //
			if (loc) {
				break;
			}
		}
	}

	return engine.evalText(text, *engine.getCallContext()->err, loc);
}

static bool Function_print(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	bool first = true;
	if (auto c = engine.getCustomOutput()) {
		for (auto &it : args) {
			if (first) {
				first = false;
			} else {
				(*c) << " ";
			}
			engine.resolve((*c), it, 0, *engine.getCallContext()->err);
		}
	}
	return true;
}

} // namespace stappler::makefile
