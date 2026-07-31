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

#ifndef TESTS_WINDOW_SRC_TESTLAYOUT_H_
#define TESTS_WINDOW_SRC_TESTLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Common base for every layout in this test app.
//
// It carries the two things each test used to repeat by hand:
//
//   * a stylesheet attached as a ui::StyleSystem - setStyleSheet(), so a test does not have to
//     include the ui headers or know how the system is wired;
//
//   * an on-screen caption naming the test and saying in one line what to look at. The text comes
//     from the entry in TestRegistry, so what the screen says and what the registry says can not
//     drift apart.
//
// The caption occupies a strip along the top edge. Test content that anchors to the top must
// therefore start at getWorkTop() instead of at the full content height, or it renders underneath
// the caption. Tests that centre their content, or anchor it to the bottom, need nothing.
class TestLayout : public basic2d::SceneLayout2d {
public:
	static constexpr float CaptionHeight = 76.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	// Called by the registry right after construction. A layout that never gets one (the app's own
	// general demo, an overlay) simply shows no caption and keeps its full content area.
	virtual void setTestInfo(StringView title, StringView description, StringView env);

	// Hide the scene's FPS counter while this test is on screen. It is marked AlwaysDirty, so it
	// damages a region every frame - which a test about damage tracking can not tolerate. Restored
	// when the test is left, so it also works when the test is opened from the menu.
	void setHideFps(bool);

	// 0 when there is no caption
	float getCaptionHeight() const;

	// Top edge of the area left over for the test itself
	float getWorkTop() const;

	// Content size minus the caption strip
	Size2 getWorkSize() const;

protected:
	// Attach a ui::StyleSystem carrying this stylesheet. Note that a stylesheet alone changes
	// nothing: a ui::StyleResolver somewhere below is what applies it.
	void setStyleSheet(StringView css);

	// Same, from a file on disk - which also puts the sheet under a file watch, so rewriting the
	// file reloads it live.
	void setStyleSheet(const FileInfo &);

	basic2d::Layer *_captionBackground = nullptr;
	basic2d::Label *_captionTitle = nullptr;
	basic2d::Label *_captionDescription = nullptr;
	bool _hasCaption = false;
	bool _hideFps = false;
	bool _fpsWasVisible = false;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_TESTLAYOUT_H_
