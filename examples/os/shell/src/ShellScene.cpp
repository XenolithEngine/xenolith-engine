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


#include "ShellScene.h"

#include "XL2dSceneContent.h"
#include "XL2dIcons.h"
#include "XLUiPanel.h"
#include "XLUiButton.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"
#include "XLSceneInspector.h"
#include "XLAppThread.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

static constexpr StringView ShellScene_css = R"(
#shell {
	display: flex; flex-direction: row; flex-wrap: wrap;
	align-content: flex-start; align-items: flex-start; justify-content: flex-start;
	gap: 24px; padding: 32px;
	background-color: #263238;
}
button.tile {
	display: flex; flex-direction: column; align-items: center; justify-content: center;
	width: 136px; height: 120px; gap: 8px;
	border-radius: 16px;
	background-color: #37474f;
	color: #eceff1;
	font-size: 15px;
}
button.tile:hover { background-color: #455a64; }
button.tile > icon { order: -1; width: 48px; height: 48px; }
button.tile:disabled { opacity: 0.4; }
)";

// Catalog requests before the session answers them are retried this often, this many times.
static constexpr uint64_t ShellScene_retryDelay = 500'000;
static constexpr uint32_t ShellScene_maxAttempts = 20;

static basic2d::IconName ShellScene_getIcon(StringView name) {
	for (uint16_t i = 0; i < toInt(basic2d::IconName::Max); ++i) {
		if (basic2d::getIconName(basic2d::IconName(i)) == name) {
			return basic2d::IconName(i);
		}
	}
	return basic2d::IconName::Action_launch_solid;
}

bool ShellScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints) {
	if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	auto content = Rc<basic2d::SceneContent2d>::create();
	content->addSystem(Rc<ui::StyleSystem>::create(ShellScene_css));
	content->addSystem(Rc<ui::StyleResolver>::create(true));

	_grid = content->addChild(Rc<ui::Panel>::create());
	_grid->setName("shell");
	_grid->setAnchorPoint(Anchor::BottomLeft);

	setContent(content);
	setFpsVisible(false);
	registerCommands();
	return true;
}

void ShellScene::handleEnter(Scene *scene) {
	Scene2d::handleEnter(scene);

	if (!_client) {
		_client = Rc<WmClient>::create(_director);
		if (_client) {
			// the protocol sends the shell nothing unasked
			_client->setHandler(nullptr);
		}
	}
	requestCatalog();
}

void ShellScene::handleExit() {
	if (_retry) {
		_retry->cancel();
		_retry = nullptr;
	}
	Scene2d::handleExit();
}

void ShellScene::handleContentSizeDirty() {
	Scene2d::handleContentSizeDirty();
	if (_grid) {
		_grid->setPosition(Vec2::ZERO);
		_grid->setContentSize(_contentSize);
	}
}

void ShellScene::describeQueue(QueueInfo &info) { info.type = QueueType::Flat; }

void ShellScene::requestCatalog() {
	if (!_client || !_director) {
		return;
	}

	auto retry = [this] {
		if (++_attempts >= ShellScene_maxAttempts) {
			log::source().error("ShellScene", "the server gives no catalog");
			return;
		}
		_retry = _director->getApplication()->getLooper()->schedule(
				sprt::dispatch::TimeInterval::microseconds(ShellScene_retryDelay),
				[this](sprt::dispatch::Handle *, bool success) {
			_retry = nullptr;
			if (success) {
				requestCatalog();
			}
		}, this);
	};

	auto sent = _client->request(protocol::makeMessage(protocol::Catalog),
			[this, retry](Status st, Value &&value) {
		if (sprt::status::isSuccessful(st)) {
			const Value &reply = value;
			setCatalog(protocol::readCatalog(reply));
		} else {
			retry();
		}
	});
	if (!sent) {
		retry();
	}
}

void ShellScene::setCatalog(Vector<protocol::CatalogEntry> &&entries) {
	for (auto &it : _tiles) { it.button->removeFromParent(); }
	_tiles.clear();

	int16_t z = 1;
	for (auto &it : entries) {
		auto id = it.id;
		auto button = _grid->addChild(Rc<ui::Button>::create(it.title,
											  [this, id] {
			if (_client) {
				_client->notify(protocol::makeLaunch(id));
			}
		}),
				ZOrder(z++));
		button->setName(it.id);
		button->addStyleClass("tile");
		button->setIcon(ShellScene_getIcon(it.icon));
		button->setEnabled(it.available);
		_tiles.emplace_back(Tile{sp::move(it), button});
	}
}

void ShellScene::registerCommands() {
	/* Where each tile is, in the window's pixels (y up): what a check taps on the host. */
	inspector::addCommand(getContent(), "shell-state", "The tiles: { tiles: [{ id, rect }] }",
			[this](Value &&, Function<void(Value &&)> &&done) {
		Value ret;
		ret.setBool(true, "ok");
		auto &tiles = ret.emplace("tiles");
		tiles.setArray(Value::ArrayType());
		for (auto &it : _tiles) {
			Value tile;
			tile.setString(it.entry.id, "id");
			tile.setString(it.entry.title, "title");
			tile.setBool(it.entry.available, "available");
			auto size = it.button->getContentSize();
			auto bl = it.button->convertToWorldSpace(Vec2::ZERO);
			auto tr = it.button->convertToWorldSpace(Vec2(size.width, size.height));
			tile.setValue(Value({Value(bl.x), Value(bl.y), Value(tr.x - bl.x), Value(tr.y - bl.y)}),
					"rect");
			tiles.addValue(sp::move(tile));
		}
		done(sp::move(ret));
	});
}

} // namespace stappler::xenolith::wm
