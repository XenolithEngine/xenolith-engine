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

#include "XLUiCodeEditor.h"

#include "SPFilesystem.h"
#include "XLHotkey.h"
#include "XLInputListener.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool CodeEditor::init(const FileInfo &file) {
	// The file-less init does the widget; this overload only adds the file, so an editor opened
	// empty and one opened on a path are configured by exactly the same code.
	if (!init()) {
		return false;
	}
	return loadFile(file);
}

bool CodeEditor::init() {
	if (!TextView::init()) {
		return false;
	}

	setName("code-editor");
	addStyleClass("code-editor");

	setGutterVisible(true);
	setWordWrap(false);
	setCurrentLineHighlight(true);
	setTabInsertsIndent(true);

	setCallback([this](StringView) { _dirty = true; });

	// A Ctrl chord is already declined by the runtime's text-input processor, so this reaches the
	// scene even while the field holds the IME — no reserved-key filter needed.
	auto save = HotkeyRegistry::getInstance()->add("xenolith.ui.editor.save",
			HotkeyCombo::parse("Ctrl+S"), "Save the file open in the code editor");
	_listener->addHotkey(save, [this](HotkeyId, const InputEvent &) { return saveFile(); },
			HotkeyFlags::FocusedOnly);

	return true;
}

void CodeEditor::handleEnter(Scene *scene) {
	TextView::handleEnter(scene);

	addInspectorCommand(scene, "editor.state", "Full state of the code editor");
	addInspectorCommand(scene, "editor.set-text", "Replace the whole text: {text}");
	addInspectorCommand(scene, "editor.insert", "Insert {text} at character {at}");
	addInspectorCommand(scene, "editor.set-cursor", "Set the cursor to {start, length}");
	addInspectorCommand(scene, "editor.select",
			"Select from {line, column} to {endLine, endColumn}");
	addInspectorCommand(scene, "editor.select-all", "Select the whole document");
	addInspectorCommand(scene, "editor.copy", "Copy the selection to the clipboard");
	addInspectorCommand(scene, "editor.cut", "Cut the selection to the clipboard");
	addInspectorCommand(scene, "editor.paste", "Start an asynchronous paste at the caret");
	addInspectorCommand(scene, "editor.goto", "Move the caret to {line, column} and scroll to it");
	addInspectorCommand(scene, "editor.scroll", "Set the scroll offset to {x, y}");
	addInspectorCommand(scene, "editor.scroll-to-line", "Scroll to the logical {line}");
	addInspectorCommand(scene, "editor.scroll-to-end", "Scroll to the end of the document");
	addInspectorCommand(scene, "editor.wrap", "Turn word wrap {value} on or off");
	addInspectorCommand(scene, "editor.gutter", "Turn the line-number gutter {value} on or off");
	addInspectorCommand(scene, "editor.caret-blink", "Turn the caret blink {value} on or off");
	addInspectorCommand(scene, "editor.focus", "Acquire text input");
	addInspectorCommand(scene, "editor.key",
			"Apply a motion key: {name: UP|DOWN|LEFT|RIGHT|PAGE_UP|PAGE_DOWN|HOME|END, shift}");
	addInspectorCommand(scene, "editor.lines", "List logical lines: {offset, limit} (0, 40)");
	addInspectorCommand(scene, "editor.doc-selftest",
			"Run the ui::TextDocument index arithmetic against synthetic documents");
	addInspectorCommand(scene, "editor.load", "Load the file at {path}");
	addInspectorCommand(scene, "editor.save", "Write the text back to {path} or the loaded path");
}

bool CodeEditor::loadFile(const FileInfo &file) {
	auto data = filesystem::readTextFile<Interface>(file);
	if (data.empty() && !filesystem::exists(file)) {
		log::source().warn("ui::CodeEditor", "No such file: ", file.path);
		return false;
	}

	// No size cut: the block model renders a window of the document and the IME only ever sees
	// a window of it, so the file's size is bounded by memory, not by any layout ceiling.

	// filesystem::readTextFile resolves the category; keep the resolved path so a save goes back to
	// the same file rather than to a bundle-relative name that no longer resolves the same way.
	auto resolved = filesystem::findPath<Interface>(file);
	_path = resolved.empty() ? file.path.str<Interface>() : resolved;

	// No handler is running yet on a freshly built editor, so this takes setText's local-write path
	// and lands immediately instead of waiting for a platform echo.
	setText(data);
	_dirty = false;

	getView()->setScrollOffset(Vec2::ZERO);
	setCursor(TextCursor(0));

	return true;
}

bool CodeEditor::saveFile() {
	// A read-only editor is a VIEWER, and Ctrl+S in one must not write the file back — the text is
	// the file's own, so the write would be a no-op on a good day and a truncation on a bad one.
	if (_readOnly || _path.empty()) {
		return false;
	}

	auto text = getText();
	if (!filesystem::write(FileInfo{_path}, reinterpret_cast<const uint8_t *>(text.data()),
				text.size())) {
		log::source().error("ui::CodeEditor", "Failed to write ", _path);
		return false;
	}

	_dirty = false;
	return true;
}

Value CodeEditor::encodeState() const {
	auto ret = TextView::encodeState();
	ret.setString(_path, "path");
	ret.setBool(_dirty, "dirty");
	return ret;
}

bool CodeEditor::handleInspectorCommand(StringView action, const Value &args, Value &result) {
	// Before the base: "editor.load" would otherwise never be reached, because the base matches
	// commands by suffix and has no case for it at all — but "editor.save" and "select" do overlap
	// with nothing, so the ordering is about keeping the file cases together rather than about
	// ambiguity.
	if (action.ends_with("load")) {
		return loadFile(FileInfo{args.getString("path")});
	} else if (action.ends_with("save")) {
		const auto &path = args.getString("path");
		if (!path.empty()) {
			_path = path;
		}
		return saveFile();
	}

	return TextView::handleInspectorCommand(action, args, result);
}

} // namespace stappler::xenolith::ui
