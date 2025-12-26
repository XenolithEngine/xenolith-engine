/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'#', ',', ')', '$', '\\'>>();
		break;
	case ReadContext::LineEnd:
	case ReadContext::TrailingRecipe:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'#', '$', '\\'>>();
		break;
	case ReadContext::Multiline:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<'$', '\\'>>();
		break;
	case ReadContext::MultilineExpansion:
		return str.readUntil<StringView::WhiteSpace, StringView::Chars<',', ')', '$', '\\'>>();
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

Stmt *Stmt::readWord(StringView &str, ReadContext ctx, ErrorReporter &err) {
	Stmt *stmt = nullptr;

	auto ending = getEndChar(ctx);

	auto makeStmt = [&]() -> Stmt * {
		if (!stmt) {
			stmt = new (std::nothrow) Stmt(err);
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
		StringView sig = readContextIdentifier(str, ctx);

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
							auto stmt = new (std::nothrow) Stmt(err, StmtType::Expansion, "\n");
							makeStmt()->add(stmt);
						} else {
							auto stmt = new (std::nothrow) Stmt(err, StmtType::Expansion, " ");
							makeStmt()->add(stmt);
						}
					} else {
						skipWhitespace(str);
						auto stmt = new (std::nothrow) Stmt(err, StmtType::Expansion, " ");
						makeStmt()->add(stmt);
					}

				} else {
					auto stmt = new (std::nothrow) Stmt(err, StmtType::Expansion, str.sub(0, 1));
					makeStmt()->add(stmt);
					++str;
				}
			}
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
			break;
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
			stmt = new (std::nothrow) Stmt(err);
			stmt->type = type;
			stmt->add(new (std::nothrow) StmtValue(s));
		} else if (stmt->type == type) {
			stmt->add(new (std::nothrow) StmtValue(s));
		} else if (stmt->type == StmtType::ArgumentList) {
			stmt->tail->stmt->add(s);
		}
	};

	auto addStringWord = [&](StringView s) {
		if (!stmt) {
			stmt = new (std::nothrow) Stmt(err);
			stmt->type = type;
			stmt->add(new (std::nothrow) StmtValue(s));
		} else if (stmt->type == type) {
			stmt->add(new (std::nothrow) StmtValue(s));
		} else if (stmt->type == StmtType::ArgumentList) {
			stmt->tail->stmt->add(s);
		}
	};

	auto addStmtArgument = [&](Stmt *s) {
		if (!stmt) {
			stmt = new (std::nothrow) Stmt(err);
			stmt->type = StmtType::ArgumentList;
			stmt->add(new (std::nothrow) StmtValue(s));
		} else if (stmt->type == StmtType::ArgumentList) {
			stmt->add(new (std::nothrow)
							StmtValue(new (std::nothrow) Stmt(err, StmtType::WordList, s)));
		} else {
			if (stmt->tail != stmt->value) {
				auto firstArg = stmt->value->next;
				auto lastArg = stmt->tail;
				stmt->tail = stmt->value;
				stmt->value->next = nullptr;
				stmt->type = StmtType::WordList;
				stmt = new (std::nothrow) Stmt(err, StmtType::ArgumentList, stmt);
				stmt->add(new (std::nothrow) StmtValue(
						new (std::nothrow) Stmt(err, StmtType::WordList, firstArg, lastArg)));
			} else {
				stmt = new (std::nothrow) Stmt(err, StmtType::ArgumentList, stmt);
			}

			stmt->add(new (std::nothrow)
							StmtValue(new (std::nothrow) Stmt(err, StmtType::WordList, s)));
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

	if (isMultiline) {
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
			stmt = new (std::nothrow) Stmt(err, StmtType::Expansion, sig);
			str = tmp;
			return stmt;
		}
	}

	while (!str.empty() && (ending == 0 || !str.is(ending))) {
		auto wordStmt = readWord(str, ctx, err);
		if (!wordStmt) {
			if (ending == 0) {
				break;
			}

			return nullptr;
		}

		if (nextArgument) {
			addStmtArgument(wordStmt);
		} else {
			addStmtWord(wordStmt);
		}

		StringView whiteSpace = skipWhitespace(str);

		if (isMultiline) {
			auto nl = countNewlines(whiteSpace);
			for (uint32_t i = 0; i < nl; ++i) { addStringWord("\n"); }
			if (nl > 0) {
				whiteSpace = StringView(); // prevent to preserve already added whitespace
			}
		}

		if (!isMultiline && str.is('#')) {
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
			// preserve whitespace before ','
			if (!whiteSpace.empty()) {
				addStringWord(" ");
			}
			++str;
			skipWhitespace(str);
			nextArgument = true;
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
			if ((ctx == ReadContext::Expansion || ctx == ReadContext::MultilineExpansion) && stmt
					&& stmt->type == StmtType::ArgumentList) {
				if (!whiteSpace.empty()) {
					addStringWord(" ");
				}
			}
		} else {
			nextArgument = false;
		}
	}

	StringView whiteSpace = skipWhitespace(str);

	if (isMultiline) {
		auto nl = countNewlines(whiteSpace);
		for (uint32_t i = 0; i < nl; ++i) { addStringWord("\n"); }
	}

	if (ending && str.is(ending)) {
		if ((ctx == ReadContext::Expansion || ctx == ReadContext::MultilineExpansion) && stmt
				&& stmt->type == StmtType::ArgumentList) {
			if (!whiteSpace.empty()) {
				addStringWord(" ");
			}
		}
		++str;
	}

	return stmt;
}

Stmt::Stmt(const FileLocation &l) : loc(l) { }

Stmt::Stmt(const FileLocation &l, StringView str) : type(StmtType::Word), loc(l) {
	tail = value = new (std::nothrow) StmtValue(str);
}

Stmt::Stmt(const FileLocation &l, StmtType t, StringView str) : type(t), loc(l) {
	tail = value = new (std::nothrow) StmtValue(str);
}

Stmt::Stmt(const FileLocation &l, StmtType t, Stmt *stmt) : type(t), loc(l) {
	tail = value = new (std::nothrow) StmtValue(stmt);
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
	add(new (std::nothrow) StmtValue(str));
}

void Stmt::add(Stmt *stmt) { add(new (std::nothrow) StmtValue(stmt)); }

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
	describe([](StringView str) { std::cout << str; }, level);
}

} // namespace stappler::makefile
