/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLFontGapi.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

// Out-of-line virtual destructors anchor the vtables in this TU (stage-1 vtable lesson).
__SPRT_PUSH_ALLOW_CXXABI_ALLOC

FontGapi::~FontGapi() = default;

FontGapiProxy::~FontGapiProxy() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

void FontGapiProxy::compileImage(const Rc<core::DynamicImage> &, Function<void(bool)> &&cb) {
	// STUB (stage 3): would request the server to compile the atlas image and report back.
	log::source().warn("FontGapiProxy",
			"compileImage: remote font gAPI is not implemented yet (stage-3 stub)");
	if (cb) {
		cb(false);
	}
}

void FontGapiProxy::updateImage(sprt::dispatch::Looper *, const Rc<core::DynamicImage> &,
		Vector<FontUpdateRequest> &&, Rc<core::DependencyEvent> &&, Function<void(bool)> &&cb) {
	// STUB (stage 3): would serialize the glyph requests, ask the server to rasterize them into the
	// atlas, and deliver the atlas-ready result back to the client.
	log::source().warn("FontGapiProxy",
			"updateImage: remote font gAPI is not implemented yet (stage-3 stub)");
	if (cb) {
		cb(false);
	}
}

} // namespace stappler::xenolith::font
