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

#ifndef TESTS_WINDOW_SRC_APP_TESTLAYOUT_H_
#define TESTS_WINDOW_SRC_APP_TESTLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

struct TestInfo;

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

	// Seconds of real rendering a command lets pass before it answers, so whoever asked can
	// screenshot the settled result instead of guessing how long the relayout takes. Overridable
	// per call through the command's "settle" argument.
	static constexpr float DefaultSettle = 0.5f;

	// One action of this layout, reachable over the inspector socket. Runs on the app thread with
	// the scene graph fully available; whatever it returns is the command's result.
	using CommandHandler = Function<Value(Value &&args)>;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	// Called by the registry right after construction. A layout that never gets one (an overlay
	// pushed by a test) shows no caption, keeps its full content area and registers no commands.
	virtual void setTestInfo(const TestInfo &);

	// 0 when there is no caption
	float getCaptionHeight() const;

	// Top edge of the area left over for the test itself
	float getWorkTop() const;

	// Content size minus the caption strip
	Size2 getWorkSize() const;

protected:
	// Override to expose this layout's own actions over the inspector socket - that is how a
	// headless run drives a test that a person would drive with the control bar. Called on enter;
	// everything registered here is dropped again on exit, so a command can never outlive the
	// layout that implements it.
	virtual void registerCommands() { }

	// Register one, as "<test>.<name>" so two layouts can not collide. `description` is what the
	// `commands` protocol command reports.
	void addCommand(StringView name, StringView description, CommandHandler &&);

	// Attach a ui::StyleSystem carrying this stylesheet. Note that a stylesheet alone changes
	// nothing: a ui::StyleResolver somewhere below is what applies it.
	void setStyleSheet(StringView css);

	// Same, from a file on disk - which also puts the sheet under a file watch, so rewriting the
	// file reloads it live.
	void setStyleSheet(const FileInfo &);

	basic2d::Layer *_captionBackground = nullptr;
	basic2d::Label *_captionTitle = nullptr;
	basic2d::Label *_captionDescription = nullptr;
	const TestInfo *_info = nullptr;
	Vector<String> _registeredCommands;
	bool _hasCaption = false;
	bool _hideFps = false;
	bool _fpsWasVisible = false;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_APP_TESTLAYOUT_H_
