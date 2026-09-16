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
#include "XLEventListener.h"
#include "XLFontLocale.h" // the `rtl` media flag follows the locale
#include "XLAppThread.h"
#include "SPFilesystem.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/event.h> // WatchFlags

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

ComponentId StyleSystemState::Id;
ComponentId StyleVariables::Id;

// custom property names are case-insensitive here (unlike the web) and stored with the leading
// "--", matching sheet-declared names
static String normalizeVariableName(StringView name) {
	String result;
	result.reserve(name.size() + 2);
	if (!name.starts_with("--")) {
		result.append("--");
	}
	for (size_t i = 0; i < name.size(); ++i) {
		auto c = name[i];
		result.push_back((c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c);
	}
	return result;
}

StringView StyleVariables::get(StringView name) const {
	auto it = vars.find(normalizeVariableName(name));
	return (it == vars.end()) ? StringView() : StringView(it->second);
}

bool setStyleVariable(NotNull<Node> node, StringView name, StringView value) {
	auto key = normalizeVariableName(name);
	bool changed = false;
	node->setOrUpdateComponent<StyleVariables>([&](NotNull<StyleVariables> vars) {
		auto it = vars->vars.find(key);
		if (it != vars->vars.end()) {
			if (StringView(it->second) == value) {
				return false;
			}
			it->second = value.str<Interface>();
		} else {
			vars->vars.emplace(sp::move(key), value.str<Interface>());
		}
		changed = true;
		return true;
	});
	return changed;
}

bool removeStyleVariable(NotNull<Node> node, StringView name) {
	auto existing = node->getComponent<StyleVariables>();
	if (!existing) {
		return false;
	}

	auto key = normalizeVariableName(name);
	if (existing->vars.find(key) == existing->vars.end()) {
		return false;
	}

	node->setOrUpdateComponent<StyleVariables>([&](NotNull<StyleVariables> vars) {
		vars->vars.erase(key);
		return true;
	});
	return true;
}

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

	// added here, subscribed on enter: a system added during handleEnter would miss the pass
	if (!_localeListener) {
		_localeListener = owner->addSystem(Rc<EventListener>::create());
	}
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

	// follow locale changes; guarded so repeated enters leave one delegate
	if (_localeListener && !_localeDelegate) {
		_localeDelegate = _localeListener->listenForEvent(locale::onLocale, [this](const Event &) {
			setMediaOption(StringView("rtl"),
					locale::getTextDirection() == font::TextDirection::RightToLeft);
		});
	}

	_owner->setOrUpdateComponent<StyleSystemState>([&](NotNull<StyleSystemState> state) {
		state->systemId = _systemId;
		state->version = 0;
		return true;
	});
}

void StyleSystem::handleExit() {
	// the listener stays with the owner; drop the delegate so re-entering subscribes once
	_localeDelegate = nullptr;
	cancelWatches();
	System::handleExit();
}

// mark every descendant so its resolver re-resolves; recursive, since this runs outside a pass
static void StyleSystem_markSubtree(Node *node) {
	for (auto &child : node->getChildren()) {
		child->markContentSizeDirty();
		StyleSystem_markSubtree(child);
	}
}

void StyleSystem::setMediaOption(StringView name, bool enabled) {
	if (_media.hasOption(name) == enabled) {
		return;
	}
	if (enabled) {
		_media.addOption(name);
	} else {
		_media.removeOption(name);
	}
	// re-evaluate media queries and bump the version for resolvers in the subtree
	_resolvedForVersion = maxOf<uint32_t>();
	invalidateStyles();

	// a version bump is only read during a pass, so request one over the whole subtree: each
	// resolver re-resolves a node only when that node's own phase fires
	if (_owner) {
		_owner->markContentSizeDirty();
		StyleSystem_markSubtree(_owner);
	}
}

void StyleSystem::updateMedia() {
	if (_mediaExplicit || !_owner) {
		return;
	}

	// seed the `rtl` media flag from the locale for every StyleSystem, including popup windows
	// that inherit nothing from their parent window
	setMediaOption(StringView("rtl"),
			locale::getTextDirection() == font::TextDirection::RightToLeft);

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
