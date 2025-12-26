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

#ifndef XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLITEMHANDLE_H_
#define XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLITEMHANDLE_H_

#include "XL2dScrollController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class ScrollController;

class SP_PUBLIC ScrollItemHandle : public System {
public:
	using Item = ScrollController::Item;
	using Callback = Function<void(const Item &)>;

	virtual ~ScrollItemHandle();

	virtual void onNodeInserted(ScrollController *, Item &, size_t index);
	virtual void onNodeUpdated(ScrollController *, Item &, size_t index);
	virtual void onNodeRemoved(ScrollController *, Item &, size_t index);

	void setInsertCallback(Callback &&);
	void setUpdateCallback(Callback &&);
	void setRemoveCallback(Callback &&);

	void resize(float newSize, bool forward = true);
	void forceResize(float newSize, bool forward = true);

	void setLocked(bool);
	bool isLocked() const;

	bool isConnected() const;

protected:
	ScrollController *_controller = nullptr;
	size_t _itemIndex = 0;

	Callback _insertCallback;
	Callback _updateCallback;
	Callback _removeCallback;
	bool _isLocked = false;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLITEMHANDLE_H_ */
