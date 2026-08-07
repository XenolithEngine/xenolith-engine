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

#include "MultiWindowLayout.h"

#include "SecondaryWindow.h"

#include "XLAction.h"
#include "XLAppWindow.h"
#include "XLDirector.h"

#include <stdlib.h> // getenv
#include "XLAppThread.h"
#include "XLCoreInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using basic2d::Label;

namespace {

// Content of the secondary window: one label with the shared text, nothing else. Kept as a bare
// SceneLayout2d (not a TestLayout) so the second window carries no caption and no stylesheet - what
// it renders is exactly this label.
class SecondWindowContent : public basic2d::SceneLayout2d {
public:
	virtual bool init() override {
		if (!SceneLayout2d::init()) {
			return false;
		}

		_label = addChild(Rc<Label>::create(), ZOrder(1));
		_label->setFontSize(24);
		_label->setColor(Color::Black, false);
		_label->setString(MultiWindowLayout::kSharedText);
		_label->setAnchorPoint(Anchor::Middle);

		// Headless renders on demand and this window has no FPS counter to keep it dirty, so without
		// this it stops producing frames after the first one - and a label that never visits never
		// lays its glyphs out, which is exactly what the test is watching for.
		runAction(Rc<RenderContinuously>::create());
		return true;
	}

	virtual void handleContentSizeDirty() override {
		SceneLayout2d::handleContentSizeDirty();
		_label->setPosition(_contentSize / 2.0f);
	}

	Label *getLabel() const { return _label; }

protected:
	Label *_label = nullptr;
};

static SecondWindowContent *s_secondContent = nullptr;

// How long after the secondary asks for the glyphs the capture is taken. Overridable so a run with
// XL_FONT_GLYPH_DELAY_US can be pointed either inside the rasterisation window (is the window gated
// while its glyphs are being made?) or after it (did they arrive at all?).
static float s_captureDelay() {
	auto v = ::getenv("XL_MULTIWINDOW_CAPTURE_DELAY");
	return v ? float(::atof(v)) : 0.3f;
}

} // namespace

WideStringView MultiWindowLayout::getRaceText() {
	// Latin, Cyrillic and Greek letter ranges, every code point distinct: ~250 glyphs the atlas has
	// never seen, so one flush has real work to do.
	static WideString s_text = [] {
		WideString out;
		for (char16_t c = u'\u0021'; c <= u'\u007E'; ++c) { out.push_back(c); }
		for (char16_t c = u'\u0410'; c <= u'\u044F'; ++c) { out.push_back(c); }
		for (char16_t c = u'\u0391'; c <= u'\u03C9'; ++c) { out.push_back(c); }
		for (char16_t c = u'\u00C0'; c <= u'\u00FF'; ++c) { out.push_back(c); }
		return out;
	}();
	return s_text;
}

bool MultiWindowLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_primaryLabel = addChild(Rc<Label>::create(), ZOrder(1));
	_primaryLabel->setFontSize(24);
	_primaryLabel->setColor(Color::White, false);
	_primaryLabel->setString(kSharedText);
	_primaryLabel->setAnchorPoint(Anchor::Middle);

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(1.2f), [this] { openSecondWindow(); },
			Rc<DelayTime>::create(2.0f), [this] { raceStepPrimary(); },
			// One frame, not one second: the secondary must lay the new glyphs out on the tick right
			// AFTER the flush that sent them, while their upload is still outstanding. That is the
			// window in which it used to be told "nothing new" and waited for nothing.
			Rc<DelayTime>::create(1.0f / 60.0f), [this] { raceStepSecondary(); },
			// Capture promptly: the point is what the second window puts on screen WHILE the atlas
			// upload for those glyphs is still outstanding. Wait longer and RenderContinuously has
			// already redrawn it correctly, which hides the very thing under test. Pair this with
			// XL_FONT_UPLOAD_DELAY_MS to make that interval wide enough to land in.
			Rc<DelayTime>::create(s_captureDelay()), [this] { captureSecondWindow(); },
			Rc<DelayTime>::create(1.5f), [this] { runChecks(); }));

	return true;
}

void MultiWindowLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();
	_primaryLabel->setPosition(Vec2(_contentSize.width / 2.0f, getWorkTop() / 2.0f));
}

void MultiWindowLayout::handleExit() {
	SecondaryWindow::close(kSecondWindowId);
	s_secondContent = nullptr;
	TestLayout::handleExit();
}

void MultiWindowLayout::openSecondWindow() {
	auto server = _director ? _director->getRenderServer() : nullptr;
	if (!server) {
		log::source().error("MultiWindowTest", "no render server on the primary window");
		return;
	}

	log::source().warn("MultiWindowTest", "opening the second Root window '", kSecondWindowId, "'");

	SecondaryWindow::open(static_cast<AppWindow *>(server), kSecondWindowId, Extent2(640, 200),
			[](StringView) -> Rc<basic2d::SceneLayout2d> {
		auto content = Rc<SecondWindowContent>::create();
		s_secondContent = content;
		return content;
	});
}

void MultiWindowLayout::raceStepPrimary() {
	log::source().warn("MultiWindowTest", "race: primary takes the fresh glyphs (",
			getRaceText().size(), " code points at size ", kRaceFontSize, ")");
	_primaryLabel->setFontSize(kRaceFontSize);
	_primaryLabel->setString(getRaceText());
}

void MultiWindowLayout::raceStepSecondary() {
	log::source().warn("MultiWindowTest", "race: secondary asks for the same glyphs");
	if (s_secondContent) {
		s_secondContent->getLabel()->setFontSize(kRaceFontSize);
		s_secondContent->getLabel()->setString(getRaceText());
		_raceApplied = true;
	}
}

void MultiWindowLayout::captureSecondWindow() {
	auto scene = SecondaryWindow::getScene(kSecondWindowId);
	auto director = scene ? scene->getDirector() : nullptr;
	auto server = director ? director->getRenderServer() : nullptr;
	if (!server) {
		log::source().error("MultiWindowTest", "no render server on the second window");
		return;
	}

	auto app = _director->getApplication();
	static_cast<AppWindow *>(server)->captureScreenshot(
			[this, app = Rc<AppThread>(app)](const core::ImageInfoData &info,
					BytesView view) mutable {
		// Runs on the device-task thread; `view` points into a staging buffer that dies with the
		// task, so the pixels have to be counted right here.
		uint64_t lit = 0;
		if (!view.empty()) {
			auto bmp = core::getBitmap(info, view);
			// Optional: keep the captured frame itself, not just the pixel count. The count alone
			// cannot tell "the glyphs are missing" from "the label is smaller than expected".
			if (auto out = ::getenv("XL_MULTIWINDOW_CAPTURE_FILE")) {
				bmp.save(FileInfo(StringView(out)));
			}
			auto data = bmp.dataPtr();
			const auto stride = bmp.stride();
			const auto bpp = bitmap::getBytesPerPixel(bmp.format());
			// Count pixels that differ from the background rather than "bright" ones: the clear
			// colour is whatever the scene's default is, and the test must not depend on it. The
			// top-left corner is outside the centred label in every case.
			const uint8_t *bg = data;
			for (uint32_t y = 0; y < bmp.height(); ++y) {
				auto row = data + size_t(y) * stride;
				for (uint32_t x = 0; x < bmp.width(); ++x) {
					auto px = row + size_t(x) * bpp;
					uint32_t diff = 0;
					const uint32_t channels = bpp < 3 ? bpp : 3;
					for (uint32_t c = 0; c < channels; ++c) {
						diff += uint32_t(sprt::abs(int32_t(px[c]) - int32_t(bg[c])));
					}
					if (diff > 48) {
						++lit;
					}
				}
			}
		}
		app->performOnAppThread([this, lit] {
			_litPixels = lit;
			_captureDone = true;
			log::source().warn("MultiWindowTest", "second window: ", lit, " lit pixels");
		}, nullptr);
	});
}

void MultiWindowLayout::expect(bool cond, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("MultiWindowTest", what);
	}
}

void MultiWindowLayout::runChecks() {
	expect(SecondaryWindow::isOpen(kSecondWindowId), "the second Root window did not open");
	expect(s_secondContent != nullptr, "the second window's content was never built");

	if (s_secondContent) {
		auto second = s_secondContent->getLabel();
		expect(second != nullptr && second->getContentSize().width > 1.0f,
				"the second window's label did not lay out");
		expect(_raceApplied, "the race string never reached the second window");
		if (second && _primaryLabel) {
			// Both windows now carry the same string, at the same size, through the same
			// process-wide font controller: they must measure identically. A mismatch means the
			// second window laid out against a different font state than the first.
			expect(sprt::abs(second->getContentSize().width
							 - _primaryLabel->getContentSize().width) < 1.0f,
					"the two windows measured the shared string differently");
		}
	}

	expect(_captureDone, "the second window could not be captured");
	// A label that laid out but whose glyphs are missing from the atlas renders as nothing: the
	// quads are there, the shader just has no box for them. Hundreds of code points at size 72 fill
	// the window, so the threshold is far above any antialiasing noise.
	expect(_litPixels > 1000,
			"the second window rendered (almost) no text - its glyphs were not in the atlas");

	log::source().warn("MultiWindowTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
}

} // namespace stappler::xenolith::app
