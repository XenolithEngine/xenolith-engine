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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SIDEBAR_MATERIALSIDEBAR_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SIDEBAR_MATERIALSIDEBAR_H_

#include "MaterialSurface.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC Sidebar : public Node {
public:
	enum Position {
		Left,
		Right
	};

	using WidthCallback = Function<float(const Size2 &)>;
	using BoolCallback = Function<void(bool)>;

	virtual ~Sidebar();

	virtual bool init(Position);
	virtual void handleContentSizeDirty() override;

	template <typename T, typename ... Args>
	auto setNode(const Rc<T> &ptr, Args && ... args) -> T * {
		setBaseNode(ptr.get(), std::forward<Args>(args)...);
		return ptr.get();
	}

	virtual void setBaseNode(Node *m, ZOrder zOrder = ZOrder(0));
	virtual Node *getNode() const;

	virtual void setNodeWidth(float);
	virtual float getNodeWidth() const;

	virtual void setNodeWidthCallback(const WidthCallback &);
	virtual const WidthCallback &getNodeWidthCallback() const;

	virtual void setSwallowTouches(bool);
	virtual bool isSwallowTouches() const;

	virtual void setEdgeSwipeEnabled(bool);
	virtual bool isEdgeSwipeEnabled() const;

	virtual void setBackgroundActiveOpacity(float);
	virtual void setBackgroundPassiveOpacity(float);

	virtual void show();
	virtual void hide(float factor = 1.0f);

	virtual float getProgress() const;

	virtual bool isNodeVisible() const;
	virtual bool isNodeEnabled() const;

	virtual void setNodeVisibleCallback(BoolCallback &&);
	virtual void setNodeEnabledCallback(BoolCallback &&);

	virtual void setEnabled(bool value);
	virtual bool isEnabled() const;

protected:
	virtual void setProgress(float);

	virtual void onSwipeDelta(float);
	virtual void onSwipeFinished(float);

	virtual void onNodeEnabled(bool value);
	virtual void onNodeVisible(bool value);

	virtual void stopNodeActions();

	WidthCallback _widthCallback;
	float _nodeWidth = 0.0f;

	Position _position = Position::Left;
	bool _swallowTouches = true;
	bool _edgeSwipeEnabled = true;
	float _backgroundActiveOpacity = 0.25f;
	float _backgroundPassiveOpacity = 0.0f;

	InputListener *_listener = nullptr;

	Layer *_background = nullptr;
	Node *_node = nullptr;

	BoolCallback _visibleCallback = nullptr;
	BoolCallback _enabledCallback = nullptr;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_SIDEBAR_MATERIALSIDEBAR_H_ */
