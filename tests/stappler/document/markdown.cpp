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

// Markdown parsing (stappler_markdown): the node tree a MultiMarkdown source produces, and the
// source spans every node carries. The spans are what the ui layer will map a rendered character
// back through, so they are checked as hard as the tree shape is.

#include "SPCommon.h"
#include "SPDocMarkdown.h"
#include "SPDocMarkdownMarkup.h"
#include "SPDocNode.h"
#include "SPDocPageContainer.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

static constexpr auto s_markdownSource = R"Md(# Title

Plain paragraph with **bold**, *emphasis* and `code`.

## Second level

- first item
- second item with [a link](https://example.org)

> quoted line

```cpp
int main() { return 0; }
```

| a | b |
|---|---|
| 1 | 2 |
)Md";

struct Sample {
	StringView name;
	StringView text;
};

static const Sample s_samples[] = {
	{"nested list", "# T\n\n- one\n    - inner one\n    - inner two\n- two\n"},
	{"reference link", "# T\n\nSee [the site][ref] for more.\n\n[ref]: https://example.org\n"},
	{"footnote", "# T\n\nA claim[^note].\n\n[^note]: The evidence.\n"},
	{"entities and dashes", "# T\n\nA &amp; B -- see \"quotes\" and 3 < 4.\n"},
	{"multibyte", "# Заголовок\n\nТекст с **выделением** и emoji \xf0\x9f\x8e\x89 внутри.\n"},
	{"indented code", "# T\n\n    int x = 0;\n    return x;\n"},
	{"setext header", "Title\n=====\n\nBody text.\n"},
	{"task list", "# T\n\n- [ ] open item\n- [x] done item\n"},
	{"the whole demo document", s_markdownSource},
};

// The node's own text, without its children's.
static StringView nodeText(memory::pool_t *pool, const document::Node &node) {
	return StringView(string::toUtf8<memory::PoolInterface>(node.getValue())).pdup(pool);
}

// Depth-first walk in document order, the same order the ui layer will lay nodes out in.
static void foreachNode(const document::Node &node,
		const Callback<void(const document::Node &, size_t)> &cb, size_t level = 0) {
	cb(node, level);
	for (auto &it : node.getNodes()) { foreachNode(*it, cb, level + 1); }
}

static const document::Node *findNode(const document::Node &root, StringView name,
		size_t skip = 0) {
	const document::Node *ret = nullptr;
	foreachNode(root, [&](const document::Node &node, size_t) {
		if (!ret && node.getHtmlName() == name) {
			if (skip == 0) {
				ret = &node;
			} else {
				--skip;
			}
		}
	});
	return ret;
}

// --- markup reconstruction -----------------------------------------------------------------

// Whitespace is a rendering decision, not content: two texts are compared THROUGH it, or a
// paragraph break and a space would count as a difference.
static StringView squashText(memory::pool_t *pool, StringView text) {
	memory::PoolInterface::StringType ret;
	bool space = false;
	for (auto c : text) {
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			space = !ret.empty();
			continue;
		}
		if (space) {
			ret.push_back(' ');
			space = false;
		}
		ret.push_back(c);
	}
	return StringView(ret).pdup(pool);
}

// Everything the document says, with none of how it is written.
static StringView visibleText(memory::pool_t *pool, const document::Node &root) {
	memory::PoolInterface::StringType ret;
	foreachNode(root, [&](const document::Node &node, size_t) {
		if (node.getHtmlName() == "__value__") {
			auto utf8 = string::toUtf8<memory::PoolInterface>(node.getValue());
			ret.append(utf8.data(), utf8.size());
			ret.push_back(' ');
		}
	});
	return squashText(pool, StringView(ret));
}

/* Does `got` still say every word `expect` said, in order?

Not a substring: a fragment is allowed to say MORE. An edge inside a table takes the whole table,
an edge inside a rewritten run takes the whole run, and a copied footnote reference brings its
definition along and renders a marker number that was not in the range. What may never happen is
LOSING a word, or reordering two. Punctuation is trimmed off each word because a synthetic marker
can land between a word and its full stop. */
static bool holdsWordsInOrder(StringView got, StringView expect) {
	auto isBoundary = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
	auto isTrimmable = [](char c) {
		return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`')
				|| (c >= '{' && c <= '~');
	};

	memory::PoolInterface::VectorType<StringView> words;
	size_t pos = 0;
	while (pos < got.size()) {
		while (pos < got.size() && isBoundary(got[pos])) { ++pos; }
		auto start = pos;
		while (pos < got.size() && !isBoundary(got[pos])) { ++pos; }
		auto word = got.sub(start, pos - start);
		while (!word.empty() && isTrimmable(word[0])) { word = word.sub(1); }
		while (!word.empty() && isTrimmable(word[word.size() - 1])) {
			word = word.sub(0, word.size() - 1);
		}
		if (!word.empty()) {
			words.emplace_back(word);
		}
	}

	size_t cursor = 0;
	pos = 0;
	while (pos < expect.size()) {
		while (pos < expect.size() && isBoundary(expect[pos])) { ++pos; }
		auto start = pos;
		while (pos < expect.size() && !isBoundary(expect[pos])) { ++pos; }
		auto word = expect.sub(start, pos - start);
		while (!word.empty() && isTrimmable(word[0])) { word = word.sub(1); }
		while (!word.empty() && isTrimmable(word[word.size() - 1])) {
			word = word.sub(0, word.size() - 1);
		}
		if (word.empty()) {
			continue;
		}

		bool found = false;
		while (cursor < words.size()) {
			if (words[cursor++] == word) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}
	return true;
}

static bool isValidUtf8(StringView text) {
	size_t pos = 0;
	while (pos < text.size()) {
		auto len = sprt::unicode::utf8_length_data[uint8_t(text[pos])];
		if (len == 0 || pos + len > text.size()) {
			return false;
		}
		for (size_t i = 1; i < len; ++i) {
			if ((uint8_t(text[pos + i]) & 0xC0) != 0x80) {
				return false;
			}
		}
		pos += len;
	}
	return true;
}

/* THE POINT OF THE WHOLE MILESTONE: a range of a document, handed back as the markup that
produced it.

Two properties are checked, and they pull in opposite directions on purpose. The exact one: the
whole document comes back as the source, byte for byte, which no amount of clever reassembly
would survive. The general one: for any range at all, the fragment reparses, and everything the
range showed is still in what it says - fragments are allowed to be WIDER than the range (an
edge inside a table takes the table, an edge inside a substituted run takes the run) but never
narrower, and never syntactically broken. */
static void performMarkdownMarkupTests(memory::pool_t *pool) {
	using namespace stappler::document;

	StringView src(s_markdownSource);
	auto doc = Rc<DocumentMarkdown>::create(pool,
			BytesView(reinterpret_cast<const uint8_t *>(src.data()), src.size()),
			StringView("text/markdown"));
	if (!doc || !doc->getRoot()) {
		check(false, "markdown markup: document is parsed");
		return;
	}

	auto markupOf = [&](const DocumentMarkdown &d, uint32_t begin, uint32_t end,
							MarkdownMarkup mode = MarkdownMarkup::Normalized) {
		memory::PoolInterface::StringType ret;
		writeMarkdownFragment([&](StringView s) { ret.append(s.data(), s.size()); }, d, begin, end,
				mode);
		return StringView(ret).pdup(pool);
	};

	auto markup = [&](uint32_t begin, uint32_t end,
						  MarkdownMarkup mode = MarkdownMarkup::Normalized) {
		return markupOf(*doc, begin, end, mode);
	};

	auto at = [&](StringView needle, uint32_t delta = 0) {
		auto pos = src.find(needle);
		return uint32_t(pos == maxOf<size_t>() ? 0 : pos) + delta;
	};

	// --- the exact property ---

	checkEq(markup(0, uint32_t(src.size()), MarkdownMarkup::Raw), src,
			"markdown markup: the raw whole document is the source");
	checkEq(markup(0, uint32_t(src.size())), src,
			"markdown markup: the whole document repairs to the source");

	// --- one cut per construct ---

	checkEq(markup(at("bold", 1), at("bold", 3)), "**ol**",
			"markdown markup: a cut inside bold closes both ends");
	checkEq(markup(at("Title", 2), at("Title", 5)), "# tle",
			"markdown markup: a cut inside a heading keeps the heading");
	checkEq(markup(at("a link", 2), at("a link", 6)), "- [link](https://example.org)",
			"markdown markup: a cut inside a link keeps the target and the item");

	auto fence = markup(at("int main", 4), at("int main", 8));
	check(fence.starts_with("```cpp") && fence.ends_with("```"),
			"markdown markup: a cut inside a fence reopens it");

	auto cell = markup(at("| 1 | 2 |", 2), at("| 1 | 2 |", 3));
	check(cell.find(StringView("|---|")) != maxOf<size_t>(),
			"markdown markup: a cut inside a table takes the table");

	// --- the same, over the harder corpus ---

	for (auto &sample : s_samples) {
		auto sampleDoc = Rc<DocumentMarkdown>::create(pool,
				BytesView(reinterpret_cast<const uint8_t *>(sample.text.data()),
						sample.text.size()),
				StringView("text/markdown"));
		auto label = string::toString<memory::PoolInterface>("markdown roundtrip: ", sample.name);
		if (!sampleDoc || !sampleDoc->getRoot()) {
			check(false, label);
			continue;
		}

		checkEq(markupOf(*sampleDoc, 0, uint32_t(sample.text.size())), sample.text,
				string::toString<memory::PoolInterface>("markdown whole: ", sample.name));

		// A seeded generator, so a failure is reproducible from the sample and the index rather
		// than from luck.
		uint32_t seed = 0x9E37'79B9u ^ uint32_t(sample.name.size() * 2'654'435'761u);
		auto nextRandom = [&] {
			seed = seed * 1'103'515'245u + 12'345u;
			return (seed >> 8) & 0xFF'FFFF;
		};

		bool ok = true;
		StringView failure;
		for (uint32_t i = 0; i < 96 && ok; ++i) {
			auto size = uint32_t(sample.text.size());
			auto begin = nextRandom() % size;
			auto end = nextRandom() % size;
			if (begin > end) {
				sprt::swap(begin, end);
			}
			if (begin == end) {
				continue;
			}

			auto fragment = markupOf(*sampleDoc, begin, end);
			if (fragment.empty()) {
				continue; // a range holding nothing but markup has nothing to give back
			}

			if (!isValidUtf8(fragment)) {
				ok = false;
				failure = fragment;
				break;
			}

			auto fragmentDoc = Rc<DocumentMarkdown>::create(pool,
					BytesView(reinterpret_cast<const uint8_t *>(fragment.data()), fragment.size()),
					StringView("text/markdown"));
			if (!fragmentDoc || !fragmentDoc->getRoot()) {
				ok = false;
				failure = fragment;
				break;
			}

			memory::PoolInterface::StringType rangeText;
			writeMarkdownText([&](StringView s) { rangeText.append(s.data(), s.size()); },
					*sampleDoc, begin, end);

			auto expect = squashText(pool, StringView(rangeText));
			auto got = visibleText(pool, *fragmentDoc->getRoot()->getRoot());

			/* A definition (`[^note]: …`, `[ref]: …`) is shown through whatever REFERENCES it,
			and a fragment need not carry the reference: cutting inside the text of a footnote
			copies the footnote's definition, which on its own renders nothing at all. The
			document it came from does show that text, in the footnote list at the bottom, so
			what the fragment declares counts as what it says. */
			if (fragment.find(StringView("]:")) != maxOf<size_t>() || got.empty()) {
				got = squashText(pool, string::toString<memory::PoolInterface>(got, " ", fragment));
			}
			if (!holdsWordsInOrder(got, expect)) {
				ok = false;
				failure = string::toString<memory::PoolInterface>("[", begin, ",", end, ") ",
						fragment, " -- want text: ", expect, " -- got text: ", got);
			}
		}

		if (!ok) {
			sprt::cout << "    " << failure << "\n";
		}
		check(ok, label);
	}
}

void performMarkdownTests() {
	using namespace stappler::document;

	sprt::cout << "\n== stappler markdown tests ==\n";

	auto pool = memory::pool::create((memory::pool_t *)nullptr);

	memory::perform([&] {
		StringView src(s_markdownSource);

		check(DocumentMarkdown::isMarkdown(src), "markdown: source is detected as markdown");
		check(DocumentMarkdown::isMarkdownContentType("text/markdown"),
				"markdown: content type is accepted");
		check(!DocumentMarkdown::isMarkdown(StringView("just a line of prose\n")),
				"markdown: plain prose is not claimed");

		auto doc = Rc<DocumentMarkdown>::create(pool,
				BytesView(reinterpret_cast<const uint8_t *>(src.data()), src.size()),
				StringView("text/markdown"));

		check(doc != nullptr, "markdown: document is parsed");
		if (!doc) {
			return;
		}

		checkEq(doc->getSource(), src, "markdown: source text is kept with the document");

		auto page = doc->getRoot();
		check(page != nullptr, "markdown: root page exists");
		if (!page) {
			return;
		}

		auto root = page->getRoot();

		// --- tree shape ---

		auto countTag = [&](StringView name) -> size_t {
			size_t ret = 0;
			foreachNode(*root, [&](const document::Node &node, size_t) {
				if (node.getHtmlName() == name) {
					++ret;
				}
			});
			return ret;
		};

		check(countTag("h1") == 1, "markdown: one h1");
		check(countTag("h2") == 1, "markdown: one h2");
		check(countTag("p") >= 1, "markdown: paragraph produced");
		check(countTag("strong") == 1, "markdown: strong produced");
		check(countTag("em") == 1, "markdown: em produced");
		check(countTag("code") >= 2, "markdown: inline and fenced code produced");
		check(countTag("pre") == 1, "markdown: fenced block produced");
		check(countTag("ul") == 1 && countTag("li") == 2, "markdown: list with two items");
		check(countTag("a") == 1, "markdown: link produced");
		check(countTag("blockquote") == 1, "markdown: blockquote produced");
		check(countTag("table") == 1 && countTag("td") == 2, "markdown: table with two cells");

		if (auto link = findNode(*root, "a")) {
			checkEq(link->getAttribute("href"), "https://example.org", "markdown: link href");
		} else {
			check(false, "markdown: link href");
		}

		// --- source spans ---

		size_t spanned = 0;
		bool allInside = true;
		bool allNonEmptyText = true;
		foreachNode(*root, [&](const document::Node &node, size_t) {
			auto span = node.getSourceSpan();
			if (span.empty()) {
				return;
			}
			++spanned;
			if (span.end() > src.size()) {
				allInside = false;
				return;
			}
			auto fragment = src.sub(span.offset, span.length);
			if (fragment.empty()) {
				allInside = false;
			}
			if (node.getHtmlName() == "__value__") {
				// a text run's span must cover text, not just markup punctuation
				auto trimmed = fragment;
				trimmed.trimChars<StringView::WhiteSpace>();
				if (trimmed.empty()) {
					allNonEmptyText = false;
				}
			}
		});

		check(spanned > 0, "markdown: nodes carry source spans");
		check(allInside, "markdown: every span cuts a non-empty fragment inside the source");
		check(allNonEmptyText, "markdown: every text span covers actual text");

		auto checkSpan = [&](StringView tag, StringView expect, StringView name, size_t skip = 0) {
			if (auto node = findNode(*root, tag, skip)) {
				auto span = node->getSourceSpan();
				if (span.empty() || span.end() > src.size()) {
					check(false, name);
					return;
				}
				checkEq(src.sub(span.offset, span.length), expect, name);
			} else {
				check(false, name);
			}
		};

		checkSpan("h1", "# Title", "markdown: h1 span is its own markup");
		checkSpan("strong", "**bold**", "markdown: strong span keeps its delimiters");
		checkSpan("em", "*emphasis*", "markdown: em span keeps its delimiters");
		checkSpan("a", "[a link](https://example.org)", "markdown: link span is the whole link");
		checkSpan("code", "`code`", "markdown: inline code span keeps its backticks");
		checkSpan("pre", "```cpp\nint main() { return 0; }\n```",
				"markdown: fenced block spans its fences");

		// Verbatim text leaves through exportTokenRaw, which claims its span the same way
		// exportToken does. Without that a code block carries no source map at all - the one
		// place where a copy has to be byte for byte.
		if (auto pre = findNode(*root, "pre")) {
			size_t values = 0;
			bool spanned = true;
			foreachNode(*pre, [&](const document::Node &node, size_t) {
				if (node.getHtmlName() != "__value__") {
					return;
				}
				++values;
				auto span = node.getSourceSpan();
				if (span.empty() || span.end() > src.size()
						|| src.sub(span.offset, span.length) != nodeText(pool, node)) {
					spanned = false;
				}
			});
			check(values > 0 && spanned, "markdown: code text spans its own bytes");
		}

		// The first text run of the h1 is the header text without the `#` marker: attributing it
		// to the block token instead of the innermost one is exactly the bug the span bookkeeping
		// in DocumentProcessor::exportToken exists to prevent.
		if (auto h1 = findNode(*root, "h1")) {
			auto &nodes = h1->getNodes();
			check(nodes.size() == 1, "markdown: h1 holds one text run");
			if (!nodes.empty()) {
				auto span = nodes.front()->getSourceSpan();
				checkEq(src.sub(span.offset, span.length), "Title",
						"markdown: h1 text span excludes the marker");
				checkEq(nodeText(pool, *nodes.front()), "Title", "markdown: h1 text");
			}
		}

		// --- task lists ---

		// MultiMarkdown has no task-list extension of its own; this one is ours, and what makes it
		// safe for the milestones that copy markup is that the checkbox node carries the span of
		// the `[x]` that produced it, while the item's text starts after it.
		{
			StringView taskSrc("# T\n\n- [ ] open item\n- [x] done item\n");
			auto taskDoc = Rc<DocumentMarkdown>::create(pool,
					BytesView(reinterpret_cast<const uint8_t *>(taskSrc.data()), taskSrc.size()),
					StringView("text/markdown"));

			auto taskRoot =
					(taskDoc && taskDoc->getRoot()) ? taskDoc->getRoot()->getRoot() : nullptr;
			check(taskRoot != nullptr, "markdown task: document parsed");

			if (taskRoot) {
				size_t boxes = 0, checked = 0;
				bool spansOk = true;
				bool textOk = true;
				foreachNode(*taskRoot, [&](const document::Node &node, size_t) {
					if (node.getHtmlName() == "input") {
						++boxes;
						if (!node.getAttribute("checked").empty()) {
							++checked;
						}
						auto span = node.getSourceSpan();
						if (span.empty() || span.end() > taskSrc.size()
								|| taskSrc.sub(span.offset, 3) != StringView("[ ]").sub(0, 3)) {
							// the span must cover a marker, either form
							auto fragment = taskSrc.sub(span.offset, 3);
							if (fragment != "[ ]" && fragment != "[x]") {
								spansOk = false;
							}
						}
					} else if (node.getHtmlName() == "__value__") {
						auto text = string::toUtf8<memory::PoolInterface>(node.getValue());
						if (StringView(text).starts_with("[")) {
							textOk = false; // the marker leaked into the item's text
						}
					}
				});

				check(boxes == 2, "markdown task: one checkbox per item");
				check(checked == 1, "markdown task: only the `[x]` item is checked");
				check(spansOk, "markdown task: each checkbox spans its own marker");
				check(textOk, "markdown task: the marker is not repeated in the text");
			}
		}

		// Text runs are produced in document order, so their spans never go backwards.
		uint32_t last = 0;
		bool monotonic = true;
		foreachNode(*root, [&](const document::Node &node, size_t) {
			if (node.getHtmlName() != "__value__") {
				return;
			}
			auto span = node.getSourceSpan();
			if (span.empty()) {
				return;
			}
			if (span.offset < last) {
				monotonic = false;
			}
			last = span.offset;
		});
		check(monotonic, "markdown: text spans follow document order");

		// --- the same invariants over harder constructs ---

		// Every one of these is a shape whose spans are easy to get wrong: a nested list walks
		// two block levels, a reference link resolves its target elsewhere in the document, a
		// footnote is exported far from where it was written, and multibyte text makes a byte
		// offset differ from a character index.
		for (auto &sample : s_samples) {
			auto sampleDoc = Rc<DocumentMarkdown>::create(pool,
					BytesView(reinterpret_cast<const uint8_t *>(sample.text.data()),
							sample.text.size()),
					StringView("text/markdown"));
			auto label = string::toString<memory::PoolInterface>("markdown sample: ", sample.name);
			if (!sampleDoc || !sampleDoc->getRoot()) {
				check(false, label);
				continue;
			}

			bool ok = true;
			size_t withSpan = 0;
			uint32_t prev = 0;
			foreachNode(*sampleDoc->getRoot()->getRoot(), [&](const document::Node &node, size_t) {
				auto span = node.getSourceSpan();
				if (span.empty()) {
					return;
				}
				++withSpan;
				if (span.end() > sample.text.size()) {
					ok = false;
					return;
				}
				if (node.getHtmlName() == "__value__") {
					auto fragment = sample.text.sub(span.offset, span.length);
					fragment.trimChars<StringView::WhiteSpace>();
					if (fragment.empty()) {
						ok = false;
					}
					if (span.offset < prev) {
						ok = false;
					}
					prev = span.offset;
				}
			});

			check(ok && withSpan > 0, label);
		}

		performMarkdownMarkupTests(pool);
	}, pool);

	memory::pool::destroy(pool);
}

} // namespace STAPPLER_VERSIONIZED stappler
