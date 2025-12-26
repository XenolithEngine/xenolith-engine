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
			out << ' ';
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

	FILE *fp;
	char buf[1_KiB];

	//std::cout << "CMD: " << cmd.data() << "\n";

	fp = popen(cmd.data(), "r");
	if (fp == NULL) {
		engine.getCallContext()->err->reportError(toString("Failed to run command: '", cmd, '\''));
		return false;
	} else {
		bool start = false;
		while (auto str = fgets(buf, sizeof(buf), fp)) {
			if (!start) {
				start = true;
			} else {
				out << "\n";
			}
			StringView outStr(str);
			outStr.trimChars<StringView::WhiteSpace>();

			out << outStr;
		}
		pclose(fp);
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
	engine.getCallContext()->err->reportError(StringView(str.data(), str.size()), nullptr, nullptr,
			false);
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
	engine.getCallContext()->err->reportWarning(StringView(str.data(), str.size()), nullptr,
			nullptr, false);
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
	engine.getCallContext()->err->reportInfo(StringView(str.data(), str.size()), nullptr, nullptr,
			false);
	return true;
}

static bool Function_eval(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	return true;
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
