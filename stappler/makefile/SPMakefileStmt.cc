/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#include "SPMakefileStmt.h"

namespace STAPPLER_VERSIONIZED stappler::makefile {

using PlainStopChars = StringView::Chars<'=', ':', '?', '+'>;

static uint32_t countNewlines(StringView str) {
	uint32_t count = 0;
	while (!str.empty()) {
		auto v = str.readUntil<StringView::Chars<'\n', '\r'>>();
		if (str.is("\r\n")) {
			if (!v.is('\\')) {
				++count;
			}
			str += 2;
		} else if (str.is('\n') || str.is('\r')) {
			if (!v.is('\\')) {
				++count;
			}
			++str;
		}
	}
	return count;
}

Keyword Stmt::getKeyword(StringView str) {
	if (str == "override") {
		return Keyword::Override;
	} else if (str == "include") {
		return Keyword::Include;
	} else if (str == "-include" || str == "sinclude") {
		return Keyword::IncludeOptional;
	} else if (str == "define") {
		return Keyword::Define;
	} else if (str == "undefine") {
		return Keyword::Undefine;
	} else if (str == "endef") {
		return Keyword::Endef;
	} else if (str == "ifdef") {
		return Keyword::Ifdef;
	} else if (str == "ifndef") {
		return Keyword::Ifndef;
	} else if (str == "ifeq") {
		return Keyword::Ifeq;
	} else if (str == "ifneq") {
		return Keyword::Ifneq;
	} else if (str == "else") {
		return Keyword::Else;
	} else if (str == "endif") {
		return Keyword::Endif;
	}
	return Keyword::None;
}

char Stmt::getBeginChar(ReadContext ctx) {
	switch (ctx) {
	case ReadContext::Expansion: return '(';
	case ReadContext::MultilineExpansion: return '('; break;
	case ReadContext::ConditionalQuoted: return '\''; break;
	case ReadContext::ConditionalDoubleQuoted: return '"'; break;
	default: break;
	}
	return 0;
}

char Stmt::getEndChar(ReadContext ctx) {
	switch (ctx) {
	case ReadContext::Expansion: return ')'; break;
	case ReadContext::MultilineExpansion: return ')'; break;
	case ReadContext::ConditionalQuoted: return '\''; break;
	case ReadContext::ConditionalDoubleQuoted: return '"'; break;
	default: break;
	}
	return 0;
}

StringView Stmt::getOperator(StringView str, bool allowRule) {
	if (str.is(":::=")) {
		return str.sub(0, ":::="_len);
	} else if (str.is("::=")) {
		return str.sub(0, "::="_len);
	} else if (str.is(":=")) {
		return str.sub(0, ":="_len);
	} else if (str.is("=")) {
		return str.sub(0, "="_len);
	} else if (str.is("?=")) {
		return str.sub(0, "?="_len);
	} else if (str.is("+=")) {
		return str.sub(0, "+="_len);
	} else if (allowRule && str.is(":")) {
		return str.sub(0, ":"_len);
	}
	return StringView();
}

bool Stmt::isWhitespace(const StringView &str) {
	if (str.is<StringView::WhiteSpace>()) {
		return true;
	} else if (str.is('\\')) {
		auto s = str.sub(1, 1);
		if (s.is< StringView::Chars<'\n', '\r'>>()) {
			return true;
		}
	}
	return false;
}

StringView Stmt::skipWhitespace(StringView &str) {
	StringView tmp = str;
	do {
		str.skipChars<StringView::WhiteSpace>();
		if (str.is('\\')) {
			auto s = str.sub(1, 1);
			if (s.is< StringView::Chars<'\n', '\r'>>()) {
				++str;
			}
		}
	} while (str.is<StringView::WhiteSpace>());
	return StringView(tmp.data(), str.data() - tmp.data());
}

StringView Stmt::readLine(StringView &str, ErrorReporter &err) {
	auto line = str.readUntil<StringView::Chars<'\n', '\r'>>();
	auto start = line.data();
	while (line.ends_with("\\")) {
		auto nl = str.readChars<StringView::Chars<'\n', '\r'>>();
		if (nl != "\r" && nl != "\n" && nl != "\r\n") {
			// multiple newlines, break
			line = line.sub(0, line.size() - 1);
			break;
		}
		++err.lineSize;
		line = str.readUntil<StringView::Chars<'\n', '\r'>>();
	}
	return StringView(start, (line.data() + line.size()) - start);
}

static StringView readContextIdentifier(StringView &str, ReadContext ctx) {
	switch (ctx) {
	case ReadContext::LineStart:
		return str.readUntil<StringView::WhiteSpace,
				StringView::Chars<'#', ',', ')', ':', '=', '?', '+', '$', '\\'>>();
		break;
	case ReadContext::Expansion:
		// '(' is a stop char so that literal parentheses inside $(...) can be balanced
		// against ')' instead of letting an inner ')' terminate the expansion early
		return str.readUntil<StringView::WhiteSpace,
				StringView::Chars<'#', ',', '(', ')', '$', '\\'>>();
		break;
	case ReadContext::LineEnd:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'#', '$', '\\'>>();
		break;
	case ReadContext::TrailingRecipe:
		// A recipe line is expanded and passed verbatim to the shell; '#' is NOT a make comment
		// here (the shell handles it), so it stays literal — including inside quotes, where make has
		// no cross-token quote state to recognize it otherwise.
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'$', '\\'>>();
		break;
	case ReadContext::Multiline:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'$', '\\'>>();
		break;
	case ReadContext::MultilineExpansion:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<',', '(', ')', '$', '\\'>>();
		break;
	case ReadContext::ConditionalQuoted:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'#', '$', '\\', '\''>>();
		break;
	case ReadContext::ConditionalDoubleQuoted:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'#', '$', '\\', '"'>>();
		break;
	case ReadContext::PrerequisiteList:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'#', '$', '\\', '|', ';'>>();
	case ReadContext::OrderOnlyList:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'#', '$', '\\', ';'>>();
		break;
	}
	return StringView();
}

Stmt *Stmt::readWord(StringView &str, ReadContext ctx, ErrorReporter &err, uint32_t &nestedDepth) {
	Stmt *stmt = nullptr;

	auto beginning = getBeginChar(ctx);
	auto ending = getEndChar(ctx);

	auto makeStmt = [&]() -> Stmt * {
		if (!stmt) {
			stmt = new (sprt::nothrow) Stmt(err);
			stmt->type = StmtType::Word;
		}
		return stmt;
	};

	ReadContext expType = ReadContext::Expansion;
	if (ctx == ReadContext::Multiline || ctx == ReadContext::MultilineExpansion) {
		expType = ReadContext::MultilineExpansion;
	}

	bool isMultiline = false;
	if (ctx == ReadContext::Multiline || ctx == ReadContext::MultilineExpansion) {
		isMultiline = true;
	}

	while (!str.empty() && !str.is<StringView::WhiteSpace>()) {
		err.setPos(str);

		uint32_t inPrefix = 0;
		if (beginning == '(') {
			while (str.is(beginning)) {
				++inPrefix;
				++str;
			}
		}

		StringView sig = readContextIdentifier(str, ctx);

		if (inPrefix > 0) {
			sig = StringView(sig.data() - inPrefix, sig.size() + inPrefix);
			nestedDepth += inPrefix;
		}

		uint32_t inSuffix = 0;
		while (nestedDepth > 0 && str.is(ending)) {
			++str;
			++inSuffix;
			--nestedDepth;
		}

		if (inSuffix > 0) {
			sig = StringView(sig.data(), sig.size() + inSuffix);
		}

		if (str.is<StringView::WhiteSpace>()) {
			makeStmt()->add(sig);
			break;
		} else if (str.is('#')) {
			if (!sig.ends_with('\\')) {
				makeStmt()->add(sig);
				if (ending) {
					err.setPos(str);
					err.reportError("Unexpected line ending, ')' expected");
				}
				break;
			} else {
				sig = sig.sub(0, sig.size() - 1);
				makeStmt()->add(sig);
				makeStmt()->add(str.sub(0, 1));
				++str;
			}
		} else if (str.is('$')) {
			makeStmt()->add(sig);

			++str;
			if (str.is('(')) {
				err.setPos(str);
				auto stmt = readScoped(str, StmtType::Expansion, expType, err);
				if (stmt) {
					makeStmt()->add(stmt);
				} else {
					return nullptr;
				}
			} else {
				if (isWhitespace(str)) {
					if (isMultiline) {
						auto nl = countNewlines(skipWhitespace(str));
						if (nl > 0) {
							auto stmt = new (sprt::nothrow) Stmt(err, StmtType::Expansion, "\n");
							makeStmt()->add(stmt);
						} else {
							auto stmt = new (sprt::nothrow) Stmt(err, StmtType::Expansion, " ");
							makeStmt()->add(stmt);
						}
					} else {
						skipWhitespace(str);
						auto stmt = new (sprt::nothrow) Stmt(err, StmtType::Expansion, " ");
						makeStmt()->add(stmt);
					}

				} else {
					auto stmt = new (sprt::nothrow) Stmt(err, StmtType::Expansion, str.sub(0, 1));
					makeStmt()->add(stmt);
					++str;
				}
			}
		} else if (beginning == '(' && str.is(beginning)) {
			// a literal '(' inside $(...): keep it in the word and bump the nesting depth so
			// its matching ')' is treated as balanced text rather than ending the expansion
			makeStmt()->add(StringView(sig.data(), sig.size() + 1));
			++nestedDepth;
			++str;
		} else if (ending && str.is(ending)) {
			makeStmt()->add(sig);
			break;
		} else if (ctx == ReadContext::PrerequisiteList && str.is('|')) {
			makeStmt()->add(sig);
			break;
		} else if (ctx == ReadContext::PrerequisiteList && str.is(';')) {
			makeStmt()->add(sig);
			break;
		} else if (ctx == ReadContext::OrderOnlyList && str.is(';')) {
			makeStmt()->add(sig);
			break;
		} else if (str.is(',')) {
			makeStmt()->add(sig);
			break;
		} else if (str.is('\\')) {
			if (isWhitespace(str)) {
				makeStmt()->add(sig);
				break;
			} else {
				makeStmt()->add(StringView(sig.data(), sig.size() + 1));
				++str;
			}
		} else if (ctx == ReadContext::LineStart && str.is<PlainStopChars>()
				&& !Stmt::getOperator(str, true).empty()) {
			makeStmt()->add(sig);
			break;
		} else if (!str.empty()) {
			makeStmt()->add(StringView(sig.data(), sig.size() + 1));
			++str;
			if (beginning != '(') {
				break;
			}
			// inside $(...): keep reading the text that follows a balanced "(...)" (the
			// char consumed above was exposed after the closing ')' of a nested group)
		} else {
			makeStmt()->add(sig);
			if (ending) {
				err.setPos(str);
				err.reportError("Unexpected line ending, ')' expected");
			}
		}
	}
	return stmt;
}

Stmt *Stmt::readScoped(StringView &str, StmtType type, ReadContext ctx, ErrorReporter &err) {
	Stmt *stmt = nullptr;

	auto beginning = getBeginChar(ctx);
	auto ending = getEndChar(ctx);

	auto addStmtWord = [&](Stmt *s) {
		if (!stmt) {
			stmt = new (sprt::nothrow) Stmt(err);
			stmt->type = type;
			stmt->add(new (sprt::nothrow) StmtValue(s));
		} else if (stmt->type == type) {
			stmt->add(new (sprt::nothrow) StmtValue(s));
		} else if (stmt->type == StmtType::ArgumentList) {
			stmt->tail->stmt->add(s);
		}
	};

	auto addStringWord = [&](StringView s) {
		if (!stmt) {
			stmt = new (sprt::nothrow) Stmt(err);
			stmt->type = type;
			stmt->add(new (sprt::nothrow) StmtValue(s));
		} else if (stmt->type == type) {
			stmt->add(new (sprt::nothrow) StmtValue(s));
		} else if (stmt->type == StmtType::ArgumentList) {
			stmt->tail->stmt->add(s);
		}
	};

	auto addStmtArgument = [&](Stmt *s) {
		if (!stmt) {
			stmt = new (sprt::nothrow) Stmt(err);
			stmt->type = StmtType::ArgumentList;
			stmt->add(new (sprt::nothrow) StmtValue(s));
		} else if (stmt->type == StmtType::ArgumentList) {
			stmt->add(new (sprt::nothrow)
							StmtValue(new (sprt::nothrow) Stmt(err, StmtType::WordList, s)));
		} else {
			if (stmt->tail != stmt->value) {
				auto firstArg = stmt->value->next;
				auto lastArg = stmt->tail;
				stmt->tail = stmt->value;
				stmt->value->next = nullptr;
				stmt->type = StmtType::WordList;
				stmt = new (sprt::nothrow) Stmt(err, StmtType::ArgumentList, stmt);
				stmt->add(new (sprt::nothrow) StmtValue(
						new (sprt::nothrow) Stmt(err, StmtType::WordList, firstArg, lastArg)));
			} else {
				stmt = new (sprt::nothrow) Stmt(err, StmtType::ArgumentList, stmt);
			}

			stmt->add(new (sprt::nothrow)
							StmtValue(new (sprt::nothrow) Stmt(err, StmtType::WordList, s)));
		}
	};

	if (beginning != 0) {
		if (!str.is(beginning)) {
			err.reportError(toString("Expected '", beginning, "'"));
			return nullptr;
		}

		++str;
	}

	bool nextArgument = false;

	bool isMultiline = false;
	if (ctx == ReadContext::Multiline || ctx == ReadContext::MultilineExpansion) {
		isMultiline = true;
	}

	if (ctx == ReadContext::Multiline) {
		// preserve the whole leading whitespace run (newlines + indentation) verbatim so
		// `define` values round-trip exactly (recipe tabs survive for $(eval))
		auto ws = skipWhitespace(str);
		if (!ws.empty()) {
			addStringWord(ws);
		}
	} else if (isMultiline) {
		auto nl = countNewlines(skipWhitespace(str));
		for (uint32_t i = 0; i < nl; ++i) { addStringWord("\n"); }
	} else {
		skipWhitespace(str);
	}

	// try single-word expansion
	if (ctx == ReadContext::Expansion) {
		StringView tmp = str;
		auto sig = readContextIdentifier(tmp, ctx);
		if (ending && tmp.is(ending)) {
			++tmp;
			stmt = new (sprt::nothrow) Stmt(err, StmtType::Expansion, sig);
			str = tmp;
			return stmt;
		}
	}

	auto guard = str;
	StringView whiteSpace;
	uint32_t nestedDepth = 0;
	// Within a function argument, literal whitespace is preserved verbatim, but a '\'-newline
	// line continuation collapses to a single space (GNU make). Apply this to the whitespace
	// emitted around commas and before ')'.
	auto wsToken = [](StringView ws) -> StringView {
		return (ws.find('\\') != maxOf<size_t>()) ? StringView(" ") : ws;
	};
	while (!str.empty() && (ending == 0 || !str.is(ending))) {
		auto first = str.front();
		auto wordStmt = readWord(str, ctx, err, nestedDepth);
		if (!wordStmt) {
			if (ending == 0) {
				break;
			}

			return nullptr;
		}

		// The whitespace between a function name and its first argument is the call
		// separator, which GNU make strips entirely (`$(firstword \<nl>\t$(X))` -> first
		// word of X, not the empty string). That transition is the very first word being
		// added to an Expansion (only the name is present so far), so skip the preserve
		// logic there; everywhere else a \t\n run between two $stmt is kept.
		bool nameToFirstArg = (type == StmtType::Expansion && stmt && stmt->value == stmt->tail);
		if (!whiteSpace.empty() && first == '$' && !nameToFirstArg) {
			// special case: whitespace token between two $stmt, preserve it if it has \t\n
			auto tmp = whiteSpace;
			tmp.skipUntil<StringView::Chars<'\t', '\n'>>();
			if (!tmp.empty()) {
				// a '\' in the run is a backslash-newline line continuation, which GNU make
				// collapses to a single space; only a literal tab/newline is kept verbatim
				if (whiteSpace.find('\\') != maxOf<size_t>()) {
					addStringWord(" ");
				} else {
					addStringWord(whiteSpace);
				}
			}
		}

		if (nextArgument) {
			addStmtArgument(wordStmt);
		} else {
			addStmtWord(wordStmt);
		}

		whiteSpace = skipWhitespace(str);

		if (ctx == ReadContext::Multiline) {
			// store the whitespace run verbatim (the verbatim resolve path emits it as-is)
			if (!whiteSpace.empty()) {
				addStringWord(whiteSpace);
			}
			whiteSpace = StringView(); // consumed above
		} else if (isMultiline) {
			auto nl = countNewlines(whiteSpace);
			for (uint32_t i = 0; i < nl; ++i) { addStringWord("\n"); }
			if (nl > 0) {
				whiteSpace = StringView(); // prevent to preserve already added whitespace
			}
		}

		if (!isMultiline && ctx != ReadContext::TrailingRecipe && str.is('#')) {
			// (in a recipe '#' is literal, not a make comment — see readContextIdentifier)
			if (ending) {
				err.setPos(str);
				err.reportError(toString("Unexpected line ending, '", ending, "' expected"));
			}
			break;
		} else if (ctx == ReadContext::PrerequisiteList && str.is('|')) {
			break;
		} else if (ctx == ReadContext::PrerequisiteList && str.is(';')) {
			break;
		} else if (ctx == ReadContext::OrderOnlyList && str.is(';')) {
			break;
		} else if ((ctx == ReadContext::Expansion || ctx == ReadContext::MultilineExpansion)
				&& str.is(',')) {
			// GNU make keeps whitespace within a function argument verbatim. Keep the current
			// argument's trailing whitespace (the name/first-arg separator is dropped by the split)...
			if (!whiteSpace.empty()) {
				addStringWord(wsToken(whiteSpace));
			}
			whiteSpace = StringView();
			// ...then consume this comma (and any consecutive ones) and open the next argument(s),
			// folding the whitespace after each comma into the argument it introduces. Handling the
			// run of commas here (rather than letting the main loop re-enter on a ',') avoids the
			// empty word readWord() would produce at a ',', which would corrupt argument boundaries.
			for (;;) {
				++str;
				auto leadWs = skipWhitespace(str);
				if (str.empty() || (ending && str.is(ending))) {
					// trailing argument, e.g. $(patsubst a,b,) or $(call f,a, ): the run after the comma
					addStmtArgument(new (sprt::nothrow) Stmt(err, wsToken(leadWs)));
					break;
				} else if (str.is(',')) {
					// a whitespace-only (or empty) argument between two commas, e.g. $(call f,a, ,b)
					addStmtArgument(new (sprt::nothrow) Stmt(err, wsToken(leadWs)));
				} else if (leadWs.empty()) {
					nextArgument = true; // a real word starts the next argument
					break;
				} else {
					// Open the next argument and add its leading whitespace as a STRING token, so resolve
					// keeps it without inserting a synthetic separator before the first word (a whitespace
					// token wrapped as a Stmt would not set the "ends in space" flag resolve relies on).
					addStmtArgument(new (sprt::nothrow) Stmt(err, StringView()));
					addStringWord(wsToken(leadWs));
					nextArgument = false;
					break;
				}
			}
		} else if (ctx == ReadContext::LineStart && str.is<PlainStopChars>()) {
			auto op = Stmt::getOperator(str, true);
			if (!op.empty()) {
				break;
			} else {
				err.setPos(str);
				err.reportError("Unexpected chars in plain string");
				break;
			}
		} else if (ending && str.is(ending)) {
			if (nestedDepth > 0) {
				// drop nested ")" as string
				uint32_t counter = 0;
				auto d = str.data();
				while (nestedDepth > 0 && str.is(ending)) {
					--nestedDepth;
					++counter;
					++str;
				}
				addStringWord(StringView(d, counter));
			} else if ((ctx == ReadContext::Expansion || ctx == ReadContext::MultilineExpansion)
					&& stmt && stmt->type == StmtType::ArgumentList) {
				if (!whiteSpace.empty()) {
					addStringWord(wsToken(whiteSpace));
				}
			}
		} else {
			nextArgument = false;
		}
		if (str.data() == guard.data()) {
			slog().error("makefile::Stmt", "No forward progress on exception parsing, exiting");
			++str;
			break;
		}
		guard = str;
	}

	whiteSpace = skipWhitespace(str);

	if (ctx == ReadContext::Multiline) {
		if (!whiteSpace.empty()) {
			addStringWord(whiteSpace);
		}
		whiteSpace = StringView(); // consumed above
	} else if (isMultiline) {
		auto nl = countNewlines(whiteSpace);
		for (uint32_t i = 0; i < nl; ++i) { addStringWord("\n"); }
	}

	if (ending && str.is(ending)) {
		if ((ctx == ReadContext::Expansion || ctx == ReadContext::MultilineExpansion) && stmt
				&& stmt->type == StmtType::ArgumentList) {
			if (!whiteSpace.empty()) {
				addStringWord(wsToken(whiteSpace));
			}
		}
		++str;
	}

	if (stmt && ctx == ReadContext::Multiline) {
		stmt->multiline = true;
	}

	return stmt;
}

Stmt::Stmt(const FileLocation &l) : loc(l) { }

Stmt::Stmt(const FileLocation &l, StringView str) : type(StmtType::Word), loc(l) {
	tail = value = new (sprt::nothrow) StmtValue(str);
}

Stmt::Stmt(const FileLocation &l, StmtType t, StringView str) : type(t), loc(l) {
	tail = value = new (sprt::nothrow) StmtValue(str);
}

Stmt::Stmt(const FileLocation &l, StmtType t, Stmt *stmt) : type(t), loc(l) {
	tail = value = new (sprt::nothrow) StmtValue(stmt);
}

Stmt::Stmt(const FileLocation &l, StmtType _t, StmtValue *v, StmtValue *t)
: type(_t), value(v), tail(t), loc(l) { }

StmtValue *Stmt::add(StmtValue *val) {
	if (!tail) {
		value = tail = val;
	} else {
		tail->next = val;
		tail = val;
	}
	return tail;
}

StmtValue *Stmt::add(StmtValue *val, StmtValue *last) {
	if (!tail) {
		value = val;
		tail = last;
	} else {
		tail->next = val;
		tail = last;
	}
	return tail;
}

void Stmt::add(StringView str) {
	if (str.empty()) {
		return;
	}

	if (type == StmtType::Word && tail && !tail->isStmt) {
		// merge words if possible
		if (tail->str.data() + tail->str.size() == str.data()) {
			tail->str = StringView(tail->str.data(), tail->str.size() + str.size());
			return;
		}
	}
	add(new (sprt::nothrow) StmtValue(str));
}

void Stmt::add(Stmt *stmt) { add(new (sprt::nothrow) StmtValue(stmt)); }

void Stmt::describe(const Callback<void(StringView)> &out, uint32_t level) {
	if (level == 0) {
		loc.describe(out);
	}

	for (uint32_t i = 0; i < level; ++i) { out << '\t'; }
	switch (type) {
	case StmtType::Word: out << "Word\n"; break;
	case StmtType::WordList: out << "WordList\n"; break;
	case StmtType::ArgumentList: out << "ArgumentList\n"; break;
	case StmtType::Expansion: out << "Expansion\n"; break;
	}
	auto v = value;
	while (v) {
		if (v->isStmt) {
			v->stmt->describe(out, level + 1);
		} else {
			for (uint32_t i = 0; i < level + 1; ++i) { out << '\t'; }
			out << '"' << v->str << '"' << "\n";
		}
		v = v->next;
	}
}

void Stmt::describe(uint32_t level) {
	describe([](StringView str) { sprt::cout << str; }, level);
}

} // namespace stappler::makefile
