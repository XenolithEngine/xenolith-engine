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

The editor behaviour is the TextView configuration set in init(); this class adds loading, saving
and the save hotkey. With setReadOnly(true) it is a file viewer: selection and copy still work,
saveFile() refuses.

CSS: everything ui::TextView publishes (type `text-input`, class `text-view` and the gutter /
current-line classes), plus the class `code-editor` on the widget. */
class SP_PUBLIC CodeEditor : public TextView {
public:
	virtual ~CodeEditor() = default;

	// Loads a source file. Takes FileInfo to override VectorSprite::init(const FileInfo &); a
	// StringView overload would hide Sprite::init(StringView).
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
