/**
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

#include "XLUiStyleSystem.h"
#include "XLDirector.h"
#include "XLAppThread.h"
#include "SPFilesystem.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/event.h> // WatchFlags

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId StyleSystemState::Id;

static sprt::atomic<uint64_t> s_styleSystemId = 1;

bool StyleSystem::init() {
	if (!System::init()) {
		return false;
	}

	_systemPriority = StyleDefaultPriority;
	_systemId = s_styleSystemId.fetch_add(1);

	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents);
	return true;
}

bool StyleSystem::init(Rc<StyleSheet> &&sheet) {
	if (!init()) {
		return false;
	}
	_sheet = move(sheet);
	return true;
}

bool StyleSystem::init(StringView css) {
	if (!init()) {
		return false;
	}

	_sources.emplace_back(StyleSource{false, String(css.data(), css.size())});
	rebuildFromSources();
	return true;
}

bool StyleSystem::init(const FileInfo &file) {
	if (!init()) {
		return false;
	}

	_sources.emplace_back(StyleSource{true, String(file.path.data(), file.path.size()),
		file.category, file.flags});
	rebuildFromSources();
	return true;
}

bool StyleSystem::addStyle(StringView css) {
	_sources.emplace_back(StyleSource{false, String(css.data(), css.size())});
	return rebuildFromSources();
}

bool StyleSystem::addStyle(const FileInfo &file) {
	_sources.emplace_back(StyleSource{true, String(file.path.data(), file.path.size()),
		file.category, file.flags});
	auto ret = rebuildFromSources();
	if (_running) {
		// re-arm watches so the newly added file is watched too
		cancelWatches();
		registerWatches();
	}
	return ret;
}

bool StyleSystem::rebuildFromSources() {
	auto sheet = Rc<StyleSheet>::create();
	if (!sheet) {
		return false;
	}
	for (auto &s : _sources) {
		if (s.file) {
			sheet->addStyle(FileInfo(StringView(s.value), s.category, s.flags));
		} else {
			sheet->addStyle(StringView(s.value));
		}
	}
	// setStyleSheet bumps the version and invalidates the subtree (no-op when no owner yet)
	setStyleSheet(sp::move(sheet));
	return true;
}

void StyleSystem::registerWatches() {
	if (!_owner) {
		return;
	}
	auto dir = _owner->getDirector();
	if (!dir) {
		return;
	}
	auto app = dir->getApplication();
	if (!app) {
		return;
	}
	auto looper = app->getLooper();
	if (!looper) {
		return;
	}

	for (auto &s : _sources) {
		if (!s.file) {
			continue;
		}

		// watchFile needs a concrete on-disk path; resolve the FileInfo now
		auto real = filesystem::findPath<mem_std::Interface>(
				FileInfo(StringView(s.value), s.category, s.flags), filesystem::Access::Read);
		if (real.empty()) {
			continue;
		}

		auto handle = looper->watchFile(StringView(real),
				sprt::dispatch::WatchFlags::Modified | sprt::dispatch::WatchFlags::MovedTo
						| sprt::dispatch::WatchFlags::Created,
				[this](sprt::dispatch::WatchFlags) -> sprt::Status {
			// the app looper runs on the director thread, so this fires on the same thread
			// as handleEnter/handleExit: the reload + invalidation can run in place

			auto dir = _owner->getDirector();
			if (dir && dir->getRenderServer()) {
				dir->getRenderServer()->setReadyForNextFrame();
			}

			rebuildFromSources();
			return sprt::Status::Ok;
		}, this);
		if (handle) {
			_watches.emplace_back(sp::move(handle));
		}
	}
}

void StyleSystem::cancelWatches() {
	for (auto &w : _watches) {
		if (w) {
			w->cancel();
		}
	}
	_watches.clear();
}

void StyleSystem::setStyleSheet(Rc<StyleSheet> &&sheet) {
	_sheet = move(sheet);
	_resolvedForVersion = maxOf<uint32_t>();
	invalidateStyles();
}

void StyleSystem::setMediaParameters(const document::MediaParameters &media) {
	_media = media;
	_mediaExplicit = true;
	_resolvedForVersion = maxOf<uint32_t>();
	invalidateStyles();
}

SpanView<bool> StyleSystem::getMediaResolved() {
	if (_sheet && _resolvedForVersion != _sheet->getVersion()) {
		_mediaResolved = _sheet->resolveMedia(_media);
		_resolvedForVersion = _sheet->getVersion();
	}
	return _mediaResolved;
}

void StyleSystem::invalidateStyles() {
	if (_owner) {
		_owner->setOrUpdateComponent<StyleSystemState>([&](NotNull<StyleSystemState> state) {
			++state->version;
			return true;
		});
	}
}

void StyleSystem::handleAdded(Node *owner) {
	System::handleAdded(owner);
	owner->setComponent<StyleSystemState>();
}

void StyleSystem::handleRemoved() {
	cancelWatches();
	if (_owner) {
		_owner->removeComponent<StyleSystemState>();
	}
	System::handleRemoved();
}

void StyleSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);
	updateMedia();
	registerWatches();

	_owner->setOrUpdateComponent<StyleSystemState>([&](NotNull<StyleSystemState> state) {
		state->systemId = _systemId;
		state->version = 0;
		return true;
	});
}

void StyleSystem::handleExit() {
	cancelWatches();
	System::handleExit();
}

void StyleSystem::updateMedia() {
	if (_mediaExplicit || !_owner) {
		return;
	}

	if (auto dir = _owner->getDirector()) {
		auto &constraints = dir->getFrameConstraints();
		auto screen = constraints.getScreenSize();
		if (constraints.density > 0.0f) {
			_media.density = constraints.density;
			_media.dpi = int(92.0f * constraints.density);
			_media.surfaceSize = Size2(float(screen.width) / constraints.density,
					float(screen.height) / constraints.density);
			_resolvedForVersion = maxOf<uint32_t>();
		}
	}
}

} // namespace stappler::xenolith::ui
