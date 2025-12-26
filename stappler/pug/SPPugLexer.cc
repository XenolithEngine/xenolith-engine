/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPPugLexer.h"
#include "SPPugExpression.h"

namespace STAPPLER_VERSIONIZED stappler::pug {

Lexer::Lexer(const StringView &str, const OutStream &err)
: content(str), root(Token::Root, content) {
	success = perform(err);
}

bool Lexer::perform(const OutStream &OutStream) { return parseToken(OutStream, root); }

static uint32_t checkIndent(uint32_t &indent, StringView &str) {
	auto indentStr = str.readChars<StringView::Chars<'\t', ' '>>();
	if (!indentStr.empty()) {
		if (indent == maxOf<uint32_t>()) {
			if (indentStr.is('\t')) {
				indent = 0;
			} else {
				indentStr = indentStr.readUntil<StringView::Chars<'\t'>>();
				indent = indentStr.size();
			}
			return 1;
		} else if (indent == 0) {
			auto indentStrTmp = indentStr.readChars<StringView::Chars<'\t'>>();
			if (!indentStr.empty()) {
				return maxOf<uint32_t>();
			}
			return indentStrTmp.size();
		} else {
			auto indentStrTmp = indentStr.readChars<StringView::Chars<' '>>();
			if (!indentStr.empty()) {
				return maxOf<uint32_t>();
			}
			return indentStrTmp.size() / indent;
		}
	}
	return 0;
}

using TagWordFilter = StringView::Compose<StringView::CharGroup<CharGroupId::Alphanumeric>,
		StringView::Chars<'-', '_'>>;
using AttrWordFilter = StringView::Compose<StringView::CharGroup<CharGroupId::Alphanumeric>,
		StringView::Chars<'@', '-', '_', ':', '(', ')', '.'>>;
using SpacingFilter = StringView::Chars<' ', '\t'>;
using NewLineFilter = StringView::Chars<'\n', '\r'>;

bool Lexer::parseToken(const OutStream &OutStream, Token &tok) {
	StringView r = tok.data;

	Token *currentTok = &tok;

	std::array<Token *, 32> stack;

	while (!r.empty()) {
		StringView tmp(r);
		auto indent = indentLevel;
		bool followTag = false;
		if (r.is(':')) {
			++r;
			r.skipChars<SpacingFilter>();
			followTag = true;
		} else {
			indent = checkIndent(indentStep, r);
			if (indent == 0 && !r.is<NewLineFilter>()) {
				indentStep = maxOf<uint32_t>();
			} else if (indent == maxOf<uint32_t>() && !r.is<NewLineFilter>() && !r.empty()) {
				return onError(OutStream, r, "Mixed tab and spaces indentations");
			}
		}

		if (!r.is<NewLineFilter>() && !r.empty()) {
			if (indent == indentLevel) {
				// do nothing
			} else if (indent == indentLevel + 1) {
				if (currentTok->tail) {
					stack[indentLevel] = currentTok;
					currentTok = currentTok->tail;
					indentLevel = indent;
				} else {
					return false; // invalid indentation
				}
			} else if (indent < indentLevel) {
				currentTok = stack[indent];
				indentLevel = indent;
			} else {
				return onError(OutStream, tmp, "Wrong indentation markup");
			}

			if (auto line = readLine(OutStream, tmp, r, currentTok)) {
				if (followTag) {
					currentTok->tail->addChild(line);
					currentTok = line;
					stack[indentLevel] = currentTok;
				} else {
					currentTok->addChild(line);
				}
			} else {
				if (!r.is<StringView::Chars<'\n', '\r'>>() && !r.empty()) {
					return false;
				}
			}
		}
		if (!r.is(':')) {
			if (followTag) {
				currentTok = indentLevel > 0 ? stack[indentLevel - 1]->tail : root.tail;
			}
			r.skipUntil<StringView::Chars<'\n'>>();
			if (r.is('\n')) {
				++r;
			}
		}
	}
	return true;
}

bool Lexer::readAttributes(const OutStream &OutStream, Token *data, StringView &r) const {
	auto attrs = new (std::nothrow) Token{Token::TagAttrList, r};

	auto readAttrName = [](StringView &r) -> Token * {
		auto ret = new (std::nothrow) Token{Token::AttrName, r};
		if (r.is('\'')) {
			++r;
			while (!r.empty() && !r.is('\'')) {
				auto str = r.readUntil<StringView::Chars<'\'', '\\'>>();
				if (r.is('\\')) {
					if (!str.empty()) {
						ret->addChild(new (std::nothrow) Token{Token::PlainText, str});
					}
					++r;
					if (!r.empty()) {
						ret->addChild(new (std::nothrow) Token{Token::PlainText, StringView(r, 1)});
						++r;
					}
				} else if (r.is('\'') && !str.empty()) {
					ret->addChild(new (std::nothrow) Token{Token::PlainText, str});
				}
			}
			if (r.is('\'')) {
				++r;
			} else {
				return nullptr;
			}
		} else if (r.is('"')) {
			++r;
			while (!r.empty() && !r.is('"')) {
				auto str = r.readUntil<StringView::Chars<'"', '\\'>>();
				if (r.is('\\')) {
					if (!str.empty()) {
						ret->addChild(new (std::nothrow) Token{Token::PlainText, str});
					}
					++r;
					if (!r.empty()) {
						ret->addChild(new (std::nothrow) Token{Token::PlainText, StringView(r, 1)});
						++r;
					}
				} else if (r.is('"') && !str.empty()) {
					ret->addChild(new (std::nothrow) Token{Token::PlainText, str});
				}
			}
			if (r.is('"')) {
				++r;
			} else {
				return nullptr;
			}
		} else {
			StringView tmp2(r);
			auto str = tmp2.readChars<AttrWordFilter>();
			if (!str.empty()) {
				if (tmp2.empty() || (!tmp2.is('=') && !tmp2.is('!'))) {
					if (str.back() == ')') {
						str = StringView(str, str.size() - 1);
					}
				}
				r += str.size();
				ret->addChild(new (std::nothrow) Token{Token::PlainText, str});
			} else {
				return nullptr;
			}
		}
		ret->data = StringView(ret->data, ret->data.size() - r.size());
		return ret;
	};

	r.skipChars<SpacingFilter, NewLineFilter>();
	while (!r.is<NewLineFilter>() && !r.is(')') && !r.empty()) {
		auto tok = new (std::nothrow) Token{Token::AttrPairEscaped, r};
		auto name = readAttrName(r);
		if (!name) {
			return onError(OutStream, r, StringView("Invalid attribute name"));
		}

		if (r.is("!=")) {
			tok->type = Token::AttrPairUnescaped;
			r += 2;
		} else if (r.is('=')) {
			tok->type = Token::AttrPairEscaped;
			++r;
		} else if (r.is<CharGroupId::WhiteSpace>() || r.is(',') || r.is(')')) {
			tok->addChild(name);
			tok->data = StringView(tok->data, tok->data.size() - r.size());
			r.skipChars<SpacingFilter, NewLineFilter>();
			if (r.is(',')) {
				++r;
			}
			r.skipChars<SpacingFilter, NewLineFilter>();
			attrs->addChild(tok);
			continue;
		} else {
			return onError(OutStream, r, StringView("Invalid attribute operator"));
		}

		tok->addChild(name);

		auto valTok = new (std::nothrow) Token{Token::AttrValue, r};
		if (!readOutputExpression(valTok, r)) {
			return onError(OutStream, r, StringView("Invalid attribute value"));
		}

		valTok->data = StringView(valTok->data, valTok->data.size() - r.size());

		tok->addChild(valTok);
		tok->data = StringView(tok->data, tok->data.size() - r.size());

		attrs->addChild(tok);

		r.skipChars<SpacingFilter, NewLineFilter>();
		if (r.is(',')) {
			++r;
		}
		r.skipChars<SpacingFilter, NewLineFilter>();
	}

	if (!r.is(')')) {
		return onError(OutStream, r, StringView("Invalid attribute list"));
	} else {
		attrs->data = StringView(attrs->data, attrs->data.size() - r.size());
		++r;
	}

	data->addChild(attrs);
	data->data = StringView(data->data, data->data.size() - r.size());

	return true;
}

bool Lexer::readOutputExpression(Token *valTok, StringView &r) const {
	if (auto expr = Expression::parse(r, Expression::Options::getDefaultInline())) {
		valTok->expression = expr;
		return true;
	}

	return false;
}

bool Lexer::readTagInfo(const OutStream &OutStream, Token *data, StringView &r,
		bool interpolated) const {
	while (r.is('.') || r.is('#') || r.is('(') || r.is('/') || r.is('=') || r.is('!') || r.is('&')
			|| r.is(':')) {
		if (r.is(':')) {
			return true;
		}

		auto c = r[0];
		++r;
		switch (c) {
		case '.': {
			auto word = r.readChars<TagWordFilter>();
			if (!word.empty()) {
				data->addChild(new (std::nothrow) Token{Token::TagClassNote, word});
			} else if (r.is<NewLineFilter>()) {
				data->addChild(new (std::nothrow) Token{Token::TagTrailingDot, StringView()});
			}
			break;
		}
		case '#':
			data->addChild(
					new (std::nothrow) Token{Token::TagIdNote, r.readChars<TagWordFilter>()});
			break;
		case '(':
			if (!readAttributes(OutStream, data, r)) {
				return false; // wrong attribute format
			}
			break;
		case '&':
			if (r.is("attributes")) {
				r += "attributes("_len;
				auto tmp = r;
				if (auto expr = Expression::parse(r, Expression::Options::getDefaultInline())) {
					if (r.is(')')) {
						++r;
						data->addChild(new (std::nothrow) Token{Token::TagAttrExpr,
							StringView(tmp, tmp.size() - r.size()), expr});
					} else {
						return onError(OutStream, r, "Invalid expression in &attributes");
					}
				} else {
					return onError(OutStream, r, "Invalid expression in &attributes");
				}
			} else {
				return onError(OutStream, r, "Unknown expression in tag");
			}
			break;
		case '/':
			data->addChild(new (std::nothrow) Token{Token::TagTrailingSlash, StringView()});
			break;
		case '=':
			data->addChild(new (std::nothrow) Token{Token::TagTrailingEq, StringView()});
			break;
		case '!':
			if (r.is('=')) {
				++r;
				data->addChild(new (std::nothrow) Token{Token::TagTrailingNEq, StringView()});
			}
			break;
		default: break;
		}

		if (data->tail->type == Token::TagTrailingSlash
				|| data->tail->type == Token::TagTrailingDot) {
			r.skipChars<SpacingFilter>();
			if (!r.is<NewLineFilter>()) {
				return onError(OutStream, r, "Data after endline tag");
			}
			break;
		} else if (data->tail->type == Token::TagTrailingEq
				|| data->tail->type == Token::TagTrailingNEq) {
			r.skipChars<SpacingFilter>();
			if (r.is<NewLineFilter>() || (interpolated && r.is(']'))) {
				return true;
			}
			auto tmp = r;
			if (auto expr = Expression::parse(r, Expression::Options::getDefaultInline())) {
				r.skipChars<SpacingFilter>();
				if (r.is<NewLineFilter>() || r.empty() || (interpolated && r.is(']'))) {
					data->addChild(new (std::nothrow) Token{data->tail->type == Token::TagTrailingEq
								? Token::OutputEscaped
								: Token::OutputUnescaped,
						StringView(tmp, tmp.size() - r.size()), expr});
					return true;
				}
			}
			return onError(OutStream, r, "Invalid expression in tag attribute output block");
		}
	}

	r.skipChars<SpacingFilter>();
	if (!r.is<NewLineFilter>()) {
		return readPlainTextInterpolation(OutStream, data, r, interpolated);
	}
	return true;
};

bool Lexer::readCode(Token *data, StringView &r) const {
	r.skipChars<SpacingFilter>();
	while (!r.empty() && !r.is<NewLineFilter>()) {
		auto tmp = r;
		if (auto expr = Expression::parse(r, Expression::Options::getDefaultInline())) {
			r.skipChars<SpacingFilter>();
			if (r.is<NewLineFilter>() || r.is(';')) {
				if (r.is(';')) {
					++r;
				}
			} else {
				return false;
			}
			data->addChild(new (std::nothrow)
							Token{Token::Code, StringView(tmp, tmp.size() - r.size()), expr});
		} else {
			return false;
		}
	}
	return true;
}

bool Lexer::readCodeBlock(Token *data, StringView &r) const {
	auto newlineTok = StringView(r).readChars<StringView::Chars<'\n', '\r', ' ', '\t'>>();

	while (r.is(';') || r.is(newlineTok)) {
		r += newlineTok.size();

		auto tmp = r;
		if (auto expr = Expression::parse(r,
					Expression::Options::getWithNewlineToken(newlineTok))) {
			r.skipChars<SpacingFilter>();
			if (!r.is<NewLineFilter>() && !r.is(';')) {
				return false;
			}
			data->addChild(new (std::nothrow)
							Token{Token::Code, StringView(tmp, tmp.size() - r.size()), expr});
		} else {
			return false;
		}
	}

	return true;
}

bool Lexer::readPlainTextInterpolation(const OutStream &OutStream, Token *data, StringView &r,
		bool interpolated) const {
	auto line = interpolated ? r : r.readUntil<NewLineFilter>();
	StringView tmp(line);

	StringView buf;
	auto flushBuffer = [&]() {
		if (!buf.empty()) {
			data->addChild(new (std::nothrow) Token{Token::PlainText, buf});
			buf.clear();
		}
	};

	auto appendBuffer = [&](const StringView &str) {
		if (buf.empty()) {
			buf = str;
		} else if (buf.data() + buf.size() == str.data()) {
			buf = StringView(buf.data(), buf.size() + str.size());
		} else {
			flushBuffer();
			buf = str;
		}
	};

	while (!line.empty() && (!interpolated || !line.is(']'))) {
		if (interpolated) {
			appendBuffer(line.readUntil<StringView::Chars<'\\', '#', '!', ']'>>());
		} else {
			appendBuffer(line.readUntil<StringView::Chars<'\\', '#', '!'>>());
		}
		if (line.is('\\')) {
			auto tmp = line;
			++line;
			if (line.is("#{") || line.is("#[") || line.is("!{")) {
				appendBuffer(StringView(line, 2));
				line += 2;
			} else {
				++line;
				appendBuffer(StringView(tmp, 2));
			}
		} else if (line.is("#{")) {
			flushBuffer();
			line += 2;

			auto tmp = line;
			if (auto expr = Expression::parse(line, Expression::Options::getDefaultInline())) {
				data->addChild(new (std::nothrow) Token{Token::OutputEscaped,
					StringView(tmp, tmp.size() - line.size()), expr});
				if (line.is('}')) {
					++line;
				} else {
					return onError(OutStream, tmp, "Invalid interpolation expression");
				}
			}
		} else if (line.is("#[")) {
			flushBuffer();
			line += 2;

			auto word = line.readChars<TagWordFilter>();
			line.skipChars<SpacingFilter>();
			auto retData = new (std::nothrow) Token{Token::LineData, tmp};
			retData->addChild(new (std::nothrow) Token{Token::Tag, word});
			if (!readTagInfo(OutStream, retData, line, true)) {
				return false;
			}
			if (line.is(']')) {
				++line;
				data->addChild(retData);
			} else {
				return onError(OutStream, word, "Invalid tag interpolation expression");
			}
		} else if (line.is("!{")) {
			flushBuffer();
			line += 2;

			auto tmp = line;
			if (auto expr = Expression::parse(line, Expression::Options::getDefaultInline())) {
				data->addChild(new (std::nothrow) Token{Token::OutputUnescaped,
					StringView(tmp, tmp.size() - line.size()), expr});
				if (line.is('}')) {
					++line;
				} else {
					return onError(OutStream, tmp, "Invalid interpolation expression");
				}
			}
		} else if (interpolated && line.is(']')) {
			// no action
		} else {
			appendBuffer(StringView(line, 1));
			++line;
		}
	}

	flushBuffer();

	if (interpolated) {
		r = line;
	}
	return true;
}

static Token *Lexer_completeLine(Token *retData, const StringView &line, StringView &r) {
	retData->data = StringView(retData->data, retData->data.size() - r.size());
	auto retTok = new (std::nothrow) Token(Token::Line, StringView(line, line.size() - r.size()));
	retTok->addChild(retData);
	return retTok;
};

Token *Lexer::readLine(const OutStream &OutStream, const StringView &line, StringView &r,
		Token *rootLine) {
	if (rootLine && rootLine->child) {
		switch (rootLine->child->type) {
		case Token::LineComment:
		case Token::LineDot: return readPlainLine(OutStream, line, r); break;
		case Token::LineData:
			if (rootLine->child->tail && rootLine->child->tail->type == Token::TagTrailingDot) {
				return readPlainLine(OutStream, line, r);
			} else {
				return readCommonLine(OutStream, line, r);
			}
			break;
		case Token::LinePlainText: return readPlainLine(OutStream, line, r); break;
		default: break;
		}
	}

	return readCommonLine(OutStream, line, r);
}

Token *Lexer::readPlainLine(const OutStream &OutStream, const StringView &line, StringView &r) {
	auto retData = new (std::nothrow) Token{Token::LinePlainText, r};
	r.skipChars<SpacingFilter>();
	if (!r.is<NewLineFilter>()) {
		if (!readPlainTextInterpolation(OutStream, retData, r)) {
			return nullptr;
		}
	}
	return Lexer_completeLine(retData, line, r);
}

Token *Lexer::readCommonLine(const OutStream &OutStream, const StringView &line, StringView &r) {
	StringView tmp(r);
	if (r.is("//")) {
		bool isHtml = false;
		auto retData = new (std::nothrow) Token{Token::LineComment, tmp};
		if (r.is("//-")) {
			retData->addChild(new (std::nothrow) Token(Token::CommentTemplate, StringView(r, 3)));
			r += 3;
		} else {
			isHtml = true;
			retData->addChild(new (std::nothrow) Token(Token::CommentHtml, StringView(r, 2)));
			r += 2;
		}

		if (!r.is<NewLineFilter>()) {
			if (isHtml) {
				if (!readPlainTextInterpolation(OutStream, retData, r)) {
					return nullptr;
				}
			} else {
				retData->addChild(
						new (std::nothrow) Token{Token::PlainText, r.readUntil<NewLineFilter>()});
			}
		}

		return Lexer_completeLine(retData, line, r);
	} else if (r.is<StringView::CharGroup<CharGroupId::Latin>>()) {
		return readKeywordLine(OutStream, line, r);
	} else if (r.is('.') || r.is('#') || r.is('(') || r.is('&')) {
		StringView t(r, 1, 1);
		if (t.is<TagWordFilter>()) {
			auto retData = new (std::nothrow) Token{Token::LineData, tmp};
			retData->addChild(new (std::nothrow) Token{Token::Tag, r.readChars<TagWordFilter>()});
			if (!readTagInfo(OutStream, retData, r)) {
				return nullptr;
			}
			return Lexer_completeLine(retData, line, r);
		} else if (r.is('.') && t.is<NewLineFilter>()) {
			auto retData = new (std::nothrow) Token{Token::LineDot, tmp};
			retData->addChild(new (std::nothrow) Token{Token::TagTrailingDot, StringView(r, 1)});
			++r;
			return Lexer_completeLine(retData, line, r);
		}
	} else if (r.is('|')) {
		auto retData = new (std::nothrow) Token{Token::LinePiped, tmp};
		retData->addChild(new (std::nothrow) Token{Token::PipeMark, StringView()});
		++r;
		r.skipChars<SpacingFilter>();
		if (!r.is<NewLineFilter>()) {
			if (!readPlainTextInterpolation(OutStream, retData, r)) {
				return nullptr;
			}
		}
		return Lexer_completeLine(retData, line, r);
	} else if (r.is('=') || r.is("!=")) {
		auto retData = new (std::nothrow) Token{Token::LineOut, tmp};
		Token *exprToken = nullptr;
		if (r.is('=')) {
			++r;
			exprToken = new (std::nothrow) Token{Token::OutputEscaped, StringView()};
		} else {
			r += 2;
			exprToken = new (std::nothrow) Token{Token::OutputUnescaped, StringView()};
		}
		r.skipChars<SpacingFilter>();

		if (!r.is<NewLineFilter>()) {
			auto tmp = r;
			if (auto expr = Expression::parse(r, Expression::Options::getDefaultInline())) {
				r.skipChars<SpacingFilter>();
				if (!r.is<NewLineFilter>() && !r.empty()) {
					onError(OutStream, r, "Invalid expression after output expression block");
					return nullptr;
				} else {
					exprToken->data = StringView(tmp, tmp.size() - r.size());
					exprToken->expression = expr;
					retData->addChild(exprToken);
				}
			} else {
				onError(OutStream, r, "Invalid expression in output block");
				return nullptr;
			}
		}
		return Lexer_completeLine(retData, line, r);
	} else if (r.is('-')) {
		++r;
		if (!r.is<NewLineFilter>()) {
			auto retData = new (std::nothrow) Token{Token::LineCode, tmp};
			if (readCode(retData, r)) {
				return Lexer_completeLine(retData, line, r);
			} else {
				onError(OutStream, r, "Fail to read line of code");
				return nullptr;
			}
		} else {
			auto retData = new (std::nothrow) Token{Token::LineCodeBlock, tmp};
			if (readCodeBlock(retData, r)) {
				return Lexer_completeLine(retData, line, r);
			} else {
				onError(OutStream, r, "Fail to read block of code");
				return nullptr;
			}
		}
	} else if (r.is('+')) {
		++r;
		auto retData = new (std::nothrow) Token{Token::MixinCall, tmp};
		r.skipChars<SpacingFilter>();

		auto name = r.readChars<TagWordFilter>();
		if (name.empty()) {
			onError(OutStream, r, "Invalid mixin name");
			return nullptr;
		}

		retData->data = name;

		if (r.is('(')) {
			auto tmp = r;
			if (auto expr = Expression::parse(r, Expression::Options::getDefaultInline())) {
				auto exprToken = new (std::nothrow) Token{Token::MixinArgs, tmp};
				r.skipChars<SpacingFilter>();
				if (!r.is<NewLineFilter>() && !r.empty()) {
					onError(OutStream, r, "Invalid expression after mixin call block");
					return nullptr;
				} else {
					exprToken->data = StringView(tmp, tmp.size() - r.size());
					exprToken->expression = expr;
					retData->addChild(exprToken);
				}
			} else {
				onError(OutStream, r, "Invalid expression in mixin call block");
				return nullptr;
			}
		}

		auto retTok =
				new (std::nothrow) Token(Token::Line, StringView(line, line.size() - r.size()));
		retTok->addChild(retData);
		return retTok;
	} else if (r.is('<')) {
		auto retData = new (std::nothrow) Token{Token::LinePlainText, r.readUntil<NewLineFilter>()};
		auto retTok =
				new (std::nothrow) Token(Token::Line, StringView(line, line.size() - r.size()));
		retTok->addChild(retData);
		return retTok;
	} else if (r.is<NewLineFilter>() || r.empty()) {
		return nullptr;
	}

	onError(OutStream, r, "Fail to recognize line type");
	return nullptr;
}

Token *Lexer::readKeywordLine(const OutStream &OutStream, const StringView &line, StringView &r) {
	StringView tmp(r);

	auto readKeywordExpression = [&, this](StringView &r, Token::Type t) -> Token * {
		if (auto expr = Expression::parse(r, Expression::Options::getDefaultInline())) {
			auto retData = new (std::nothrow) Token{t, StringView(tmp, tmp.size() - r.size())};
			retData->expression = expr;
			if (t == Token::ControlMixin) {
				if (!((retData->expression->op == Expression::Call
							  && retData->expression->left->isToken)
							|| (retData->expression->op == Expression::NoOp
									&& retData->expression->isToken))) {
					onError(OutStream, r, "Invalid mixin definition");
					return nullptr;
				}
			}
			return retData;
		} else {
			onError(OutStream, r, "Invalid expression in control statement");
			return nullptr;
		}
	};

	auto word = r.readChars<TagWordFilter>();
	bool hasSpacing = false;
	if (r.is<SpacingFilter>()) {
		hasSpacing = true;
		r.skipChars<SpacingFilter>();
		if (word == "include") {
			auto target = r.readUntil<NewLineFilter>();
			target.trimChars<SpacingFilter>();
			if (!target.empty()) {
				return new (std::nothrow) Token{Token::Include, target};
			} else {
				return nullptr;
			}
		} else if (word == "mixin") {
			return readKeywordExpression(r, Token::ControlMixin);
		} else if (word == "doctype") {
			auto target = r.readUntil<NewLineFilter>();
			target.trimChars<SpacingFilter>();
			if (!target.empty()) {
				return new (std::nothrow) Token{Token::Doctype, target};
			} else {
				return nullptr;
			}
		} else if (word == "case") {
			return readKeywordExpression(r, Token::ControlCase);
		} else if (word == "when") {
			return readKeywordExpression(r, Token::ControlWhen);
		} else if (word == "if") {
			return readKeywordExpression(r, Token::ControlIf);
		} else if (word == "unless") {
			return readKeywordExpression(r, Token::ControlUnless);
		} else if (word == "elseif") {
			return readKeywordExpression(r, Token::ControlElseIf);
		} else if (word == "while") {
			return readKeywordExpression(r, Token::ControlWhile);
		} else if (word == "else" && r.is("if")) {
			r += 2;
			if (r.is<SpacingFilter>()) {
				r.skipChars<SpacingFilter>();
				return readKeywordExpression(r, Token::ControlElseIf);
			} else {
				onError(OutStream, r, "Invalid expression in 'else if' statement");
				return nullptr;
			}
		} else if (word == "each" || word == "for") {
			StringView var1 = r.readChars<TagWordFilter>();
			StringView var2;
			if (r.is<SpacingFilter>() || r.is(',')) {
				r.skipChars<SpacingFilter>();
				if (r.is(',')) {
					++r;
					r.skipChars<SpacingFilter>();
					var2 = r.readChars<TagWordFilter>();
					if (!r.is<SpacingFilter>()) {
						onError(OutStream, r, "Invalid variable expression in 'each' statement");
						return nullptr;
					}
					r.skipChars<SpacingFilter>();
				}

				if (!var1.empty() && r.is("in")) {
					r += 2;
					if (r.is<SpacingFilter>()) {
						r.skipChars<SpacingFilter>();
						if (auto expr = Expression::parse(r,
									Expression::Options::getDefaultInline())) {
							if (!var2.empty()) {
								auto retData = new (std::nothrow) Token{Token::ControlEachPair,
									StringView(tmp, tmp.size() - r.size())};
								retData->addChild(
										new (std::nothrow) Token(Token::ControlEachVariable, var1));
								retData->addChild(
										new (std::nothrow) Token(Token::ControlEachVariable, var2));
								retData->expression = expr;
								return retData;
							} else {
								auto retData = new (std::nothrow) Token{Token::ControlEach,
									StringView(tmp, tmp.size() - r.size())};
								retData->addChild(
										new (std::nothrow) Token(Token::ControlEachVariable, var1));
								retData->expression = expr;
								return retData;
							}
						} else {
							onError(OutStream, r, "Invalid expression in 'each' statement");
							return nullptr;
						}
					}
				}
			}
			onError(OutStream, r, "Invalid 'each' statement");
			return nullptr;
		}
	}

	if (word == "default") {
		if (r.is(':') || r.is<NewLineFilter>()) {
			return new (std::nothrow)
					Token{Token::ControlDefault, StringView(tmp, tmp.size() - r.size())};
		} else {
			onError(OutStream, r, "Invalid 'default' line");
			return nullptr;
		}
	} else if (word == "else") {
		if (r.is(':') || r.is<NewLineFilter>()) {
			return new (std::nothrow)
					Token{Token::ControlElse, StringView(tmp, tmp.size() - r.size())};
		} else {
			onError(OutStream, r, "Invalid 'else' line");
			return nullptr;
		}
	}

	auto retData = new (std::nothrow) Token{Token::LineData, tmp};
	retData->addChild(new (std::nothrow) Token{Token::Tag, word});
	if (!hasSpacing) {
		if (!readTagInfo(OutStream, retData, r)) {
			return nullptr;
		}
	} else {
		if (!r.is<NewLineFilter>()) {
			if (!readPlainTextInterpolation(OutStream, retData, r, false)) {
				return nullptr;
			}
		}
	}
	return Lexer_completeLine(retData, line, r);
}

bool Lexer::onError(const OutStream &OutStream, const StringView &pos,
		const StringView &str) const {
	StringStream tmpOut;
	std::ostream *out = &std::cout;

	if (OutStream) {
		out = &tmpOut;
	}

	StringView r(content.data(), content.size() - pos.size());

	auto numDigits = [](auto number) -> size_t {
		size_t digits = 0;
		if (number < 0) {
			digits = 1; // remove this line if '-' counts as a digit
		}
		while (number) {
			number /= 10;
			digits++;
		}
		return digits;
	};

	const char *ptr = r.data();
	size_t line = 1;

	while (!r.empty()) {
		r.skipUntil<StringView::Chars<'\n'>>();
		if (r.is('\n')) {
			++line;
			++r;
			ptr = r.data();
		}
	}

	r = StringView(ptr, content.size() - (ptr - content.data()));
	r = r.readUntil<StringView::Chars<'\n'>>();

	auto nd = numDigits(line) + 5;

	*out << "-> " << line << ": " << r << "\n";

	for (size_t i = 0; i < nd; ++i) { *out << ' '; }

	r = StringView(r, pos.data() - r.data());
	while (!r.empty()) {
		r.skipUntil<SpacingFilter>();
		if (r.is<SpacingFilter>()) {
			*out << r[0];
			++r;
		}
	}

	*out << "^\n";
	*out << "Lexer error: " << str << "\n";

	if (OutStream) {
		OutStream(tmpOut.weak());
	}

	return false;
}

} // namespace stappler::pug
