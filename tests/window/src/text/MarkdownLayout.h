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

#ifndef TESTS_WINDOW_SRC_TEXT_MARKDOWNLAYOUT_H_
#define TESTS_WINDOW_SRC_TEXT_MARKDOWNLAYOUT_H_

#include "app/TestLayout.h"
#include "XLUiMarkdownView.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* The Markdown viewer, on a document that carries one of everything the milestone claims.

What the stand proves is not "text appeared". It is that a block became the node its tag says it
did, that an inline construct became a style range inside its block rather than a node beside it,
and that a paragraph re-wraps when the view is narrowed - the three things the whole design rests
on. `markdown.dump` answers all three in one Value, which is also what the python check reads.

The sample is inline and fixed on purpose: a stand that reads the repository README would report a
different tree every time the README is edited. `markdown.file` loads a real one when a person
wants to look at it. */
class MarkdownLayout : public TestLayout {
public:
	virtual ~MarkdownLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	// What the stand shows with no command given.
	static StringView getSampleSource();

protected:
	virtual void registerCommands() override;

	// The tree as the check reads it: one entry per node, with its type, classes, text extent
	// and the source runs the builder recorded.
	Value encodeTree() const;
	Value encodeFlow() const;
	Value encodeNode(const Node *) const;

	// What is selected, and - the part a check cannot see any other way - which labels actually
	// carry a drawn highlight, where the handles are, and whether the document scrolled.
	Value encodeSelection() const;

	basic2d::Layer *_background = nullptr;
	ui::MarkdownView *_view = nullptr;

	// 0 means "follow the work area"; a command sets it to exercise re-wrapping.
	float _width = 0.0f;

	// The clipboard is asserted on by reading it back, exactly as ClipboardLayout does: the write
	// and the read each cross to the context thread, so a command cannot see its own answer.
	Rc<ClipboardSession> _clipboard;
	size_t _deliveries = 0;
	Value _lastRead;

	// The last link a click followed, so a check can name it.
	String _lastLink;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_TEXT_MARKDOWNLAYOUT_H_
