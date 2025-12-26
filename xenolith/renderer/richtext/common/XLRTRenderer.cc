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

#include "XLRTRenderer.h"
#include "XLRTCommonSource.h"
#include "XLNode.h"
#include "XL2dScrollView.h"
#include "XLDirector.h"
#include "XLFontController.h"
#include "XLTemporaryResource.h"
#include "XLResourceCache.h"
#include "SPDocPageContainer.h"
#include "SPDocStyle.h"
#include "SPDocLayoutEngine.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

static constexpr const float PAGE_SPLIT_COEF = (4.0f / 3.0f);
static constexpr const float PAGE_SPLIT_WIDTH = (800.0f);

bool Renderer::shouldSplitPages(const Size2 &s) {
	float coef = s.width / s.height;
	if (coef >= PAGE_SPLIT_COEF && s.width > PAGE_SPLIT_WIDTH) {
		return true;
	}
	return false;
}

RendererResource::~RendererResource() {
	if (resource && cache) {
		cache->removeTemporaryResource(resource->getName());
	}
}

RendererResult::RendererResult(const RendererResult &other) {
	app = other.app;
	fc = other.fc;
	document = other.document;
	source = other.source;
	result = other.result;
	hyphen = other.hyphen;
	resource = other.resource;
	locks = other.locks;
	media = other.media;
	ids = other.ids;
	ctime = other.ctime;
}

RendererResult &RendererResult::operator=(const RendererResult &other) {
	app = other.app;
	fc = other.fc;
	document = other.document;
	source = other.source;
	result = other.result;
	hyphen = other.hyphen;
	resource = other.resource;
	locks = other.locks;
	media = other.media;
	ids = other.ids;
	ctime = other.ctime;
	return *this;
}

Renderer::~Renderer() { }

bool Renderer::init(Vector<String> &&ids) {
	if (!System::init()) {
		return false;
	}

	_ids = sp::move(ids);

	pushVersionOptions();

	scheduleUpdate();

	return true;
}

void Renderer::handleVisitSelf(FrameInfo &info, Node *node, NodeVisitFlags flags) {
	System::handleVisitSelf(info, node, flags);
	if (_renderingDirty && !_renderingInProgress && _enabled && _source) {
		requestRendering();
	}
}

void Renderer::update(const UpdateTime &time) {
	System::update(time);

	if (_source) {
		_source->update(time);
		if (_source->isDirty()) {
			onSource();
		}
	}
}

void Renderer::setSource(CommonSource *source) {
	if (_source != source) {
		_source = source;
		_renderingDirty = true;
	}
}

CommonSource *Renderer::getSource() const { return _source; }

Document *Renderer::getDocument() const {
	if (_result) {
		return _result->document;
	} else if (auto s = getSource()) {
		return s->getDocument();
	}
	return nullptr;
}

RendererResult *Renderer::getResult() const { return _result; }

MediaParameters Renderer::getMedia() const { return _media; }

void Renderer::handleContentSizeDirty() {
	_isPageSplitted = false;
	auto size = _owner->getContentSize();
	auto scroll = dynamic_cast<basic2d::ScrollViewBase *>(_owner);
	if (scroll) {
		auto &padding = scroll->getPadding();
		if (scroll->isVertical()) {
			size.width -= padding.horizontal();
		}
	}
	if (hasFlag(document::RenderFlags::PaginatedLayout)) {
		if (shouldSplitPages(size)) {
			size.width /= 2.0f;
			_isPageSplitted = true;
		}
		size.width -= _pageMargin.horizontal();
		size.height -= _pageMargin.vertical();
		setSurfaceSize(size);
	} else {
		setSurfaceSize(size);
	}
}

void Renderer::setSurfaceSize(const Size2 &size) {
	if (_surfaceSize != size) {
		if (hasFlag(document::RenderFlags::PaginatedLayout)
				|| !hasFlag(document::RenderFlags::NoHeightCheck)
				|| size.width != _surfaceSize.width) {
			_renderingDirty = true;
		}
		_surfaceSize = size;
	}
	if (_media.surfaceSize != _surfaceSize) {
		_media.surfaceSize = _surfaceSize;
	}
}

const Size2 &Renderer::getSurfaceSize() const { return _surfaceSize; }

const Padding &Renderer::getPageMargin() const { return _pageMargin; }

bool Renderer::isPageSplitted() const { return _isPageSplitted; }

void Renderer::setIds(Vector<String> &&ids) { _ids = sp::move(ids); }

void Renderer::setDpi(int dpi) {
	if (_media.dpi != dpi) {
		_media.dpi = dpi;
		_renderingDirty = true;
	}
}
void Renderer::setDensity(float density) {
	if (_media.density != density) {
		_media.density = density;
		_renderingDirty = true;
	}
}

void Renderer::setDefaultBackground(const Color4B &c) {
	if (_media.defaultBackground != c) {
		_media.defaultBackground = c;
		_renderingDirty = true;
	}
}

void Renderer::setFontScale(float val) {
	if (_fontScale != val) {
		_fontScale = val;
		_renderingDirty = true;
	}
}

void Renderer::setMediaType(document::MediaType value) {
	if (_media.mediaType != value) {
		_media.mediaType = value;
		_renderingDirty = true;
	}
}
void Renderer::setOrientationValue(document::Orientation value) {
	if (_media.orientation != value) {
		_media.orientation = value;
		_renderingDirty = true;
	}
}
void Renderer::setPointerValue(document::Pointer value) {
	if (_media.pointer != value) {
		_media.pointer = value;
		_renderingDirty = true;
	}
}
void Renderer::setHoverValue(document::Hover value) {
	if (_media.hover != value) {
		_media.hover = value;
		_renderingDirty = true;
	}
}
void Renderer::setLightLevelValue(document::LightLevel value) {
	if (_media.lightLevel != value) {
		_media.lightLevel = value;
		_renderingDirty = true;
	}
}
void Renderer::setScriptingValue(document::Scripting value) {
	if (_media.scripting != value) {
		_media.scripting = value;
		_renderingDirty = true;
	}
}

void Renderer::setPageMargin(const Padding &margin) {
	if (_pageMargin != margin) {
		_pageMargin = margin;
		if (hasFlag(document::RenderFlags::PaginatedLayout)) {
			_renderingDirty = true;
		}
	}
}

void Renderer::addOption(const StringView &str) {
	_media.addOption(str);
	_renderingDirty = true;
}

void Renderer::removeOption(const StringView &str) {
	_media.removeOption(str);
	_renderingDirty = true;
}

bool Renderer::hasOption(const StringView &str) const { return _media.hasOption(str); }

void Renderer::addFlag(document::RenderFlags flag) {
	_media.flags |= flag;
	_renderingDirty = true;
}
void Renderer::removeFlag(document::RenderFlags flag) {
	_media.flags &= ~flag;
	_renderingDirty = true;
}
bool Renderer::hasFlag(document::RenderFlags flag) const {
	return (_media.flags & flag) != document::RenderFlags::None;
}

void Renderer::onSource() {
	auto s = getSource();
	if (s && s->isReady()) {
		_renderingDirty = true;
		requestRendering();
		s->dropDirty();
	}
}

bool Renderer::requestRendering() {
	auto s = getSource();
	if (!_enabled || _renderingInProgress || _surfaceSize == Size2::ZERO || !s) {
		return false;
	}

	Document *document = nullptr;
	if (s->isReady()) {
		document = s->getDocument();
	}

	if (document) {
		auto app = _owner->getDirector()->getApplication();

		auto req = Rc<RendererResult>::alloc();
		req->ctime = Time::now();
		req->app = app;
		req->fc = app->getExtension<font::FontController>();
		req->document = document;
		req->source = s;
		req->hyphen = s->getHyphens();

		req->media = _media;
		req->media.density = _owner->getInputDensity();
		//req->media.dpi = app->getInfo().dpi;
		req->media.fontScale = _fontScale;
		req->media.pageMargin = _pageMargin;
		if (_isPageSplitted) {
			req->media.flags |= document::RenderFlags::SplitPages;
		}
		req->ids = _ids;

		req->resource = _source->prepareResource(_owner->getDirector()->getResourceCache(),
				req->document, req->ctime);

		_renderingInProgress = true;
		if (_renderingCallback) {
			_renderingCallback(nullptr, true);
		}

		app->perform([req](const thread::Task &) -> bool {
			Vector<StringView> targetIds;
			for (auto &it : req->ids) { targetIds.emplace_back(it); }

			document::LayoutEngine impl(req->document,
					[req](const document::FontStyleParameters &params) {
				auto f = req->fc->getLayout(params);
				if (!f) {
					auto p = params;
					p.fontFamily = "default";
					f = req->fc->getLayout(p);
				}
				return f;
			}, req->media, targetIds);
			impl.setExternalAssetsMeta(document::ExternalAssetsMap(req->resource->externalAssets));
			impl.setHyphens(req->hyphen);
			impl.render();

			req->result = impl.getResult();
			return true;
		}, [this, req](const thread::Task &, bool) {
			if (req->result) {
				onResult(req);
			}
		}, this);

		_renderingDirty = false;
	}
	return false;
}

void Renderer::onResult(RendererResult *result) {
	_renderingInProgress = false;
	if (_renderingDirty) {
		requestRendering();
	} else {
		_result = result;
	}

	if (_owner && !_renderingInProgress && _renderingCallback) {
		_renderingCallback(_result, false);
	}
}

void Renderer::setRenderingCallback(const RenderingCallback &cb) { _renderingCallback = cb; }

StringView Renderer::getLegacyBackground(const document::Node &node, const StringView &opt) const {
	if (auto doc = getSource()->getDocument()) {
		auto root = doc->getRoot();

		document::StyleList style;
		auto media = _media;
		media.density = _owner->getInputDensity();
		//media.dpi = _owner->getDirector()->getApplication()->getInfo().dpi;
		media.addOption(opt.str<Interface>());

		auto resolved = media.resolveMediaQueries<memory::PoolInterface>(doc->getData()->queries);

		root->resolveNodeStyle(style, node, SpanView<const document::Node *>(), media, resolved);

		document::SimpleStyleInterface iface(resolved, doc->getData()->strings, media.density,
				_fontScale);
		auto bg = style.compileBackground(&iface);

		return bg.backgroundImage;
	}
	return StringView();
}

void Renderer::pushVersionOptions() {
	auto v = config::ENGINE_VERSION;
	for (uint32_t i = 0; i <= v; i++) { addOption(toString("stappler-v", i, "-plus")); }
	addOption(toString("stappler-v", v));
}

} // namespace stappler::xenolith::richtext
