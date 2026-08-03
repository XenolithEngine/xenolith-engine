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

#include "XLCommon.h"

#include "TestLayout.h"
#include "TestRegistry.h"
#include "XL2dScene.h"
#include "XLSceneContent.h"
#include "XLSceneInspector.h"
#include "XLUiStyleSystem.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Above anything a test is likely to use, so the caption stays readable even over a test that
// fills its whole area.
static constexpr ZOrder CaptionZOrder = ZOrder(1'000);

bool TestLayout::init() {
	if (!SceneLayout2d::init()) {
		return false;
	}

	// Built up front but left empty: a layout that never receives a TestInfo keeps them hidden and
	// reports a caption height of zero, so it behaves exactly as it did before the caption existed.
	_captionBackground = addChild(Rc<basic2d::Layer>::create(Color::Grey_900), CaptionZOrder);
	_captionBackground->setVisible(false);

	_captionTitle = addChild(Rc<basic2d::Label>::create(), CaptionZOrder + ZOrder(1));
	_captionTitle->setFontSize(22);
	_captionTitle->setColor(Color::White);
	_captionTitle->setVisible(false);

	_captionDescription = addChild(Rc<basic2d::Label>::create(), CaptionZOrder + ZOrder(1));
	_captionDescription->setFontSize(16);
	_captionDescription->setColor(Color::Grey_400);
	_captionDescription->setVisible(false);

	// The engine renders on demand: with nothing dirty it stops producing frames, and a scheduled
	// action or a state change made off-screen is only picked up the next time something wakes the
	// loop (moving the mouse over the window). Every test here either animates, runs timed phases,
	// or is watched for reactivity, so hold the loop open for all of them - a lone RenderContinuously
	// changes nothing on screen and damages nothing, it only keeps the frames coming.
	runAction(Rc<RenderContinuously>::create());

	return true;
}

void TestLayout::setTestInfo(const TestInfo &info) {
	_info = &info;

	// The FPS counter is marked AlwaysDirty, so it damages a region every frame - which a test
	// about damage tracking can not tolerate. Restored when the test is left, so it also works
	// when the test is opened from the menu.
	_hideFps = info.hideFps;

	// The front page carries no selecting variable and gets no caption: it is the app itself, not
	// a test, and a caption over it would just take space from the menu.
	_hasCaption = !info.env.empty();

	_captionBackground->setVisible(_hasCaption);
	_captionTitle->setVisible(_hasCaption);
	_captionDescription->setVisible(_hasCaption);

	if (!_hasCaption) {
		return;
	}

	// The selecting variable goes on screen too: it is what someone watching the window needs in
	// order to reproduce the run.
	_captionTitle->setString(string::toString<Interface>(info.title, "   ", info.env));
	_captionDescription->setString(info.description);

	_contentSizeDirty = true;
}

void TestLayout::addCommand(StringView name, StringView description, CommandHandler &&handler) {
	// A layout built outside the registry (an overlay a test pushes) has no name to key commands
	// on, and nothing external ever asks for it - leave it alone.
	if (!_info || _info->name.empty() || !_scene || !handler) {
		return;
	}

	auto content = _scene->getContent();
	auto full = toString(_info->name, ".", name);

	if (!inspector::addCommand(content, full, description,
				[this, handler = sp::move(handler)](Value &&args,
						Function<void(Value &&)> &&done) mutable {
		auto settle = args.hasValue("settle") ? float(args.getDouble("settle")) : DefaultSettle;
		auto result = handler(sp::move(args));

		// Answer only once the change has been on screen for `settle` seconds. Every test layout
		// holds a RenderContinuously (see init), so that is real rendering time and a screenshot
		// taken when the reply lands shows the settled scene, not a half-finished relayout.
		runAction(Rc<Sequence>::create(settle,
				Function<void()>([done = sp::move(done), result = sp::move(result)]() mutable {
			done(sp::move(result));
		})));
	})) {
		return; // no inspector on this scene - the app was not built with one
	}

	_registeredCommands.emplace_back(sp::move(full));
}

void TestLayout::handleEnter(Scene *scene) {
	SceneLayout2d::handleEnter(scene);

	if (_hideFps) {
		if (auto s = dynamic_cast<basic2d::Scene2d *>(scene)) {
			_fpsWasVisible = s->isFpsVisible();
			s->setFpsVisible(false);
		}
	}

	registerCommands();
}

void TestLayout::handleExit() {
	if (!_registeredCommands.empty()) {
		if (auto i = inspector::get(_scene ? _scene->getContent() : nullptr)) {
			for (auto &it : _registeredCommands) { i->removeCommand(it); }
		}
		_registeredCommands.clear();
	}

	if (_hideFps && _fpsWasVisible) {
		if (auto s = dynamic_cast<basic2d::Scene2d *>(_scene)) {
			s->setFpsVisible(true);
		}
		_fpsWasVisible = false;
	}

	SceneLayout2d::handleExit();
}

float TestLayout::getCaptionHeight() const { return _hasCaption ? CaptionHeight : 0.0f; }

float TestLayout::getWorkTop() const { return getContentSize().height - getCaptionHeight(); }

Size2 TestLayout::getWorkSize() const {
	const auto cs = getContentSize();
	return Size2(cs.width, sprt::max(cs.height - getCaptionHeight(), 0.0f));
}

void TestLayout::setStyleSheet(StringView css) { addSystem(Rc<ui::StyleSystem>::create(css)); }

void TestLayout::setStyleSheet(const FileInfo &file) {
	addSystem(Rc<ui::StyleSystem>::create(file));
}

void TestLayout::handleContentSizeDirty() {
	SceneLayout2d::handleContentSizeDirty();

	if (!_hasCaption) {
		return;
	}

	const auto cs = getContentSize();

	_captionBackground->setAnchorPoint(Anchor::BottomLeft);
	_captionBackground->setContentSize(Size2(cs.width, CaptionHeight));
	_captionBackground->setPosition(Vec2(0.0f, cs.height - CaptionHeight));

	_captionTitle->setAnchorPoint(Vec2(0.0f, 1.0f));
	_captionTitle->setPosition(Vec2(24.0f, cs.height - 12.0f));

	_captionDescription->setAnchorPoint(Vec2(0.0f, 1.0f));
	_captionDescription->setWidth(sprt::max(cs.width - 48.0f, 0.0f));
	_captionDescription->setPosition(Vec2(24.0f, cs.height - 44.0f));
}

} // namespace stappler::xenolith::app
