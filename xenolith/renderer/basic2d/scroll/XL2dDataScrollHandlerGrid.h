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

#ifndef XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DDATASCROLLHANDLERHANDLERGRID_H_
#define XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DDATASCROLLHANDLERHANDLERGRID_H_

#include "XL2dDataScrollView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC DataScrollHandlerGrid : public DataScrollView::Handler {
public:
	virtual ~DataScrollHandlerGrid() = default;

	virtual bool init(DataScrollView *) override;
	virtual bool init(DataScrollView *, const Padding &p);
	virtual DataScrollView::ItemMap run(Request, DataMap &&) override;

	virtual void setCellMinWidth(float v);
	virtual void setCellAspectRatio(float v);
	virtual void setCellHeight(float v);

	virtual void setAutoPaddings(bool);
	virtual bool isAutoPaddings() const;

protected:
	virtual Rc<DataScrollView::Item> onItem(Value &&, DataScrollView::DataSource::Id);

	bool _autoPaddings = false;
	bool _fixedHeight = false;
	float _currentWidth = 0.0f;

	float _cellAspectRatio = 1.0f;
	float _cellMinWidth = 1.0f;

	float _cellHeight = 0.0f;

	float _widthPadding = 0.0f;
	Padding _padding;
	Size2 _currentCellSize;
	uint32_t _currentCols = 0;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DDATASCROLLHANDLERHANDLERGRID_H_ */
