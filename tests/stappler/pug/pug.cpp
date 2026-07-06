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

#include "SPCommon.h"
#include "SPMemInterface.h"
#include "SPDataEncode.h"
#include "SPPugTemplate.h"
#include "SPPugContext.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

// Serializes NodeStream events into a flat string for exact comparison:
//   +tag attr=value attr='string' "text" -tag
struct RecordingStream : pug::NodeStream {
	pug::StringStream out;
	pug::Vector<pug::String> stack;
	pug::StringStream errors;

	virtual bool pushNode(StringView tag) override {
		out << "+" << tag << " ";
		stack.emplace_back(tag.str<memory::PoolInterface>());
		return true;
	}

	virtual bool popNode() override {
		if (stack.empty()) {
			out << "-UNBALANCED ";
			return false;
		}
		out << "-" << stack.back() << " ";
		stack.pop_back();
		return true;
	}

	virtual bool setAttribute(StringView name, const pug::Value &value, bool) override {
		out << name << "=";
		if (value.isString()) {
			out << "'" << value.getString() << "'";
		} else {
			out << data::toString(value);
		}
		out << " ";
		return true;
	}

	virtual bool pushString(StringView str) override {
		out << "\"" << str << "\" ";
		return true;
	}

	virtual void onError(StringView err) override { errors << err << "\n"; }
};

static void runNodesCase(StringView name, StringView source, StringView expected,
		const Callback<void(pug::Context &)> &populate = nullptr,
		const Callback<void(pug::Context &, RecordingStream &)> &setup = nullptr) {
	auto tpl = pug::Template::read(source, pug::Template::Options::getNodes(),
			[&](StringView err) { sprt::cout << "    [pug:read] " << err << "\n"; });
	if (!tpl) {
		check(false, name);
		return;
	}

	RecordingStream stream;
	pug::Context ctx;
	ctx.loadDefaults();
	if (populate) {
		populate(ctx);
	}
	if (setup) {
		setup(ctx, stream);
	}

	if (!tpl->run(ctx, stream)) {
		sprt::cout << "    [pug:run] failed: " << stream.errors.str() << "\n";
		check(false, name);
		return;
	}

	checkEq(stream.out.str(), expected, name);
}

} // namespace

void performPugTests() {
	using namespace stappler::mem_pool;

	sprt::cout << "\n== stappler pug tests ==\n";

	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	memory::perform([&] {
		// typed const attributes, id/class notes, valueless attribute, self-closing tag
		runNodesCase("pug: nodes-basic",
				"flex(direction=\"row\" gap=8 padding=[4, 8])\n"
				"\tlabel.red.big#title Hello\n"
				"\tbutton(enabled)\n"
				"\tnode/\n",
				"+flex direction='row' gap=8 padding=[4,8] "
				"+label id='title' class='red big' \"Hello\" -label "
				"+button enabled=true -button "
				"+node -node "
				"-flex ");

		// dynamic attributes and text interpolation from a shared context
		runNodesCase("pug: nodes-dynamic",
				"node(v=count)\n"
				"\tlabel Hello, #{name}!\n",
				"+node v=42 +label \"Hello, \" \"World\" \"!\" -label -node ",
				[](pug::Context &ctx) {
			ctx.set("count", pug::Value(42));
			ctx.set("name", pug::Value("World"));
		});

		// control flow: each + if
		runNodesCase("pug: nodes-control",
				"each i in list\n"
				"\tnode(v=i)\n"
				"if flag\n"
				"\tlayer\n",
				"+node v=1 -node +node v=2 -node +layer -layer ",
				[](pug::Context &ctx) {
			ctx.set("list", pug::Value({pug::Value(1), pug::Value(2)}));
			ctx.set("flag", pug::Value(true));
		});

		// mixins
		runNodesCase("pug: nodes-mixin",
				"mixin item(txt)\n"
				"\tlabel #{txt}\n"
				"+item(\"A\")\n"
				"+item(\"B\")\n",
				"+label \"A\" -label +label \"B\" -label ");

		// include runs structured against the SAME context and the SAME sink
		{
			auto sub = pug::Template::read("label #{name}\n",
					pug::Template::Options::getNodes(),
					[&](StringView err) { sprt::cout << "    [pug:read] " << err << "\n"; });
			runNodesCase("pug: nodes-include",
					"node\n"
					"\tinclude sub\n",
					"+node +label \"World\" -label -node ",
					[](pug::Context &ctx) { ctx.set("name", pug::Value("World")); },
					[&](pug::Context &ctx, RecordingStream &) {
				ctx.setIncludeCallback([sub](const StringView &, pug::Context &ictx,
						const pug::Context::OutStream &out, pug::Template::RunContext &rctx) {
					return sub->run(ictx, out, rctx);
				});
			});
		}

		// running an HTML-mode template into a NodeStream must fail
		{
			auto tpl = pug::Template::read("div text\n", pug::Template::Options::getDefault(),
					nullptr);
			RecordingStream stream;
			pug::Context ctx;
			check(tpl && !tpl->run(ctx, stream), "pug: nodes-mode-mismatch");
		}

		// HTML regression: the same source without Options::Nodes still renders old HTML
		{
			auto tpl = pug::Template::read("div.cls(a=\"1\")\n\tspan text\n",
					pug::Template::Options::getDefault(),
					[&](StringView err) { sprt::cout << "    [pug:read] " << err << "\n"; });
			if (!tpl) {
				check(false, "pug: html-regression");
			} else {
				pug::StringStream out;
				pug::Context ctx;
				ctx.loadDefaults();
				if (!tpl->run(ctx, [&](StringView str) { out << str; })) {
					check(false, "pug: html-regression");
				} else {
					checkEq(out.str(),
							"<html><body><div a=\"1\" class=\"cls\"><span>text</span></div></body></html>",
							"pug: html-regression");
				}
			}
		}
	}, pool);

	memory::pool::destroy(pool);
}

} // namespace stappler
