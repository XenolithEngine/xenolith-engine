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

#ifndef EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTCOMMONVIEW_H_
#define EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTCOMMONVIEW_H_

#include "XLRTCommonSource.h"
#include "XLRTRenderer.h"
#include "XL2dScrollView.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

class Renderer;

class SP_PUBLIC CommonObject : public basic2d::Sprite {
public:
	virtual ~CommonObject() = default;

	virtual bool init(RendererResult *, document::Object *obj);
	virtual void handleEnter(Scene *) override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual bool initAsLabel(document::Label *);
	virtual bool initAsBackground(document::Background *);
	virtual bool initAsPath(document::PathObject *);

	virtual void updateLabel(document::Label *);
	virtual void updateVertexes(FrameInfo &frame) override;

	virtual void updateColor() override;
	virtual void updateVertexesColor() override;

	virtual void pushCommands(FrameInfo &, NodeVisitFlags flags) override;

	Rc<RendererResult> _result;
	document::Object *_object = nullptr;

	Rc<basic2d::VectorSprite> _pathSprite;
	Rc<basic2d::VectorSprite> _overlay;

	bool _vertexesVisible = true;
};

class SP_PUBLIC CommonView : public basic2d::ScrollView {
public:
	using ResultCallback = Function<void(RendererResult *)>;

	virtual ~CommonView();

	virtual bool init(Layout = Vertical, CommonSource * = nullptr, Vector<String> &&ids = {});
	virtual void setLayout(Layout l) override;

	virtual void handleContentSizeDirty() override;

	virtual void setSource(CommonSource *);
	virtual CommonSource *getSource() const;

	virtual Renderer *getRenderer() const;
	virtual Document *getDocument() const;
	virtual RendererResult *getResult() const;

	virtual void refresh();

	virtual void setResultCallback(const ResultCallback &);
	virtual const ResultCallback &getResultCallback() const;

	virtual void setScrollRelativePosition(float value) override;

	virtual bool showNextPage();
	virtual bool showPrevPage();

	virtual float getObjectsOffset() const;

	virtual void setLinksEnabled(bool);
	virtual bool isLinksEnabled() const;

protected:
	virtual Vec2 convertToObjectSpace(Vec2) const;
	virtual bool isObjectActive(const Object &) const;
	virtual bool isObjectTapped(Vec2, const Object &) const;
	virtual void onObjectPressBegin(Vec2, const Object &);
	virtual void onObjectPressEnd(Vec2, const Object &);

	virtual void onSwipeBegin() override;

	virtual bool onPressBegin(Vec2) override;
	virtual bool onPressEnd(Vec2, TimeInterval) override;
	virtual bool onPressCancel(Vec2, TimeInterval) override;

	virtual void onRenderer(RendererResult *, bool);

	virtual void emplaceResultData(RendererResult *, float offset);

	virtual PageData getPageData(size_t) const;

	virtual Rc<ActionInterval> onSwipeFinalizeAction(float velocity) override;

	virtual void onPosition() override;

	virtual Rc<basic2d::ScrollController> onScrollController() const;

	bool _linksEnabled = true;
	Padding _pageMargin;
	Renderer *_renderer = nullptr;
	basic2d::Layer *_background = nullptr;

	float _density = 1.0f;
	Rc<CommonSource> _source;

	ResultCallback _callback;

	Vector<PageData> _pages;
	float _objectsOffset = 0.0f;
	float _gestureStart = nan();
};

} // namespace stappler::xenolith::richtext

#endif /* EXTRA_DOCUMENT_RICHTEXT_COMMON_XLRTCOMMONVIEW_H_ */
