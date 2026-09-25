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

#ifndef TESTS_WINDOW_SRC_WINDOW_FRAMEREQUESTLAYOUT_H_
#define TESTS_WINDOW_SRC_WINDOW_FRAMEREQUESTLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A window asks for its frames itself (Node::markSceneChanged -> Director::handleSceneChanged), and
// every way a scene changes has to end in a finite number of requests. Each command makes one kind
// of change from outside a frame; frame-request-check.py reads the director's request count
// (`frame` with count 0) and requires it to stop growing.
//
//   move     a node moves once
//   same     a node is set to where it already is: no change at all
//   text     a label gets new glyphs, so its frames wait on the atlas and wake the app thread
//   resize   a row changes width, and lays its cells out again inside the visit
//   echo     a node placed in the visit answers with a task that places it again, after the frame
//   animate  a finite action
//
// Unlike every other test here it drops TestLayout's RenderContinuously, and its commands answer
// at once rather than after frames of their own: nothing draws unless the director asks.
class FrameRequestLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	basic2d::Layer *_box = nullptr;
	basic2d::Label *_label = nullptr;
	Node *_row = nullptr;
	Node *_echo = nullptr;
	bool _boxMoved = false;
	bool _rowWide = false;
	bool _echoWide = false;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WINDOW_FRAMEREQUESTLAYOUT_H_
