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

#include "XLFontDeferredRequest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

void DeferredRequest::runFontRenderer(event::Looper *queue, const Rc<FontComponent> &ext,
		const Vector<FontUpdateRequest> &req,
		Function<void(uint32_t reqIdx, const CharTexture &texData)> &&onTex,
		Function<void()> &&onComp) {
	auto data = Rc<DeferredRequest>::alloc(ext, req);
	data->onTexture = sp::move(onTex);
	data->onComplete = sp::move(onComp);

	for (uint32_t i = 0; i < queue->getThreadPool()->getInfo().threadCount; ++i) {
		queue->performAsync([data]() { data->runThread(); });
	}
}

DeferredRequest::~DeferredRequest() { }

DeferredRequest::DeferredRequest(const Rc<FontComponent> &ext, const Vector<FontUpdateRequest> &req)
: ext(ext) {
	for (auto &it : req) { nrequests += it.chars.size(); }

	fontRequests.reserve(nrequests);

	for (uint32_t i = 0; i < req.size(); ++i) {
		faces.emplace_back(req[i].object);
		for (auto &it : req[i].chars) { fontRequests.emplace_back(i, it); }
	}
}

void DeferredRequest::runThread() {
	Vector<Rc<FontFaceObjectHandle>> threadFaces;
	threadFaces.resize(faces.size(), nullptr);
	uint32_t target = current.fetch_add(1);
	uint32_t c = 0;
	while (target < nrequests) {
		auto &v = fontRequests[target];
		if (v.second == 0) {
			c = complete.fetch_add(1);
			target = current.fetch_add(1);
			continue;
		}

		if (!threadFaces[v.first]) {
			threadFaces[v.first] = ext->getLibrary()->makeThreadHandle(faces[v.first]);
		}

		threadFaces[v.first]->acquireTexture(v.second,
				[&, this](const font::CharTexture &tex) { onTexture(v.first, tex); });
		c = complete.fetch_add(1);
		target = current.fetch_add(1);
	}
	threadFaces.clear();
	if (c == nrequests - 1) {
		onComplete();
	}
}

} // namespace stappler::xenolith::font
