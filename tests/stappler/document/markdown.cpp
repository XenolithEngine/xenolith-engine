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
		struct Sample {
			StringView name;
			StringView text;
		};

		const Sample samples[] = {
			{"nested list", "# T\n\n- one\n    - inner one\n    - inner two\n- two\n"},
			{"reference link",
				"# T\n\nSee [the site][ref] for more.\n\n[ref]: https://example.org\n"},
			{"footnote", "# T\n\nA claim[^note].\n\n[^note]: The evidence.\n"},
			{"entities and dashes", "# T\n\nA &amp; B -- see \"quotes\" and 3 < 4.\n"},
			{"multibyte",
				"# Заголовок\n\nТекст с **выделением** и emoji \xf0\x9f\x8e\x89 внутри.\n"},
			{"indented code", "# T\n\n    int x = 0;\n    return x;\n"},
			{"setext header", "Title\n=====\n\nBody text.\n"},
			{"task list", "# T\n\n- [ ] open item\n- [x] done item\n"},
		};

		for (auto &sample : samples) {
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
	}, pool);

	memory::pool::destroy(pool);
}

} // namespace STAPPLER_VERSIONIZED stappler
