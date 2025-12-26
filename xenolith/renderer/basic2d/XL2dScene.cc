/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XL2dLayer.h"
#include "XL2dLabel.h"
#include "XL2dScene.h"
#include "XL2dVectorSprite.h"
#include "XLContext.h"
#include "XLDirector.h"
#include "XLInputListener.h"
#include "XLSceneContent.h"
#include "XLAppWindow.h"
#include "backend/vk/XL2dVkShadowPass.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class Scene2d::FpsDisplay : public Layer {
public:
	enum DisplayMode {
		Fps,
		Vertexes,
		Cache,
		Full,
		Disabled,
	};

	virtual ~FpsDisplay() { }

	virtual bool init() override;
	virtual void update(const UpdateTime &) override;

	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	void incrementMode();

	void hide();
	void show();

protected:
	using Layer::init;

	uint32_t _frames = 0;
	Label *_label = nullptr;
	DisplayMode _mode = Fps;
};

bool Scene2d::FpsDisplay::init() {
	if (!Layer::init(Color::White)) {
		return false;
	}

	_label = addChild(Rc<Label>::create(), Node::ZOrderMax);
	_label->setString("0.0\n0.0\n0.0\n0 0 0 0");
	_label->setFontFamily("monospace");
	_label->setAnchorPoint(Anchor::BottomLeft);
	_label->setColor(Color::Black, true);
	_label->setFontSize(16);
	_label->setContentSizeDirtyCallback([this] { setContentSize(_label->getContentSize()); });
	_label->setPersistentGlyphData(true);
	_label->addCommandFlags(CommandFlags::DoNotCount);

	addCommandFlags(CommandFlags::DoNotCount);
	scheduleUpdate();

	return true;
}

void Scene2d::FpsDisplay::update(const UpdateTime &) {
	if (_director) {
		auto fps = _director->getAvgFps();
		auto spf = _director->getSpf();
		auto fenceTime = _director->getFenceFrameTime();
		auto timestampTime = _director->getTimestampFrameTime();
		auto stat = _director->getDrawStat();
		auto tm = _director->getDirectorFrameTime();
		auto vertex = stat.vertexInputTime / float(1'000);

		auto &cfg = _director->getWindow()->getAppSwapchainConfig();

		String configData;
		switch (cfg.presentMode) {
		case core::PresentMode::Unsupported: configData = toString("U", cfg.imageCount); break;
		case core::PresentMode::Immediate: configData = toString("I", cfg.imageCount); break;
		case core::PresentMode::FifoRelaxed: configData = toString("Fr", cfg.imageCount); break;
		case core::PresentMode::Fifo: configData = toString("F", cfg.imageCount); break;
		case core::PresentMode::Mailbox: configData = toString("M", cfg.imageCount); break;
		}

		if (_label) {
			String str;
			switch (_mode) {
			case Fps:
				str = toString(configData, " ", std::setprecision(3), "FPS: ", fps, " SPF: ", spf,
						"\nGPU: ", fenceTime, " (", timestampTime, ")", "\nDir: ", tm,
						" Ver: ", vertex, "\nF12 to switch");
				break;
			case Vertexes:
				str = toString(std::setprecision(3), "V:", stat.vertexes, " T:", stat.triangles,
						"\nZ:", stat.zPaths, " C:", stat.drawCalls, " M: ", stat.materials, "\n",
						stat.solidCmds, "/", stat.surfaceCmds, "/", stat.transparentCmds,
						"\nF12 to switch");
				break;
			case Cache:
				str = toString(std::setprecision(3), "Cache:", stat.cachedFramebuffers, "/",
						stat.cachedImages, "/", stat.cachedImageViews, "\nF12 to switch");
				break;
			case Full:
				str = toString(configData, " ", std::setprecision(3), "FPS: ", fps, " SPF: ", spf,
						"\nGPU: ", fenceTime, " (", timestampTime, ")", "\nDir: ", tm,
						" Ver: ", vertex, "\n", "V:", stat.vertexes, " T:", stat.triangles,
						"\nZ:", stat.zPaths, " C:", stat.drawCalls, " M: ", stat.materials, "\n",
						stat.solidCmds, "/", stat.surfaceCmds, "/", stat.transparentCmds, "\n",
						"Cache:", stat.cachedFramebuffers, "/", stat.cachedImages, "/",
						stat.cachedImageViews, "\nF12 to switch");
				break;
			default: break;
			}
			_label->setString(str);
		}
		++_frames;
	}
}

bool Scene2d::FpsDisplay::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	// place above any shadows
	frame.depthStack.emplace_back(100.0f);
	auto ret = Layer::visitDraw(frame, parentFlags);
	frame.depthStack.pop_back();
	return ret;
}

void Scene2d::FpsDisplay::incrementMode() {
	_mode = DisplayMode(toInt(_mode) + 1);
	if (_mode > Disabled) {
		_mode = Fps;
	}

	setVisible(_mode != Disabled);
}

void Scene2d::FpsDisplay::hide() {
	if (_mode != DisplayMode::Disabled) {
		_mode = DisplayMode::Disabled;
		setVisible(_mode != Disabled);
	}
}

void Scene2d::FpsDisplay::show() {
	if (_mode == DisplayMode::Disabled) {
		_mode = DisplayMode::Fps;
		setVisible(_mode != Disabled);
	}
}

bool Scene2d::init(NotNull<AppThread> app, NotNull<AppWindow> window,
		const core::FrameConstraints &constraints) {
	return init(app, window, [](Queue::Builder &) { }, constraints);
}

bool Scene2d::init(NotNull<AppThread> app, NotNull<AppWindow> window,
		const Callback<void(Queue::Builder &)> &cb, const core::FrameConstraints &constraints) {
	core::Queue::Builder builder("Loader");

	QueueInfo queueInfo{
		Extent2(constraints.extent.width, constraints.extent.height),
		Color4F::WHITE,
	};

	buildQueueResources(queueInfo, builder);

#if MODULE_XENOLITH_BACKEND_VK

	basic2d::vk::ShadowPass::RenderQueueInfo info{
		app->getContext()->getGlLoop(),
		queueInfo.extent,
		basic2d::vk::ShadowPass::Flags::None,
		queueInfo.backgroundColor,
	};

	basic2d::vk::ShadowPass::makeRenderQueue(builder, info);

	cb(builder);

	if (!init(move(builder), constraints)) {
		return false;
	}

	return true;
#else
	log::source().error("Scene2d", "No available GAPI found");
	return false;
#endif
}

bool Scene2d::init(Queue::Builder &&builder, const core::FrameConstraints &constraints) {
	if (!xenolith::Scene::init(move(builder), constraints)) {
		return false;
	}

	initialize();

	return true;
}

void Scene2d::update(const UpdateTime &time) { xenolith::Scene::update(time); }

void Scene2d::handleContentSizeDirty() {
	xenolith::Scene::handleContentSizeDirty();

	if (_fps) {
		_fps->setPosition(Vec2(6.0f, 6.0f));
	}

	_pointerCenter->setPosition(_contentSize / 2.0f);
}

void Scene2d::setFpsVisible(bool value) {
	if (value) {
		_fps->show();
	} else {
		_fps->hide();
	}
}

bool Scene2d::isFpsVisible() const { return _fps->isVisible(); }

void Scene2d::setContent(SceneContent *content) {
	xenolith::Scene::setContent(content);

	addContentNodes(_content);
}

void Scene2d::buildQueueResources(QueueInfo &, core::Queue::Builder &) { }

void Scene2d::initialize() {
	_listener = addSystem(Rc<InputListener>::create());
	_listener->addKeyRecognizer([this](const GestureData &ev) {
		if (ev.event == GestureEvent::Ended) {
			_fps->incrementMode();
		}
		return true;
	}, InputKeyInfo{makeKeyMask({InputKeyCode::F12})});

	_listener->addKeyRecognizer([this](const GestureData &ev) {
		_pointerReal->setVisible(
				ev.event != GestureEvent::Ended && ev.event != GestureEvent::Cancelled);
		_pointerVirtual->setVisible(
				ev.event != GestureEvent::Ended && ev.event != GestureEvent::Cancelled);
		_pointerCenter->setVisible(
				ev.event != GestureEvent::Ended && ev.event != GestureEvent::Cancelled);
		return true;
	}, InputKeyInfo{makeKeyMask({InputKeyCode::LEFT_CONTROL})});

	_listener->addTapRecognizer([this](const GestureTap &ev) {
		if (_fps->isTouched(ev.input->currentLocation)) {
			_fps->incrementMode();
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch}), 1});

	_listener->addTouchRecognizer([this](const GestureData &ev) {
		if ((ev.input->data.getModifiers() & InputModifier::Ctrl) == InputModifier::None) {
			if (_data1.event != InputEventName::End && _data1.event != InputEventName::Cancel) {

				updateInputEventData(_data1, ev.input->data,
						_content->convertToWorldSpace(_pointerReal->getPosition().xy()),
						maxOf<uint32_t>() - 1);
				updateInputEventData(_data2, ev.input->data,
						_content->convertToWorldSpace(_pointerVirtual->getPosition().xy()),
						maxOf<uint32_t>() - 2);

				_data1.event = InputEventName::Cancel;
				_data2.event = InputEventName::Cancel;

				Vector<InputEventData> events{_data1, _data2};

				_scene->getDirector()->getWindow()->handleInputEvents(sp::move(events));
			}
			return false;
		}

		if (ev.event == GestureEvent::Began) {
			_listener->setExclusiveForTouch(ev.input->data.id);
		}

		updateInputEventData(_data1, ev.input->data,
				_content->convertToWorldSpace(_pointerReal->getPosition().xy()),
				maxOf<uint32_t>() - 1);
		updateInputEventData(_data2, ev.input->data,
				_content->convertToWorldSpace(_pointerVirtual->getPosition().xy()),
				maxOf<uint32_t>() - 2);

		Vector<InputEventData> events{_data1, _data2};

		_scene->getDirector()->getWindow()->handleInputEvents(sp::move(events));

		return true;
	}, InputTouchInfo(makeButtonMask({InputMouseButton::MouseRight})));

	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if ((tap.input->data.getModifiers() & InputModifier::Shift) != InputModifier::None
				&& (tap.input->data.getModifiers() & InputModifier::Ctrl) != InputModifier::None) {
			_pointerCenter->setPosition(_content->convertToNodeSpace(tap.input->currentLocation));
		}
		return true;
	}, InputTapInfo(makeButtonMask({InputMouseButton::MouseRight}), 1));

	_listener->addMoveRecognizer([this](const GestureData &ev) {
		auto pos = _content->convertToNodeSpace(ev.input->currentLocation);
		auto diff = pos - _pointerCenter->getPosition().xy();

		_pointerReal->setPosition(pos);
		_pointerVirtual->setPosition(pos - diff * 2.0f);
		return true;
	});

#if NDEBUG
	_listener->setEnabled(false);
#endif
}

void Scene2d::addContentNodes(SceneContent *root) {
	if (_fps) {
		_fps->removeFromParent(true);
		_fps = nullptr;
	}

	if (_pointerReal) {
		_pointerReal->removeFromParent(true);
		_pointerReal = nullptr;
	}

	if (_pointerVirtual) {
		_pointerVirtual->removeFromParent(true);
		_pointerVirtual = nullptr;
	}

	if (_pointerCenter) {
		_pointerCenter->removeFromParent(true);
		_pointerCenter = nullptr;
	}

	if (root) {
		_fps = root->addChild(Rc<FpsDisplay>::create(), Node::ZOrderMax);
#if NDEBUG
		_fps->setVisible(false);
#endif

		do {
			auto image = Rc<VectorImage>::create(Size2(24, 24));
			image->addPath()->openForWriting(
					[](vg::PathWriter &writer) { writer.addCircle(12, 12, 12); });

			_pointerReal = root->addChild(Rc<VectorSprite>::create(move(image)), ZOrder::max());
			_pointerReal->setAnchorPoint(Anchor::Middle);
			_pointerReal->setContentSize(Size2(12, 12));
			_pointerReal->setColor(Color::Red_500);
			_pointerReal->setVisible(false);
		} while (0);

		do {
			auto image = Rc<VectorImage>::create(Size2(24, 24));
			image->addPath()->openForWriting(
					[](vg::PathWriter &writer) { writer.addCircle(12, 12, 12); });

			_pointerVirtual = root->addChild(Rc<VectorSprite>::create(move(image)), ZOrder::max());
			_pointerVirtual->setAnchorPoint(Anchor::Middle);
			_pointerVirtual->setContentSize(Size2(12, 12));
			_pointerVirtual->setColor(Color::Blue_500);
			_pointerVirtual->setVisible(false);
		} while (0);

		do {
			auto image = Rc<VectorImage>::create(Size2(24, 24));
			image->addPath()->openForWriting(
					[](vg::PathWriter &writer) { writer.addCircle(12, 12, 12); });

			_pointerCenter = root->addChild(Rc<VectorSprite>::create(move(image)), ZOrder::max());
			_pointerCenter->setAnchorPoint(Anchor::Middle);
			_pointerCenter->setContentSize(Size2(12, 12));
			_pointerCenter->setColor(Color::Green_500);
			_pointerCenter->setVisible(false);
		} while (0);
	}
}

void Scene2d::updateInputEventData(InputEventData &data, const InputEventData &source,
		Vec2 sourcePosition, uint32_t id) {
	auto pos = _inverse.transformPoint(sourcePosition);

	data = source;
	data.id = id;
	data.input.x = pos.x;
	data.input.y = pos.y;
	data.input.button = InputMouseButton::Touch;
	data.input.modifiers |= InputModifier::Unmanaged;
}

} // namespace stappler::xenolith::basic2d
