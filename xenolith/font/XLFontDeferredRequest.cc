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

void DeferredRequest::runFontRenderer(sprt::dispatch::Looper *queue, const Rc<FontComponent> &ext,
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

void DeferredRequest::runFontRendererDirect(sprt::dispatch::Looper *queue,
		const Rc<FontComponent> &ext, const Vector<FontUpdateRequest> &req,
		Function<GlyphTarget(uint32_t reqIdx, const CharTexture &texData)> &&onRender,
		Function<void()> &&onComp) {
	auto data = Rc<DeferredRequest>::alloc(ext, req);
	data->onRender = sp::move(onRender);
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
	// The batch is fanned out to every worker of the pool, so most of the threads below may find
	// nothing to do. Completion has to be reported exactly once, by whoever finishes the last
	// request — see the guard at the end.
	Vector<Rc<FontFaceObjectHandle>> threadFaces;
	threadFaces.resize(faces.size(), nullptr);

	// An empty batch still has to report: the frame that waits on this input has no other way to
	// become ready. (It also cannot be caught below — `nrequests - 1` underflows to UINT32_MAX.)
	if (nrequests == 0) {
		if (!completed.exchange(true)) {
			onComplete();
		}
		return;
	}

	uint32_t target = current.fetch_add(1);
	bool finishedLast = false;
	while (target < nrequests) {
		auto &v = fontRequests[target];
		if (v.second == 0) {
			finishedLast = (complete.fetch_add(1) + 1 == nrequests);
			target = current.fetch_add(1);
			continue;
		}

		if (!threadFaces[v.first]) {
			threadFaces[v.first] = ext->getLibrary()->makeThreadHandle(faces[v.first]);
		}

		if (onRender) {
			threadFaces[v.first]->renderTexture(v.second,
					[&, this](const font::CharTexture &tex) { return onRender(v.first, tex); });
		} else {
			threadFaces[v.first]->acquireTexture(v.second,
					[&, this](const font::CharTexture &tex) { onTexture(v.first, tex); });
		}
		finishedLast = (complete.fetch_add(1) + 1 == nrequests);
		target = current.fetch_add(1);
	}
	threadFaces.clear();

	// Only the thread that pushed the counter to nrequests reports, and only once. The previous
	// test compared a counter that stayed 0 on a thread which never got a request, so a
	// single-request batch fired the completion once per idle worker.
	if (finishedLast && !completed.exchange(true)) {
		onComplete();
	}
}

} // namespace stappler::xenolith::font
