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

#ifndef EXTRA_WEBSERVER_PUG_SPPUGLEXER_H_
#define EXTRA_WEBSERVER_PUG_SPPUGLEXER_H_

#include "SPPugToken.h"

namespace STAPPLER_VERSIONIZED stappler::pug {

struct SP_PUBLIC Lexer {
	using OutStream = Callback<void(StringView)>;

	Lexer(const StringView &, const OutStream & = nullptr);

	bool perform(const OutStream &);
	bool parseToken(const OutStream &, Token &);

	bool readAttributes(const OutStream &, Token *, StringView &) const;
	bool readOutputExpression(Token *, StringView &) const;
	bool readTagInfo(const OutStream &, Token *, StringView &, bool interpolated = false) const;
	bool readCode(Token *, StringView &) const;
	bool readCodeBlock(Token *, StringView &) const;

	bool readPlainTextInterpolation(const OutStream &errCb, Token *, StringView &, bool interpolated = false) const;

	Token *readLine(const OutStream &errCb, const StringView &line, StringView &, Token *rootLine);
	Token *readPlainLine(const OutStream &errCb, const StringView &line, StringView &);
	Token *readCommonLine(const OutStream &errCb, const StringView &line, StringView &);
	Token *readKeywordLine(const OutStream &errCb, const StringView &line, StringView &);

	bool onError(const OutStream &, const StringView &r, const StringView &) const;

	operator bool () const { return success; }

	uint32_t offset = 0;
	uint32_t lineOffset = 0;

	uint32_t indentStep = maxOf<uint32_t>();
	uint32_t indentLevel = 0;

	bool success = false;

	StringView content;
	Token root;
};

}

#endif /* EXTRA_WEBSERVER_PUG_SPPUGLEXER_H_ */
