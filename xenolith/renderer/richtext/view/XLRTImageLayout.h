/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef EXTRA_DOCUMENT_RICHTEXT_VIEW_XLRTIMAGEVIEW_H_
#define EXTRA_DOCUMENT_RICHTEXT_VIEW_XLRTIMAGEVIEW_H_

#include "XLRTCommonSource.h"
#include "XL2dImageLayer.h"
#include "XLRTTooltip.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

/*
class SP_PUBLIC ImageLayout : public material2d::DecoratedLayout {
public:
	virtual ~ImageLayout();

	virtual bool init(RendererResult *, const StringView &id, const StringView &src, const StringView &alt = StringView());

	virtual void handleContentSizeDirty() override;
	virtual void handleEnter(Scene *) override;

	virtual void close();

protected:
	String _src;
	material2d::AppBar *_toolbar = nullptr;
	bool _expanded = true;

	material2d::ImageLayer *_sprite = nullptr;
	Rc<RendererResult> _result;
};
*/
}

#endif /* EXTRA_DOCUMENT_RICHTEXT_VIEW_XLRTIMAGEVIEW_H_ */
