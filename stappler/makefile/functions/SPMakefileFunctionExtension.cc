/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPFilesystemFile.h"
#include "SPMakefileVariable.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

// Extended (non-GNU) functions for direct file I/O, modeled on GNU make's $(file):
//   $(xl_cat <file>)           -- read <file>; a single trailing newline is removed
//                                 (empty result if the file does not exist)
//   $(xl_write <file>,<text>)  -- create/overwrite <file> with <text>; expands to nothing
//   $(xl_append <file>,<text>) -- append <text> to <file>, creating it if necessary
//
// Paths are resolved against the makefile root, like $(wildcard)/$(realpath). On write and
// append a final newline is added when <text> does not already end with one, so repeated
// $(xl_append ...) calls accumulate one record per line. Like $(shell), these touch the
// filesystem at expansion time and are only safe under the module's trusted-makefile model
// (see the note on the Makefile class in SPMakefile.h); never expand untrusted makefile text.

static bool Function_xl_cat(const Callback<void(StringView)> &out, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto name = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	name.trimChars<StringView::WhiteSpace>();

	auto path = engine.getAbsolutePath(name);
	if (path.empty()) {
		return true; // missing file -> empty result, matching $(file <name) / $(wildcard)
	}

	auto content = filesystem::readTextFile<Interface>(FileInfo{path});

	// drop a single trailing newline (CR/LF pair too), matching GNU make's $(file <name)
	StringView sv(content.data(), content.size());
	if (sv.ends_with("\n")) {
		sv = sv.sub(0, sv.size() - 1);
		if (sv.ends_with("\r")) {
			sv = sv.sub(0, sv.size() - 1);
		}
	}
	out << sv;
	return true;
}

// Resolve the path argument (#0) to an absolute path and the text argument (#1) into `text`,
// appending a final newline when it is missing. Returns false (error reported) when the path
// cannot be resolved.
static bool Function_xl_resolveWrite(VariableEngine &engine, SpanView<StmtValue *> args,
		StringView fnName, StringView &path, String &text) {
	auto err = engine.getCallContext()->err;

	auto name = engine.resolve(args[0], 0, *err);
	name.trimChars<StringView::WhiteSpace>();

	path = engine.getAbsolutePath(name);
	if (path.empty()) {
		err->reportError(toString("$(", fnName, ") cannot resolve path: '", name, "'"));
		return false;
	}

	text = engine.resolve(args[1], 0, *err).str<Interface>();
	if (!StringView(text.data(), text.size()).ends_with("\n")) {
		text.push_back('\n');
	}
	return true;
}

static bool Function_xl_write(const Callback<void(StringView)> &, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	StringView path;
	String text;
	if (!Function_xl_resolveWrite(engine, args, "xl_write", path, text)) {
		return false;
	}

	if (!filesystem::write(FileInfo{path}, reinterpret_cast<const uint8_t *>(text.data()),
				text.size(), true)) {
		engine.getCallContext()->err->reportError(
				toString("$(xl_write) failed to write file: '", path, "'"));
		return false;
	}
	return true; // expands to nothing, like $(file >...)
}

static bool Function_xl_append(const Callback<void(StringView)> &, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	StringView path;
	String text;
	if (!Function_xl_resolveWrite(engine, args, "xl_append", path, text)) {
		return false;
	}

	auto file = filesystem::File::open(FileInfo{path},
			filesystem::OpenFlags::Write | filesystem::OpenFlags::Create
					| filesystem::OpenFlags::Append);
	if (!file) {
		engine.getCallContext()->err->reportError(
				toString("$(xl_append) failed to open file: '", path, "'"));
		return false;
	}

	file.write(reinterpret_cast<const uint8_t *>(text.data()), text.size());
	file.close();
	return true; // expands to nothing, like $(file >>...)
}

static bool Function_xl_mkdir(const Callback<void(StringView)> &, void *, VariableEngine &engine,
		SpanView<StmtValue *> args) {
	auto err = engine.getCallContext()->err;

	auto name = engine.resolve(args[0], 0, *err);
	name.trimChars<StringView::WhiteSpace>();
	auto path = engine.getAbsolutePath(name);
	if (path.empty()) {
		return true; // missing file -> empty result, matching $(file <name) / $(wildcard)
	}

	filesystem::mkdir_recursive(FileInfo{path});

	return true; // expands to nothing
}

// $(xl_make_path <text>) -- force-encode every space inside <text> as the engine's path-space
// placeholder, so a path that contains spaces survives the engine's whitespace word-splitting as a
// single word (and is decoded back to a real space at the shell/filesystem/display boundary). Use it
// to make a space-containing path produced outside the lexer's "\ " escaping safe to handle, e.g. a
// path from $(shell), an environment variable, or a literal:
//   SRC := $(xl_make_path $(MY_DIR_WITH_SPACES)/main.c)
// This is a pure text transform (no filesystem access, no absolute-path resolution); wrap it in
// $(abspath ...) etc. if needed. Surrounding whitespace is trimmed; only interior spaces are encoded.
// Idempotent: an already-encoded path (no remaining real spaces) passes through unchanged.
static bool Function_xl_make_path(const Callback<void(StringView)> &out, void *,
		VariableEngine &engine, SpanView<StmtValue *> args) {
	auto name = engine.resolve(args[0], 0, *engine.getCallContext()->err);
	name.trimChars<StringView::WhiteSpace>();

	mem_std::Interface::StringType storage;
	out << encodePathSpaces(name, storage);
	return true;
}

// $(xl_make_plain <text>) -- inverse of $(xl_make_path): decode every path-space placeholder back to a
// real space, yielding the plain, space-containing text. Use it when a value carrying an encoded path
// (from $(wildcard)/$(realpath)/$(abspath)/$(xl_make_path) or an authored "\ ") must be handed to
// something that expects literal spaces inside make text -- e.g. embedding the path in a message, or
// composing a string for a context that does its own quoting. This is a pure text transform; the result
// is a normal space-separated string and will word-split again if re-expanded unquoted.
static bool Function_xl_make_plain(const Callback<void(StringView)> &out, void *,
		VariableEngine &engine, SpanView<StmtValue *> args) {
	auto name = engine.resolve(args[0], 0, *engine.getCallContext()->err);

	mem_std::Interface::StringType storage;
	out << decodePathSpaces(name, storage);
	return true;
}


} // namespace stappler::makefile
