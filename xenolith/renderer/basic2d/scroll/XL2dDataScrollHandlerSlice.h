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

#ifndef XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DDATASCROLLHANDLERHANDLERSLICE_H_
#define XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DDATASCROLLHANDLERHANDLERSLICE_H_

#include "XL2dDataScrollView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC DataScrollHandlerSlice : public DataScrollView::Handler {
public:
	using DataCallback =
			Function<Rc<DataScrollView::Item>(DataScrollHandlerSlice *h, Value &&, Vec2)>;

	virtual ~DataScrollHandlerSlice() = default;

	virtual bool init(DataScrollView *, DataCallback && = nullptr);
	virtual void setDataCallback(DataCallback &&);

	virtual DataScrollView::ItemMap run(Request, DataMap &&) override;

protected:
	virtual Vec2 getOrigin(Request) const;
	virtual Rc<DataScrollView::Item> onItem(Value &&, const Vec2 &);

	Vec2 _originFront;
	Vec2 _originBack;
	DataCallback _dataCallback;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DDATASCROLLHANDLERHANDLERSLICE_H_ */
