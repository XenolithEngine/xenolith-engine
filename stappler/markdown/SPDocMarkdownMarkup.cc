/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "SPDocMarkdownMarkup.h"
#include "SPDocPageContainer.h"
#include "SPString.h"

namespace STAPPLER_VERSIONIZED stappler::document {

static constexpr auto s_valueTag = StringView("__value__");

using MarkdownMarkup_Nodes = memory::StandardInterface::VectorType<const Node *>;
using MarkdownMarkup_Views = memory::StandardInterface::VectorType<StringView>;
using MarkdownMarkup_String = memory::StandardInterface::StringType;

// --- spans -------------------------------------------------------------------------------------

static SourceSpan MarkdownMarkup_unite(SourceSpan a, SourceSpan b) {
	if (a.empty()) {
		return b;
	}
	if (b.empty()) {
		return a;
	}
	auto offset = sprt::min(a.offset, b.offset);
	auto end = sprt::max(a.end(), b.end());
	return SourceSpan{offset, end - offset};
}

/* The byte range a whole SUBTREE occupies, which is not always the node's own span.

Two shapes make the difference load-bearing. A wrapper the parser invented carries no span at all -
the `code` inside a `pre` is one - so its own span says nothing about where its text is. And a
table's caption is WRITTEN AFTER the table it belongs to, while the html shape puts it first, so a
child's span can sit outside its parent's and after its own later siblings. Uniting parent and
children makes both harmless, and is why nothing below sorts children or assumes containment. */
static SourceSpan MarkdownMarkup_hull(const Node &node) {
	auto ret = node.getSourceSpan();
	for (auto &it : node.getNodes()) { ret = MarkdownMarkup_unite(ret, MarkdownMarkup_hull(*it)); }
	return ret;
}

// The union of the children's hulls: what a node's own markup wraps around.
static SourceSpan MarkdownMarkup_innerHull(const Node &node) {
	SourceSpan ret;
	for (auto &it : node.getNodes()) { ret = MarkdownMarkup_unite(ret, MarkdownMarkup_hull(*it)); }
	return ret;
}

// Strictly inside: an offset sitting exactly on a boundary cuts nothing, and a node that is not
// cut needs no repair.
static bool MarkdownMarkup_contains(SourceSpan span, uint32_t offset) {
	return !span.empty() && span.offset < offset && offset < span.end();
}

/* The nodes strictly containing `offset`, outermost first.

A linear scan per level, deliberately: children are not sorted by span (see the caption above), so
there is nothing to binary-search. The first match wins, which matters for footnotes - a definition
is exported both at its reference and in the list at the end of the document, and two subtrees then
claim the same bytes. */
static void MarkdownMarkup_chain(const Node &node, uint32_t offset, MarkdownMarkup_Nodes &chain) {
	for (auto &it : node.getNodes()) {
		if (MarkdownMarkup_contains(MarkdownMarkup_hull(*it), offset)) {
			chain.emplace_back(it);
			MarkdownMarkup_chain(*it, offset, chain);
			return;
		}
	}
}

/* Is there anything AT this offset at all?

Some bytes of a Markdown document belong to no node: the blank line between two blocks, a table's
alignment row, and every reference or footnote definition, which the parser consumes into a lookup
table and never turns into a node. They are invisible to a reader, so a selection cannot really
end inside one - but a slice can, and it comes back as a truncated `[r`. */
static bool MarkdownMarkup_covered(const Node &node, uint32_t offset) {
	for (auto &it : node.getNodes()) {
		auto hull = MarkdownMarkup_hull(*it);
		if (hull.empty() || hull.offset > offset || offset > hull.end()) {
			continue;
		}
		if (MarkdownMarkup_covered(*it, offset)) {
			return true;
		}
		auto span = it->getSourceSpan();
		if (!span.empty() && span.offset <= offset && offset <= span.end()) {
			return true;
		}
	}
	return false;
}

// The nearest edge of anything, in one direction; `offset` itself when there is none.
static uint32_t MarkdownMarkup_boundary(const Node &node, uint32_t offset, bool forward,
		uint32_t best) {
	auto span = node.getSourceSpan();
	if (!span.empty()) {
		const uint32_t sides[2] = {span.offset, span.end()};
		for (auto side : sides) {
			if (forward ? (side > offset && side < best) : (side < offset && side > best)) {
				best = side;
			}
		}
	}
	for (auto &it : node.getNodes()) { best = MarkdownMarkup_boundary(*it, offset, forward, best); }
	return best;
}

static bool MarkdownMarkup_holds(const MarkdownMarkup_Nodes &chain, const Node *node) {
	for (auto &it : chain) {
		if (it == node) {
			return true;
		}
	}
	return false;
}

// --- what an edge does -------------------------------------------------------------------------

/* WHAT AN EDGE DOES, per construct it cuts through.

`Inline` is the easy half and the reason the whole approach works: the span of an inline construct
includes both of its delimiters, so the opener is `source[start .. first child)` and the closer is
`source[last child .. end)`, and re-emitting them balances the fragment.

`Block` is the half that has to be restrained. A block marker is LINE-INITIAL markup, and this
assembles a single line out of the pieces - so only the INNERMOST block on the chain may speak. An
outer list or list item would contribute the marker of a line the edge is not even on: cutting
inside the second item of a nested list, the outer item's prefix is the bullet of the FIRST item,
and the fragment comes back with a phantom entry.

`Quote` is the exception that proves it: `> ` is repeated on every line of a quote, so the verbatim
middle already carries it everywhere but the first line, and emitting it is what makes the first
line match the rest.

`Whole` is for constructs that cannot be reopened at all. A table's alignment row (`|---|`) belongs
to no node's span, and an indented code block has no marker to emit - its indent is inside the text
the parser handed back. Both are taken entire or not at all.

`Atomic` is a leaf whose markup means nothing in halves. */
enum class MarkdownMarkup_Role {
	Skip,
	Inline,
	Quote,
	Block,
	Whole,
	Atomic,
};

/* Are these characters the source bytes one for one?

They are not wherever the parser rewrote them: an entity became one character, and smart
typography turned quotes, dashes and ellipses into single ones. Such a run has no per character
mapping to fall back on, so an edge inside it is meaningless - `A &amp; B` cut in the middle of
the entity would copy `p; B`. */
static bool MarkdownMarkup_isVerbatim(const Node &node, StringView source) {
	auto span = node.getSourceSpan();
	auto decoded = string::toUtf16<memory::StandardInterface>(source.sub(span.offset, span.length));
	return WideStringView(decoded) == node.getValue();
}

static MarkdownMarkup_Role MarkdownMarkup_role(const Node &node, StringView source) {
	auto span = node.getSourceSpan();
	if (span.empty() || span.end() > source.size()) {
		// no markup of its own to give back
		return MarkdownMarkup_Role::Skip;
	}

	auto tag = node.getHtmlName();
	if (tag == s_valueTag) {
		return MarkdownMarkup_isVerbatim(node, source) ? MarkdownMarkup_Role::Skip
													   : MarkdownMarkup_Role::Whole;
	}

	// Markup with nothing spanned inside it cannot be reopened - there is no "before the content"
	// to copy when the content has no place. A footnote reference is the live case: `[^note]`
	// renders as a number the processor writes itself, so the node holds markup and no text.
	if (tag != s_valueTag && MarkdownMarkup_innerHull(node).empty()) {
		return MarkdownMarkup_Role::Atomic;
	}

	if (tag == "strong" || tag == "b" || tag == "em" || tag == "i" || tag == "code" || tag == "a"
			|| tag == "del" || tag == "s" || tag == "ins" || tag == "mark" || tag == "sub"
			|| tag == "sup" || tag == "abbr" || tag == "span") {
		return MarkdownMarkup_Role::Inline;
	}

	if (tag == "blockquote") {
		return MarkdownMarkup_Role::Quote;
	}

	if (tag == "pre") {
		// A fence is reopenable, an indented block is not: its markup is the indent, and the
		// indent is part of the text the raw exporter produced.
		auto head = source.sub(span.offset, sprt::min(uint32_t(3), span.length));
		return (head == "```" || head == "~~~") ? MarkdownMarkup_Role::Block
												: MarkdownMarkup_Role::Whole;
	}

	if (tag == "p" || tag == "li" || tag == "dt" || tag == "dd" || tag == "h1" || tag == "h2"
			|| tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
		return MarkdownMarkup_Role::Block;
	}

	if (tag == "table" || tag == "thead" || tag == "tbody" || tag == "tr" || tag == "td"
			|| tag == "th" || tag == "caption") {
		return MarkdownMarkup_Role::Whole;
	}

	if (tag == "input" || tag == "br" || tag == "hr" || tag == "img") {
		return MarkdownMarkup_Role::Atomic;
	}


	// ul, ol, dl, div, figure, colgroup: a container with no markup between its own start and its
	// first child, so there is nothing to repair and nothing to lose by ignoring it.
	return MarkdownMarkup_Role::Skip;
}

// --- utf-8 -------------------------------------------------------------------------------------

static bool MarkdownMarkup_isContinuation(char c) { return (uint8_t(c) & 0xC0) == 0x80; }

// An edge computed from a run map can land mid-codepoint: a verbatim run maps CHARACTERS one for
// one, never bytes, and Cyrillic is two bytes per character. Invalid UTF-8 is never an acceptable
// answer, so both edges move OUT of the code point they split.
static uint32_t MarkdownMarkup_alignBack(StringView source, uint32_t offset) {
	while (offset > 0 && offset < source.size() && MarkdownMarkup_isContinuation(source[offset])) {
		--offset;
	}
	return offset;
}

static uint32_t MarkdownMarkup_alignForward(StringView source, uint32_t offset) {
	while (offset < source.size() && MarkdownMarkup_isContinuation(source[offset])) { ++offset; }
	return offset;
}

// --- post-processing ---------------------------------------------------------------------------

static bool MarkdownMarkup_isSpace(char c) { return c == ' ' || c == '\t'; }

// A nested item's prefix carries the indent it was written at (`    - `), and four spaces at the
// start of a fragment are an indented code block. Removing the same amount from every line keeps
// the relative nesting and loses the absolute one, which is what a fragment wants.
static void MarkdownMarkup_dedent(MarkdownMarkup_String &buf, uint32_t indent) {
	MarkdownMarkup_String ret;
	ret.reserve(buf.size());

	size_t pos = 0;
	while (pos < buf.size()) {
		auto lineEnd = buf.find('\n', pos);
		if (lineEnd == MarkdownMarkup_String::npos) {
			lineEnd = buf.size();
		}

		auto start = pos;
		uint32_t dropped = 0;
		while (start < lineEnd && dropped < indent && MarkdownMarkup_isSpace(buf[start])) {
			++start;
			++dropped;
		}

		ret.append(buf, start, lineEnd - start);
		if (lineEnd < buf.size()) {
			ret.push_back('\n');
		}
		pos = lineEnd + 1;
	}

	buf = sp::move(ret);
}

/* A cut in the middle of a line can PROMOTE the rest of it into a block.

`see -- this`, cut between the dashes, starts a fragment with `- this`, which is a list. So does a
cut before a `#`, a `>`, a fence or a setext rule. The fix keeps the selection exactly as it is and
takes the character's meaning away instead: a backslash escape, which Markdown renders as the
character itself. Leading whitespace has no escape and is simply dropped - four spaces at the head
of a fragment mean code, and the user selected text. */
static void MarkdownMarkup_guardFirstLine(MarkdownMarkup_String &buf) {
	if (buf.empty()) {
		return;
	}

	auto lineEnd = buf.find('\n');
	if (lineEnd == MarkdownMarkup_String::npos) {
		lineEnd = buf.size();
	}

	size_t drop = 0;
	while (drop < lineEnd && MarkdownMarkup_isSpace(buf[drop])) { ++drop; }
	if (drop > 0) {
		buf.erase(0, drop);
		return;
	}

	StringView line(buf.data(), lineEnd);
	auto c = line[0];

	// `#` opens a heading only in a run of one to six followed by a space
	if (c == '#') {
		size_t count = 0;
		while (count < line.size() && line[count] == '#') { ++count; }
		if (count <= 6 && (count == line.size() || MarkdownMarkup_isSpace(line[count]))) {
			buf.insert(size_t(0), 1, '\\');
		}
		return;
	}

	if (c == '>') {
		buf.insert(size_t(0), 1, '\\');
		return;
	}

	// a bullet is a marker only when a space follows it; `*emphasis*` is not a list
	if ((c == '-' || c == '+' || c == '*') && line.size() > 1 && MarkdownMarkup_isSpace(line[1])) {
		buf.insert(size_t(0), 1, '\\');
		return;
	}

	if (c == '`' || c == '~') {
		size_t count = 0;
		while (count < line.size() && line[count] == c) { ++count; }
		if (count >= 3) {
			buf.insert(size_t(0), 1, '\\');
		}
		return;
	}

	// a line of nothing but `-` or `=` turns the line ABOVE it into a heading; here there is no
	// line above, but a fragment pasted under one would move it
	if (c == '-' || c == '=') {
		size_t count = 0;
		while (count < line.size() && line[count] == c) { ++count; }
		if (count == line.size() && count > 1) {
			buf.insert(size_t(0), 1, '\\');
		}
		return;
	}

	// an enumerator: the escape goes before the punctuation, not before the number
	if (c >= '0' && c <= '9') {
		size_t count = 0;
		while (count < line.size() && line[count] >= '0' && line[count] <= '9') { ++count; }
		if (count + 1 < line.size() && (line[count] == '.' || line[count] == ')')
				&& MarkdownMarkup_isSpace(line[count + 1])) {
			buf.insert(count, 1, '\\');
		}
	}
}

// Every `[...]` in the fragment, so the definitions it depends on can be looked up.
static void MarkdownMarkup_collectLabels(StringView fragment,
		memory::StandardInterface::VectorType<MarkdownMarkup_String> &labels) {
	size_t pos = 0;
	while (pos < fragment.size()) {
		if (fragment[pos] == '\\') {
			pos += 2;
			continue;
		}
		if (fragment[pos] != '[') {
			++pos;
			continue;
		}
		auto close = pos + 1;
		while (close < fragment.size() && fragment[close] != ']' && fragment[close] != '\n') {
			++close;
		}
		if (close < fragment.size() && fragment[close] == ']') {
			labels.emplace_back(fragment.data() + pos + 1, close - pos - 1);
			pos = close + 1;
		} else {
			++pos;
		}
	}
}

/* A reference link and a footnote point at a DEFINITION that lives somewhere else in the document,
usually at the bottom. A fragment carrying `[the site][ref]` without `[ref]: https://…` pastes as
literal brackets, so the definitions the fragment needs are appended after it. Appending leaves the
verbatim middle alone, which is the whole point of the design. */
static void MarkdownMarkup_appendDefinitions(MarkdownMarkup_String &buf, StringView source,
		uint32_t begin, uint32_t end) {
	memory::StandardInterface::VectorType<MarkdownMarkup_String> labels;
	MarkdownMarkup_collectLabels(StringView(buf.data(), buf.size()), labels);
	if (labels.empty()) {
		return;
	}

	size_t pos = 0;
	while (pos < source.size()) {
		auto lineEnd = source.sub(pos).find('\n');
		auto stop = (lineEnd == maxOf<size_t>()) ? source.size() : pos + lineEnd;

		StringView line(source.data() + pos, stop - pos);
		auto cursor = line;
		cursor.skipChars<StringView::Chars<' ', '\t'>>();
		cursor.skipChars<StringView::Chars<'*'>>(); // an abbreviation is `*[ABBR]: …`

		if (!cursor.empty() && cursor[0] == '[') {
			auto label = cursor.sub(1);
			auto close = label.find(']');
			if (close != maxOf<size_t>()) {
				auto name = label.sub(0, close);
				auto rest = label.sub(close + 1);
				if (!rest.empty() && rest[0] == ':') {
					// A definition the fragment already carries must not be repeated, and it can
					// arrive by two roads: inside the copied range, or as the block marker a cut
					// inside a footnote's own text re-emits.
					bool inside = !(stop <= begin || pos >= end)
							|| StringView(buf.data(), buf.size()).find(line) != maxOf<size_t>();
					if (!inside) {
						for (auto &it : labels) {
							if (StringView(it.data(), it.size()) == name) {
								buf.append("\n\n");
								buf.append(line.data(), line.size());
								break;
							}
						}
					}
				}
			}
		}

		pos = stop + 1;
	}
}

// --- the fragment ------------------------------------------------------------------------------

void writeMarkdownFragment(const Callback<void(StringView)> &out, const Node &root,
		StringView source, uint32_t begin, uint32_t end, MarkdownMarkup mode) {
	if (source.empty()) {
		return;
	}

	end = sprt::min(end, uint32_t(source.size()));
	begin = sprt::min(begin, end);
	begin = MarkdownMarkup_alignBack(source, begin);
	end = MarkdownMarkup_alignForward(source, end);
	if (begin >= end) {
		return;
	}

	if (mode == MarkdownMarkup::Raw) {
		out(source.sub(begin, end - begin));
		return;
	}

	MarkdownMarkup_Nodes chainBegin;
	MarkdownMarkup_Nodes chainEnd;

	auto rebuild = [&] {
		chainBegin.clear();
		chainEnd.clear();
		MarkdownMarkup_chain(root, begin, chainBegin);
		MarkdownMarkup_chain(root, end, chainEnd);
	};

	// A construct that cannot be reopened moves the edge instead of being repaired. Widening can
	// bring a new ancestor into the chain, so this settles rather than runs once - two rounds is
	// already more than any real nesting needs.
	auto snap = [&] {
		auto changed = false;
		for (auto &it : chainBegin) {
			auto span = it->getSourceSpan();
			if (!MarkdownMarkup_contains(span, begin)) {
				continue;
			}
			switch (MarkdownMarkup_role(*it, source)) {
			case MarkdownMarkup_Role::Whole:
				begin = span.offset;
				changed = true;
				break;
			case MarkdownMarkup_Role::Atomic:
				begin = (begin - span.offset <= span.end() - begin) ? span.offset : span.end();
				changed = true;
				break;
			default: break;
			}
		}
		for (auto &it : chainEnd) {
			auto span = it->getSourceSpan();
			if (!MarkdownMarkup_contains(span, end)) {
				continue;
			}
			switch (MarkdownMarkup_role(*it, source)) {
			case MarkdownMarkup_Role::Whole:
				end = span.end();
				changed = true;
				break;
			case MarkdownMarkup_Role::Atomic:
				end = (end - span.offset <= span.end() - end) ? span.offset : span.end();
				changed = true;
				break;
			default: break;
			}
		}
		return changed;
	};

	// An edge in the space between nodes is pulled onto content first: cutting a reference
	// definition or a table's alignment row in half produces markup that means nothing, and the
	// bytes it would have copied show nothing to begin with.
	auto blank = [](StringView text) {
		for (auto c : text) {
			if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
				return false;
			}
		}
		return true;
	};

	// The two ends of the source are not gaps between nodes but the document's own edges: there is
	// nothing beyond them to be cut in half, and pulling off them would lose the trailing newline
	// of a whole-document range.
	if (begin > 0 && !MarkdownMarkup_covered(root, begin)) {
		auto to = MarkdownMarkup_boundary(root, begin, true, end);
		// Whitespace between nodes is not markup and costs nothing to keep - which is also what
		// leaves the trailing newline of a whole-document range where it was.
		if (to > begin && !blank(source.sub(begin, to - begin))) {
			begin = to;
		}
	}
	if (end < source.size() && !MarkdownMarkup_covered(root, end)) {
		auto to = MarkdownMarkup_boundary(root, end, false, begin);
		if (to < end && !blank(source.sub(to, end - to))) {
			end = to;
		}
	}
	if (begin >= end) {
		return;
	}

	for (uint32_t i = 0; i < 3; ++i) {
		rebuild();
		if (!snap()) {
			break;
		}
		if (begin >= end) {
			return; // an atomic swallowed what was left
		}
	}
	rebuild();

	// Only the innermost block may contribute a marker; see MarkdownMarkup_Role.
	const Node *innerBlockBegin = nullptr;
	for (auto &it : chainBegin) {
		if (MarkdownMarkup_role(*it, source) == MarkdownMarkup_Role::Block) {
			innerBlockBegin = it;
		}
	}
	const Node *innerBlockEnd = nullptr;
	for (auto &it : chainEnd) {
		if (MarkdownMarkup_role(*it, source) == MarkdownMarkup_Role::Block) {
			innerBlockEnd = it;
		}
	}

	auto mayRepair = [&](const Node *node, const Node *innerBlock) {
		switch (MarkdownMarkup_role(*node, source)) {
		case MarkdownMarkup_Role::Inline:
		case MarkdownMarkup_Role::Quote: return true;
		case MarkdownMarkup_Role::Block: return node == innerBlock;
		default: return false;
		}
	};

	MarkdownMarkup_Views prefixes;
	for (auto &it : chainBegin) {
		if (!mayRepair(it, innerBlockBegin)) {
			continue;
		}

		auto span = it->getSourceSpan();
		if (span.offset >= begin) {
			continue; // nothing of this node was cut away
		}
		if (span.end() > end && !MarkdownMarkup_holds(chainEnd, it)) {
			continue; // refuse to open what the other edge will not close
		}

		auto inner = MarkdownMarkup_innerHull(*it);
		if (inner.empty() || inner.offset <= span.offset || inner.offset > span.end()) {
			continue; // nothing between the node and its content: no marker to re-emit
		}

		prefixes.emplace_back(source.sub(span.offset, inner.offset - span.offset));

		// the edge landed inside the marker itself: move it past, or the marker doubles
		if (begin < inner.offset) {
			begin = inner.offset;
		}
	}

	MarkdownMarkup_Views suffixes;
	for (auto it = chainEnd.rbegin(); it != chainEnd.rend(); ++it) {
		if (!mayRepair(*it, innerBlockEnd)) {
			continue;
		}

		auto span = (*it)->getSourceSpan();
		if (span.end() <= end) {
			continue;
		}
		if (span.offset < begin && !MarkdownMarkup_holds(chainBegin, *it)) {
			continue;
		}

		auto inner = MarkdownMarkup_innerHull(**it);
		if (inner.empty() || inner.end() >= span.end() || inner.end() < span.offset) {
			continue;
		}

		suffixes.emplace_back(source.sub(inner.end(), span.end() - inner.end()));

		if (end > inner.end()) {
			end = inner.end();
		}
	}

	if (begin >= end) {
		return;
	}

	MarkdownMarkup_String buf;
	for (auto &it : prefixes) { buf.append(it.data(), it.size()); }
	buf.append(source.data() + begin, end - begin);
	for (auto &it : suffixes) { buf.append(it.data(), it.size()); }

	if (!prefixes.empty()) {
		uint32_t indent = 0;
		auto &first = prefixes.front();
		while (indent < first.size() && MarkdownMarkup_isSpace(first[indent])) { ++indent; }
		if (indent > 0) {
			MarkdownMarkup_dedent(buf, indent);
		}
	} else if (begin > 0 && source[begin - 1] != '\n') {
		// nothing was prepended, so the fragment starts mid-line and the rest of that line is
		// suddenly line-initial
		MarkdownMarkup_guardFirstLine(buf);
	}

	MarkdownMarkup_appendDefinitions(buf, source, begin, end);

	out(StringView(buf.data(), buf.size()));
}

void writeMarkdownFragment(const Callback<void(StringView)> &out, const DocumentMarkdown &doc,
		uint32_t begin, uint32_t end, MarkdownMarkup mode) {
	auto page = doc.getRoot();
	if (!page || !page->getRoot()) {
		return;
	}
	writeMarkdownFragment(out, *page->getRoot(), doc.getSource(), begin, end, mode);
}

// --- the text ----------------------------------------------------------------------------------

static bool MarkdownMarkup_isInlineTag(StringView tag) {
	return tag == s_valueTag || tag == "strong" || tag == "b" || tag == "em" || tag == "i"
			|| tag == "code" || tag == "a" || tag == "del" || tag == "s" || tag == "ins"
			|| tag == "mark" || tag == "sub" || tag == "sup" || tag == "abbr" || tag == "span"
			|| tag == "br" || tag == "img" || tag == "input";
}

static const Node *MarkdownMarkup_blockOf(const Node *node) {
	while (node && MarkdownMarkup_isInlineTag(node->getHtmlName())) { node = node->getParent(); }
	return node;
}

// Two runs in one block are adjacent text, never lines; two blocks in one item or cell are lines;
// anything else is a paragraph break.
static StringView MarkdownMarkup_separator(const Node *from, const Node *to) {
	if (from == to) {
		return StringView();
	}
	auto parent = to ? to->getParent() : nullptr;
	if (parent && from && from->getParent() == parent) {
		auto tag = parent->getHtmlName();
		if (tag == "li" || tag == "td" || tag == "th" || tag == "pre") {
			return StringView("\n");
		}
	}
	return StringView("\n\n");
}

static void MarkdownMarkup_writeText(const Callback<void(StringView)> &out, const Node &node,
		StringView source, uint32_t begin, uint32_t end, const Node *&lastBlock) {
	if (node.getHtmlName() == s_valueTag) {
		auto span = node.getSourceSpan();
		if (span.empty() || span.end() <= begin || span.offset >= end
				|| span.end() > source.size()) {
			return;
		}

		auto value = node.getValue();
		if (value.empty()) {
			return;
		}

		auto block = MarkdownMarkup_blockOf(&node);
		if (lastBlock) {
			auto sep = MarkdownMarkup_separator(lastBlock, block);
			if (!sep.empty()) {
				out(sep);
			}
		}
		lastBlock = block;

		if (span.offset >= begin && span.end() <= end) {
			out(string::toUtf8<memory::StandardInterface>(value));
			return;
		}

		/* A partially covered run can only be cut where the characters ARE the source bytes: the
		parser decoded entities and applied smart typography, so a substituted run has no per
		character mapping at all and is taken whole. */
		auto fragment = source.sub(span.offset, span.length);
		auto decoded = string::toUtf16<memory::StandardInterface>(fragment);
		if (WideStringView(decoded) != value) {
			out(string::toUtf8<memory::StandardInterface>(value));
			return;
		}

		auto from = sprt::max(begin, span.offset) - span.offset;
		auto to = sprt::min(end, span.end()) - span.offset;

		// Widening, not narrowing, and for the same reason the fragment widens: an edge that
		// splits a code point has to move OUT of it, and the two answers have to agree about
		// which way or the text would not be the text of the markup.
		from = MarkdownMarkup_alignBack(fragment, from);
		to = MarkdownMarkup_alignForward(fragment, to);
		if (from >= to) {
			return;
		}

		// bytes into characters: a verbatim run maps one for one by CHARACTER, never by byte
		auto charFrom = uint32_t(sprt::unicode::getUtf16Length(fragment.sub(0, from)));
		auto charTo = uint32_t(sprt::unicode::getUtf16Length(fragment.sub(0, to)));
		if (charTo > value.size()) {
			charTo = uint32_t(value.size());
		}
		if (charFrom >= charTo) {
			return;
		}

		out(string::toUtf8<memory::StandardInterface>(value.sub(charFrom, charTo - charFrom)));
		return;
	}

	for (auto &it : node.getNodes()) {
		MarkdownMarkup_writeText(out, *it, source, begin, end, lastBlock);
	}
}

void writeMarkdownText(const Callback<void(StringView)> &out, const Node &root, StringView source,
		uint32_t begin, uint32_t end) {
	if (source.empty()) {
		return;
	}

	end = sprt::min(end, uint32_t(source.size()));
	begin = sprt::min(begin, end);
	if (begin >= end) {
		return;
	}

	const Node *lastBlock = nullptr;
	MarkdownMarkup_writeText(out, root, source, begin, end, lastBlock);
}

void writeMarkdownText(const Callback<void(StringView)> &out, const DocumentMarkdown &doc,
		uint32_t begin, uint32_t end) {
	auto page = doc.getRoot();
	if (!page || !page->getRoot()) {
		return;
	}
	writeMarkdownText(out, *page->getRoot(), doc.getSource(), begin, end);
}

} // namespace stappler::document
