/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_FONT_XLFONTDEFERREDREQUEST_H_
#define XENOLITH_FONT_XLFONTDEFERREDREQUEST_H_

#include "XLFontComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

struct SP_PUBLIC DeferredRequest : Ref {
	static void runFontRenderer(event::Looper *, const Rc<FontComponent> &,
			const Vector<FontUpdateRequest> &req,
			Function<void(uint32_t reqIdx, const CharTexture &texData)> &&, Function<void()> &&);

	virtual ~DeferredRequest();

	DeferredRequest(const Rc<FontComponent> &lib, const Vector<FontUpdateRequest> &req);

	void runThread();

	std::atomic<uint32_t> current = 0;
	std::atomic<uint32_t> complete = 0;
	uint32_t nrequests = 0;
	Vector<Rc<font::FontFaceObject>> faces;
	Vector<Pair<uint32_t, char32_t>> fontRequests;

	Rc<font::FontComponent> ext;
	Function<void(uint32_t reqIdx, const font::CharTexture &texData)> onTexture;
	Function<void()> onComplete;
};

} // namespace stappler::xenolith::font

#endif /* XENOLITH_FONT_XLFONTDEFERREDREQUEST_H_ */
