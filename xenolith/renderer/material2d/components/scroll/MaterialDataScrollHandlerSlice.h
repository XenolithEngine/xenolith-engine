/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SCROLL_MATERIALDATASCROLLHANDLERSLICE_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SCROLL_MATERIALDATASCROLLHANDLERSLICE_H_

#include "MaterialDataScroll.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC DataScrollHandlerSlice : public DataScroll::Handler {
public:
	using DataCallback = Function<Rc<DataScroll::Item> (DataScrollHandlerSlice *h, Value &&, Vec2)>;

	virtual ~DataScrollHandlerSlice() { }
	virtual bool init(DataScroll *, DataCallback && = nullptr);
	virtual void setDataCallback(DataCallback &&);

	virtual DataScroll::ItemMap run(Request, DataMap &&) override;

protected:
	virtual Vec2 getOrigin(Request) const;
	virtual Rc<DataScroll::Item> onItem(Value &&, const Vec2 &);

	Vec2 _originFront;
	Vec2 _originBack;
	DataCallback _dataCallback;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SCROLL_MATERIALDATASCROLLHANDLERSLICE_H_ */
