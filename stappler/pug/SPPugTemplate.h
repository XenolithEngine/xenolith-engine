/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef EXTRA_WEBSERVER_PUG_SPPUGTEMPLATE_H_
#define EXTRA_WEBSERVER_PUG_SPPUGTEMPLATE_H_

#include "SPPugLexer.h"

namespace STAPPLER_VERSIONIZED stappler::pug {

class SP_PUBLIC Template : public memory::AllocPool {
public:
	using OutStream = Callback<void(StringView)>;

	enum ChunkType {
		Block,
		HtmlTag,
		HtmlInlineTag,
		HtmlEntity,
		OutputEscaped,
		OutputUnescaped,
		AttributeEscaped,
		AttributeUnescaped,
		AttributeList,
		Code,

		ControlCase,
		ControlWhen,
		ControlDefault,

		ControlIf,
		ControlUnless,
		ControlElseIf,
		ControlElse,

		ControlEach,
		ControlEachPair,

		ControlWhile,

		Include,

		ControlMixin,

		MixinCall,

		VirtualTag,
	};

	struct Chunk {
		ChunkType type = Block;
		String value;
		Expression *expr = nullptr;
		size_t indent = 0;
		Vector<Chunk *> chunks;
	};

	struct Options {
		enum Flags {
			Pretty,
			StopOnError,
			LineFeeds,
		};

		static Options getDefault();
		static Options getPretty();

		Options &setFlags(std::initializer_list<Flags> &&);
		Options &clearFlags(std::initializer_list<Flags> &&);

		bool hasFlag(Flags) const;

		std::bitset<toInt(Flags::LineFeeds) + 1> flags;
	};

	struct RunContext {
		Vector<const Template *> templateStack;
		Vector<Template::Chunk *> tagStack;
		bool withinHead = false;
		bool withinBody = false;
		Options opts;
	};

	static Template *read(const StringView &, const Options & = Options::getDefault(),
			const Callback<void(StringView)> &err = nullptr);

	static Template *read(memory::pool_t *, const StringView &, const Options & = Options::getDefault(),
			const Callback<void(StringView)> &err = nullptr);

	Options getOptions() const { return _opts; }

	bool run(Context &, const OutStream &) const;
	bool run(Context &, const OutStream &, const Options &opts) const;
	bool run(Context &, const OutStream &, RunContext &opts) const;

	void describe(const OutStream &stream, bool tokens = false) const;

protected:
	Template(memory::pool_t *, const StringView &, const Options &opts, const OutStream &err);

	bool runChunk(const Chunk &chunk, Context &, const OutStream &, RunContext &) const;
	bool runCase(const Chunk &chunk, Context &, const OutStream &, RunContext &) const;

	void pushWithPrettyFilter(StringView, size_t indent, const OutStream &) const;

	memory::pool_t *_pool;
	Lexer _lexer;
	Time _mtime;
	Chunk _root;
	Options _opts;

	Vector<StringView> _includes;
};

}

#endif /* EXTRA_WEBSERVER_PUG_SPPUGTEMPLATE_H_ */
