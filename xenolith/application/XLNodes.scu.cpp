/**
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

#include "XLCommon.h" // IWYU pragma: keep

#include "nodes/XLNode.cc"
#include "nodes/XLScene.cc"
#include "nodes/XLSceneContent.cc"
#include "nodes/XLCloseGuardWidget.cc"
#include "nodes/XLWindowDecorations.cc"
#include "nodes/XLSystem.cc"
#include "nodes/XLComponent.cc"
#include "nodes/XLDynamicStateSystem.cc"
#include "nodes/XLSubscriptionListener.cc"
#include "nodes/XLEventListener.cc"

#include "input/XLGestureRecognizer.cc"
#include "input/XLInputDispatcher.cc"
#include "input/XLInputListener.cc"
#include "input/XLFocusGroup.cc"
#include "input/XLTextInputManager.cc"

#include "actions/XLAction.cc"
#include "actions/XLActionEase.cc"
#include "actions/XLActionManager.cc"
#include "actions/XLInterpolation.cc"
