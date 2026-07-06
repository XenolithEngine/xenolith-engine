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

/* Structured output sink for templates, compiled with `Template::Options::Nodes`.

Instead of an HTML text stream, such templates emit a tree-shaped event sequence:
nodes with attributes and text content. All `setAttribute` calls for a node arrive
between its `pushNode` and the first `pushString`/child `pushNode`/`popNode`
(attributes can only be defined on the tag line in pug, so no control-flow
chunk can interpose).

Attribute values are passed as typed pool-allocated `pug::Value` (numbers, bools,
arrays, dicts survive as-is) - the sink must copy or convert them before returning,
nothing pool-allocated may be retained past the call. Values are passed by const
reference: a pool-backed Value must not be copied into a stack temporary (its
heap parts are pool-allocated and can not be freed individually). */
class SP_PUBLIC NodeStream : public memory::AllocPool {
public:
	virtual ~NodeStream() = default;

	// enter a new node; tag is the template-defined tag name
	virtual bool pushNode(StringView tag) = 0;

	// leave the current node
	virtual bool popNode() = 0;

	// define an attribute on the current node; valueless attributes arrive as Value(true)
	virtual bool setAttribute(StringView name, const Value &, bool escaped) = 0;

	// text content piece for the current node (interpolation may split text into pieces)
	virtual bool pushString(StringView) = 0;

	virtual void onError(StringView) { }
};

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

		// structured mode (Options::Nodes) only:
		NodeTag, // opens a node, value is the tag name
		NodeTagEnd, // closes a node, value is the tag name (for validation)
	};

	struct Chunk : public memory::AllocPool {
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

			// compile tag structure into node events (NodeStream) instead of HTML text
			Nodes,
		};

		static Options getDefault();
		static Options getPretty();
		static Options getNodes();

		Options &setFlags(sprt::initializer_list<Flags> &&);
		Options &clearFlags(sprt::initializer_list<Flags> &&);

		bool hasFlag(Flags) const;

		sprt::bitset<toInt(Flags::Nodes) + 1> flags;
	};

	struct RunContext {
		Vector<const Template *> templateStack;
		Vector<Template::Chunk *> tagStack;
		bool withinHead = false;
		bool withinBody = false;
		NodeStream *nodeStream = nullptr;
		Options opts;
	};

	static Template *read(const StringView &, const Options & = Options::getDefault(),
			const Callback<void(StringView)> &err = nullptr);

	static Template *read(memory::pool_t *, const StringView &,
			const Options & = Options::getDefault(),
			const Callback<void(StringView)> &err = nullptr);

	Options getOptions() const { return _opts; }

	bool run(Context &, const OutStream &) const;
	bool run(Context &, const OutStream &, const Options &opts) const;
	bool run(Context &, const OutStream &, RunContext &opts) const;

	// structured mode: requires a template compiled with Options::Nodes
	bool run(Context &, NodeStream &) const;
	bool run(Context &, NodeStream &, const Options &opts) const;

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

} // namespace stappler::pug

#endif /* EXTRA_WEBSERVER_PUG_SPPUGTEMPLATE_H_ */
