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

#ifndef XENOLITH_RENDERER_UI_INPUT_XLUICODEEDITOR_H_
#define XENOLITH_RENDERER_UI_INPUT_XLUICODEEDITOR_H_

#include "XLUiTextView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* A source file in a monospace editor: line numbers, no wrapping, Tab indents, Ctrl+S saves.

Everything that makes it an editor rather than a text view is configuration, not code — the defaults
set in init() are the whole difference from the output pane of ui::Console, which is the same class
with the opposite answers. What is left here is the file: reading it, writing it back, and the one
hotkey that connects the two.

Read-only is inherited from TextInput and means what it says at this level too: the widget still
takes taps, drag-selection and the copy chord, and saveFile() refuses. A file VIEWER is therefore
this class with setReadOnly(true) and nothing else.

CSS: everything ui::TextView publishes (type `text-input`, class `text-view` and the gutter /
current-line classes), plus the class `code-editor` on the widget. */
class SP_PUBLIC CodeEditor : public TextView {
public:
	virtual ~CodeEditor() = default;

	// Takes a FileInfo rather than a StringView because Sprite has an init(StringView) that loads a
	// texture by name, and an overload taking a StringView here would silently hide it. VectorSprite
	// also has init(const FileInfo &) - an image from a file - so this IS an override, and "the file"
	// just means something else at this level of the hierarchy.
	virtual bool init(const FileInfo &) override;
	virtual bool init() override;

	virtual void handleEnter(Scene *) override;

	virtual bool loadFile(const FileInfo &);
	virtual bool saveFile();

	// The resolved path of the file last loaded, or the one a save was pointed at.
	StringView getPath() const { return _path; }

	// Set by the change callback and cleared by a load or a save.
	bool isDirty() const { return _dirty; }

	virtual bool handleInspectorCommand(StringView action, const Value &args,
			Value &result) override;

	virtual Value encodeState() const override;

protected:
	using TextView::init;

	String _path;
	bool _dirty = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_INPUT_XLUICODEEDITOR_H_
