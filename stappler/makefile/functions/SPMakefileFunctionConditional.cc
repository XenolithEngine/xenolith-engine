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

#include "SPBuffer.h"
#include "SPFilepath.h"
#include "SPMakefileVariable.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

static bool Function_if(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto cond = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	cond.trimChars<StringView::WhiteSpace>();

	if (!cond.empty()) {
		engine.resolve(out, args[1], 0, *engine.getCallContext()->err);
	} else if (args.size() > 2) {
		engine.resolve(out, args[2], 0, *engine.getCallContext()->err);
	}

	return true;
}

static bool Function_or(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	BufferTemplate<Interface> buf;
	for (auto &it : args) {
		buf.clear();
		engine.resolve([&](StringView str) { buf.put(str.data(), str.size()); }, it, 0,
				*engine.getCallContext()->err);
		if (!buf.empty()) {
			// return first non-empty value
			out(buf.get());
			return true;
		}
	}
	return true;
}

static bool Function_and(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	BufferTemplate<Interface> buf;
	for (auto &it : args) {
		buf.clear();
		engine.resolve([&](StringView str) { buf.put(str.data(), str.size()); }, it, 0,
				*engine.getCallContext()->err);
		if (buf.empty()) {
			return true;
		}
	}

	// return last non-empty value
	out(buf.get());
	return true;
}

} // namespace stappler::makefile
