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

#ifndef TESTS_WINDOW_SRC_DRAG_DRAGTEXTLAYOUT_H_
#define TESTS_WINDOW_SRC_DRAG_DRAGTEXTLAYOUT_H_

#include "app/TestLayout.h"
#include "XLDragSystem.h"
#include "XLUiTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A text field as a drop target - the case the whole shared-offer design exists for.
//
// A field already knows how to receive text: that is what paste() does. Once a drag carries its
// payload in the clipboard's own data model, dropping text and pasting it are not two features but
// one, matched by the same type rule and inserted at the same caret. What this checks is that they
// really did stay one thing, including the parts where they are allowed to differ - a read-only
// field refuses both, and neither invents a type the payload never offered.
class DragTextLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleEnter(Scene *) override;

protected:
	void expect(bool cond, StringView phase, StringView what);
	void expectText(StringView phase, StringView what, StringView expected);

	// A drag carrying `text` under the given MIME types, dropped at the field's centre
	bool dropOn(Node *target, StringView text, SpanView<StringView> types);

	void runPhase1();
	void runPhase2();
	void runPhase3();

	ui::TextInput *_field = nullptr;
	ui::TextInput *_readOnly = nullptr;

	DragSystem *_drag = nullptr;

	size_t _encodes = 0;
	size_t _checks = 0;
	size_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DRAG_DRAGTEXTLAYOUT_H_
