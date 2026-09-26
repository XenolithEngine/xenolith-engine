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


#include "ShadeScene.h"

#include "XL2dSceneContent.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dIcons.h"
#include "XLUiPanel.h"
#include "XLUiButton.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"
#include "XLInputListener.h"
#include "XLAction.h"
#include "XLActionEase.h"
#include "XLSceneInspector.h"
#include "XLAppThread.h"

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/utils/time.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

static constexpr StringView ShadeScene_css = R"(
#bar {
	display: flex; flex-direction: row; align-items: center;
	gap: 12px; padding-left: 12px; padding-right: 12px;
	background-color: #0d1117;
	color: #e6edf3;
	font-size: 14px;
}
#indicators { flex-grow: 1; }
button.bar-button {
	width: 40px; height: 28px; border-radius: 6px;
	background-color: transparent;
	color: #e6edf3;
}
button.bar-button:hover { background-color: #30363d; }
button.bar-button > icon { width: 20px; height: 20px; }
#panel {
	display: flex; flex-direction: column; gap: 8px; padding: 16px;
	background-color: #161b22;
	color: #e6edf3;
	font-size: 16px;
}
panel.row {
	display: flex; flex-direction: row; align-items: center; gap: 8px;
	height: 48px; border-radius: 8px;
	background-color: #21262d;
}
button.row-title {
	flex-grow: 1; height: 48px; padding: 0px 12px;
	background-color: transparent;
	color: #e6edf3;
}
button.row-close {
	width: 48px; height: 48px;
	background-color: transparent;
	color: #e6edf3;
}
button.row-title:hover, button.row-close:hover { background-color: #30363d; }
)";

static constexpr uint32_t ShadeScene_slideTag = 1;
static constexpr float ShadeScene_slideTime = 0.25f;

// The panel's share of the screen below the bar.
static constexpr float ShadeScene_panelShare = 0.6f;

static constexpr uint64_t ShadeScene_retryDelay = 500'000;
static constexpr uint32_t ShadeScene_maxAttempts = 20;

static Value ShadeScene_encodeRect(const Node *node) {
	auto size = node->getContentSize();
	auto bl = node->convertToWorldSpace(Vec2::ZERO);
	auto tr = node->convertToWorldSpace(Vec2(size.width, size.height));
	return Value({Value(bl.x), Value(bl.y), Value(tr.x - bl.x), Value(tr.y - bl.y)});
}

bool ShadeScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints) {
	if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	auto content = Rc<basic2d::SceneContent2d>::create();
	content->addSystem(Rc<ui::StyleSystem>::create(ShadeScene_css));
	content->addSystem(Rc<ui::StyleResolver>::create(true));

	// Nothing else is drawn: the plane is transparent where the shade has no content.
	_backdrop = content->addChild(Rc<basic2d::Layer>::create(Color4F(0.0f, 0.0f, 0.0f, 0.5f)),
			ZOrder(1));
	_backdrop->setAnchorPoint(Anchor::BottomLeft);
	_backdrop->setVisible(false);
	auto backdropListener = _backdrop->addSystem(Rc<InputListener>::create());
	backdropListener->addTapRecognizer([this](const GestureTap &) {
		close();
		return true;
	});

	_panel = content->addChild(Rc<ui::Panel>::create(), ZOrder(2));
	_panel->setName("panel");
	_panel->setAnchorPoint(Anchor::TopLeft);
	_panel->setVisible(false);

	_bar = content->addChild(Rc<ui::Panel>::create(), ZOrder(3));
	_bar->setName("bar");
	_bar->setAnchorPoint(Anchor::TopLeft);

	_clock = _bar->addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_clock->setName("clock");
	_clock->setDeferred(false);

	_indicators = _bar->addChild(Rc<basic2d::Label>::create(), ZOrder(2));
	_indicators->setName("indicators");
	_indicators->setDeferred(false);

	_home = _bar->addChild(Rc<ui::Button>::create([this] {
		if (_client) {
			_client->notify(protocol::makeMessage(protocol::Home));
		}
		close();
	}),
			ZOrder(3));
	_home->setName("home");
	_home->addStyleClass("bar-button");
	_home->setIcon(basic2d::IconName::Action_home_solid);

	_apps = _bar->addChild(Rc<ui::Button>::create([this] {
		if (_opened) {
			close();
		} else {
			open();
		}
	}),
			ZOrder(4));
	_apps->setName("apps");
	_apps->addStyleClass("bar-button");
	_apps->setIcon(basic2d::IconName::Navigation_apps_solid);

	setContent(content);
	setFpsVisible(false);
	updateClock();
	registerCommands();
	return true;
}

void ShadeScene::handleEnter(Scene *scene) {
	Scene2d::handleEnter(scene);

	if (!_client) {
		_client = Rc<WmClient>::create(_director);
		if (_client) {
			_client->setHandler([this](Value &&value) { handleMessage(sp::move(value)); });
		}
	}

	sendInputRegion(false);
	requestApps();
	scheduleClock();
}

void ShadeScene::handleExit() {
	if (_clockTimer) {
		_clockTimer->cancel();
		_clockTimer = nullptr;
	}
	if (_retry) {
		_retry->cancel();
		_retry = nullptr;
	}
	Scene2d::handleExit();
}

void ShadeScene::handleContentSizeDirty() {
	Scene2d::handleContentSizeDirty();

	auto bar = protocol::StatusBarHeight;
	_bar->setPosition(Vec2(0.0f, _contentSize.height));
	_bar->setContentSize(Size2(_contentSize.width, bar));

	_backdrop->setPosition(Vec2::ZERO);
	_backdrop->setContentSize(Size2(_contentSize.width, _contentSize.height - bar));

	_panel->setContentSize(
			Size2(_contentSize.width, (_contentSize.height - bar) * ShadeScene_panelShare));
	_panel->stopAllActionsByTag(ShadeScene_slideTag);
	_panel->setPosition(getPanelPosition(_opened));
}

void ShadeScene::open() {
	if (_opened) {
		return;
	}
	_opened = true;

	// The whole screen is the shade's before the panel moves, and the keyboard with it
	sendInputRegion(true);
	if (_client) {
		_client->notify(protocol::makeShade(true));
	}

	_panel->stopAllActionsByTag(ShadeScene_slideTag);
	_panel->setVisible(true);
	_panel->runAction(Rc<EaseActionTyped>::create(
							  Rc<MoveTo>::create(ShadeScene_slideTime, getPanelPosition(true)),
							  interpolation::Type::QuadEaseOut),
			ShadeScene_slideTag);

	_backdrop->stopAllActionsByTag(ShadeScene_slideTag);
	_backdrop->setVisible(true);
	_backdrop->setOpacity(0.0f);
	_backdrop->runAction(Rc<FadeTo>::create(ShadeScene_slideTime, 1.0f), ShadeScene_slideTag);
}

void ShadeScene::close() {
	if (!_opened) {
		return;
	}
	_opened = false;

	_backdrop->stopAllActionsByTag(ShadeScene_slideTag);
	_backdrop->runAction(Rc<FadeTo>::create(ShadeScene_slideTime, 0.0f), ShadeScene_slideTag);

	// The input and the keyboard go back once the panel is gone
	_panel->stopAllActionsByTag(ShadeScene_slideTag);
	_panel->runAction(Rc<Sequence>::create(
							  Rc<EaseActionTyped>::create(Rc<MoveTo>::create(ShadeScene_slideTime,
																  getPanelPosition(false)),
									  interpolation::Type::QuadEaseOut),
							  [this] {
		if (_opened) {
			return;
		}
		_panel->setVisible(false);
		_backdrop->setVisible(false);
		sendInputRegion(false);
		if (_client) {
			_client->notify(protocol::makeShade(false));
		}
	}),
			ShadeScene_slideTag);
}

void ShadeScene::describeQueue(QueueInfo &info) { info.type = QueueType::Flat; }

void ShadeScene::handleMessage(Value &&value) {
	const Value &msg = value;
	auto type = protocol::getType(msg);
	if (type == protocol::Apps) {
		setApps(protocol::readApps(msg));
	} else if (type == protocol::Status) {
		setStatus(protocol::readStatus(msg));
	}
}

void ShadeScene::setApps(Vector<protocol::RunningApp> &&apps) {
	for (auto &it : _rows) { it.row->removeFromParent(); }
	_rows.clear();

	int16_t z = 1;
	for (auto &it : apps) {
		auto row = _panel->addChild(Rc<ui::Panel>::create(), ZOrder(z++));
		row->setType("panel");
		row->addStyleClass("row");

		auto label = it.label;
		auto title = row->addChild(Rc<ui::Button>::create(it.title.empty() ? it.label : it.title,
										   [this, label] {
			if (_client) {
				_client->notify(protocol::makeLabelled(protocol::Switch, label));
			}
			close();
		}),
				ZOrder(1));
		title->addStyleClass("row-title");

		auto close = row->addChild(Rc<ui::Button>::create([this, label] {
			if (_client) {
				_client->notify(protocol::makeLabelled(protocol::Close, label));
			}
		}),
				ZOrder(2));
		close->addStyleClass("row-close");
		close->setIcon(basic2d::IconName::Navigation_close_solid);

		_rows.emplace_back(Row{sp::move(it), row, title, close});
	}
}

void ShadeScene::setStatus(const protocol::Stats &stats) {
	_stats = stats;
	_indicators->setString(toString(stats.apps, " apps   CPU ", int(stats.cpu + 0.5f), "%   RAM ",
			stats.rss / (1'024 * 1'024), " MB   ", int(stats.fps + 0.5f), " / ",
			int(stats.appFps + 0.5f), " fps"));
}

void ShadeScene::updateClock() {
	sprt::time::time_exp_t local(int64_t(Time::now().toMicros()), true);
	char buf[16] = {0};
	auto len = local.strftime(buf, sizeof(buf), "%H:%M");
	_clock->setString(StringView(buf, len));
}

void ShadeScene::scheduleClock() {
	if (!_director || _clockTimer) {
		return;
	}

	// A timer, not an action: a running action would make the client draw every frame.
	_clockTimer = _director->getApplication()->getLooper()->schedule(
			sprt::dispatch::TimeInterval::seconds(1),
			[this](sprt::dispatch::Handle *, bool success) {
		_clockTimer = nullptr;
		if (success) {
			updateClock();
			scheduleClock();
		}
	}, this);
}

void ShadeScene::requestApps() {
	if (!_client || !_director) {
		return;
	}

	auto retry = [this] {
		if (++_attempts >= ShadeScene_maxAttempts) {
			log::source().error("ShadeScene", "the server gives no application list");
			return;
		}
		_retry = _director->getApplication()->getLooper()->schedule(
				sprt::dispatch::TimeInterval::microseconds(ShadeScene_retryDelay),
				[this](sprt::dispatch::Handle *, bool success) {
			_retry = nullptr;
			if (success) {
				requestApps();
			}
		}, this);
	};

	auto sent = _client->request(protocol::makeMessage(protocol::ListApps),
			[this, retry](Status st, Value &&value) {
		if (sprt::status::isSuccessful(st)) {
			const Value &reply = value;
			setApps(protocol::readApps(reply));
		} else {
			retry();
		}
	});
	if (!sent) {
		retry();
	}
}

void ShadeScene::sendInputRegion(bool whole) {
	if (!_client) {
		return;
	}

	auto &extent = _constraints.extent;
	auto bar = uint32_t(protocol::StatusBarHeight * _constraints.density + 0.5f);
	IRect rect = whole ? IRect(0, 0, extent.width, extent.height) : IRect(0, 0, extent.width, bar);
	_client->notify(protocol::makeInputRegion(makeSpanView(&rect, 1)));
}

Vec2 ShadeScene::getPanelPosition(bool opened) const {
	auto top = _contentSize.height - protocol::StatusBarHeight;
	return Vec2(0.0f, opened ? top : top + _panel->getContentSize().height);
}

void ShadeScene::registerCommands() {
	/* The shade's controls in the window's pixels (y up): what a check taps on the host. */
	inspector::addCommand(getContent(), "shade-state",
			"The bar and the panel: { open, clock, indicators, home, apps, rows }",
			[this](Value &&, Function<void(Value &&)> &&done) {
		Value ret;
		ret.setBool(true, "ok");
		ret.setBool(_opened, "open");
		ret.setBool(_panel->isVisible(), "panelVisible");
		ret.setString(_clock->getString8(), "clock");
		ret.setString(_indicators->getString8(), "indicators");
		ret.setInteger(_stats.apps, "statusApps");
		ret.setValue(ShadeScene_encodeRect(_home), "home");
		ret.setValue(ShadeScene_encodeRect(_apps), "apps");
		ret.setValue(ShadeScene_encodeRect(_bar), "bar");
		auto &rows = ret.emplace("rows");
		rows.setArray(Value::ArrayType());
		for (auto &it : _rows) {
			Value row;
			row.setString(it.app.label, "label");
			row.setString(it.app.title, "title");
			row.setBool(it.app.active, "active");
			row.setValue(ShadeScene_encodeRect(it.title), "rect");
			row.setValue(ShadeScene_encodeRect(it.close), "closeRect");
			rows.addValue(sp::move(row));
		}
		done(sp::move(ret));
	});
}

} // namespace stappler::xenolith::wm
