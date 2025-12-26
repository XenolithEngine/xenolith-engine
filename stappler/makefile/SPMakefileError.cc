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

#include "SPMakefileStmt.h"
#include "SPMakefileBlock.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

LineOffset FileLocation::makeLineOffset() {
	LineOffset off;
	uint32_t startString = 0;
	auto tmp = StringView(line, pos);
	while (!tmp.empty()) {
		tmp.skipUntil<StringView::Chars<'\n', '\r'>>();
		if (tmp.is<StringView::Chars<'\n', '\r'>>()) {
			tmp.skipChars<StringView::Chars<'\n', '\r'>>();
			startString = static_cast<uint32_t>(tmp.data() - line.data());
			++off.lineOffset;
		}
	}

	off.pos = pos - startString;
	off.selected = StringView(line, startString, maxOf<size_t>())
						   .readUntil<StringView::Chars<'\n', '\r'>>();
	return off;
}

void FileLocation::describe(const Callback<void(StringView)> &cb) {
	cb << filename << ":" << lineno << "\n";
	auto off = makeLineOffset();

	auto lsize = sprt::itoa(uint64_t(lineno), (char *)nullptr, 0);

	String outTmp;
	outTmp.resize(off.pos + lsize, ' ');

	cb << lineno << ": " << off.selected << "\n> " << outTmp << "^\n";
}

ErrorReporter::ErrorReporter(ErrorReporter *err) {
	outer = err;
	if (err) {
		callback = err->callback;
		ref = err->ref;
	}
}

ErrorReporter::ErrorReporter(const FileLocation &loc, ErrorReporter *err) {
	filename = loc.filename;
	line = loc.line;
	lineno = loc.lineno;
	pos = loc.pos;
	outer = err;

	callback = err->callback;
	ref = err->ref;
}

void ErrorReporter::setPos(const StringView &str) {
	pos = static_cast<uint32_t>(str.data() - line.data());
}

void ErrorReporter::reportError(StringView str, Stmt *stmt, Block *block, bool showSource) {
	report(log::LogType::Error, str, stmt, block, showSource);
	incrementErrors();
}

void ErrorReporter::reportWarning(StringView str, Stmt *stmt, Block *block, bool showSource) {
	report(log::LogType::Warn, str, stmt, block, showSource);
	incrementWarnings();
}

void ErrorReporter::reportInfo(StringView str, Stmt *stmt, Block *block, bool showSource) {
	report(log::LogType::Info, str, stmt, block, showSource);
}

void ErrorReporter::report(log::LogType type, StringView str, Stmt *stmt, Block *block,
		bool showSource) {
	auto off = makeLineOffset();

	auto l = lineno + off.lineOffset;
	auto lsize = sprt::itoa(uint64_t(l), (char *)nullptr, 0);

	String outTmp;
	outTmp.resize(off.pos + lsize, ' ');

	if (stmt) {
		StringStream out;
		out << filename << ":" << l << ": " << str << "\n"
			<< l << ": " << off.selected << "\n> " << outTmp << "^\n";
		out << "Located in: ";
		stmt->loc.describe([&](StringView s) { out << s; });
		if (callback) {
			callback(ref, type, out.weak());
		} else {
			log::text(type, "Makefile", out.weak());
		}
	} else if (block) {
		StringStream out;
		out << filename << ":" << l << ": " << str << "\n"
			<< l << ": " << off.selected << "\n> " << outTmp << "^\n";
		out << "Started at: ";
		block->loc.describe([&](StringView s) { out << s; });
		if (callback) {
			callback(ref, type, out.weak());
		} else {
			log::text(type, "Makefile", out.weak());
		}
	} else {
		auto err = toString(filename, ":", l, ": ", str, "\n", l, ": ", off.selected, "\n> ",
				outTmp, "^");
		if (callback) {
			callback(ref, type, err);
		} else {
			log::text(type, "Makefile", err);
		}
	}

	if (showSource) {
		auto tmp = outer;
		while (tmp) {
			off = tmp->makeLineOffset();

			l = tmp->lineno + off.lineOffset;
			lsize = sprt::itoa(uint64_t(l), (char *)nullptr, 0);

			outTmp.resize(off.pos + lsize, ' ');

			auto err = toString("Expanded from: ", tmp->filename, ":", l, "\n", l, ": ",
					off.selected, "\n> ", outTmp, "^");
			if (callback) {
				callback(ref, type, err);
			} else {
				log::text(type, "Makefile", err);
			}
			tmp = tmp->outer;
		}
	}
}

void ErrorReporter::incrementErrors() {
	++nerrors;
	if (outer) {
		outer->incrementErrors();
	}
}

void ErrorReporter::incrementWarnings() {
	++nwarnings;
	if (outer) {
		outer->incrementWarnings();
	}
}

} // namespace stappler::makefile
