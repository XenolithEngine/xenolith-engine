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

#ifndef XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLCONTROLLER_H_
#define XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLCONTROLLER_H_

#include "XL2d.h" // IWYU pragma: keep
#include "XLSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class ScrollItemHandle;
class ScrollViewBase;

class SP_PUBLIC ScrollController : public System {
public:
	struct Item;

	/// Callback for node creation
	using NodeFunction = Function<Rc<Node>(const Item &)>;

	// Callback to rebuild scroll list, usually called when scroll ContentSize was changed
	// Should return true if items was rebuilt, false if no changes performed
	using RebuildCallback = Function<bool(ScrollController *)>; // return true if item was rebuilded

	struct Item {
		Item(NodeFunction &&, Vec2 pos, Size2 size, ZOrder zIndex, StringView);

		NodeFunction nodeFunction = nullptr;
		Size2 size;
		Vec2 pos;
		ZOrder zIndex;
		String name;

		Node *node = nullptr;
		ScrollItemHandle *handle = nullptr;
	};

	virtual ~ScrollController();

	virtual void handleAdded(Node *owner) override;
	virtual void handleRemoved() override;
	virtual void handleContentSizeDirty() override;

	/// Scroll view callbacks handlers
	virtual void onScrollPosition(bool force = false);
	virtual void onScroll(float delta, bool eneded);
	virtual void onOverscroll(float delta);

	virtual float getScrollMin();
	virtual float getScrollMax();

	Node *getRoot() const;
	ScrollViewBase *getScroll() const;

	virtual void clear(); /// remove all items and non-strict barriers
	virtual void update(float position, float size); /// update current scroll position and size
	virtual void reset(float position, float size); /// set new scroll position and size

	/// Scrollable area size and offset are strict limits of scrollable area
	/// It's usefull when scroll parameters (offset, size, items positions) are known
	/// If scroll parameters are dynamic or determined in runtime - use barriers

	virtual bool setScrollableArea(float offset, float size);
	virtual float getScrollableAreaOffset() const; // NaN by default
	virtual float getScrollableAreaSize() const; // NaN by default

	/// you should return true if this call successfully rebuilds visible objects
	virtual bool rebuildObjects();

	virtual size_t size() const;
	virtual size_t addItem(NodeFunction &&, Size2 size, Vec2 pos, ZOrder zIndex = ZOrder(0),
			StringView tag = StringView());
	virtual size_t addItem(NodeFunction &&, float size, float pos, ZOrder zIndex = ZOrder(0),
			StringView tag = StringView());
	virtual size_t addItem(NodeFunction &&, float size = 0.0f, ZOrder zIndex = ZOrder(0),
			StringView tag = StringView());

	virtual size_t addPlaceholder(Size2 size, Vec2 pos);
	virtual size_t addPlaceholder(float size, float pos);
	virtual size_t addPlaceholder(float size);

	const Item *getItem(size_t);
	const Item *getItem(Node *);
	const Item *getItem(StringView);

	size_t getItemIndex(Node *);

	bool removeItem(size_t);
	bool removeItem(const Item *);
	bool removeItem(Node *);
	bool removeItem(StringView);

	const Vector<Item> &getItems() const;
	Vector<Item> &getItems();

	// Apply penging changes to ScrollView
	// Note that all changes you made in controller (add/remove/reposition/resize items)
	// will not be reflected in ScrollView immediately, it needs to be commited directly
	// or contextually (when ScrollView resized or scrolled)
	// commitChanges has no effect when there are no changes awaits
	void commitChanges();

	virtual void setScrollRelativeValue(float value);

	Node *getNodeByName(StringView) const;
	Node *getFrontNode() const;
	Node *getBackNode() const;
	Vector<Rc<Node>> getNodes() const;

	float getNextItemPosition() const;

	void setKeepNodes(bool);
	bool isKeepNodes() const;

	void resizeItem(const Item *node, float newSize, bool forward = true);

	void setAnimationPadding(float padding);
	void dropAnimationPadding();
	void updateAnimationPadding(float value);

	void setRebuildCallback(const RebuildCallback &);
	const RebuildCallback &getRebuildCallback() const;

protected:
	virtual void onNextObject(Item &, float pos,
			float size); /// insert new object at specified position

	virtual void addScrollNode(Item &);
	virtual void updateScrollNode(Item &);
	virtual void removeScrollNode(Item &);

	ScrollViewBase *_scroll = nullptr;
	Node *_root = nullptr;

	float _scrollAreaOffset = 0.0f;
	float _scrollAreaSize = 0.0f;

	float _currentMin = 0.0f;
	float _currentMax = 0.0f;

	float _windowBegin = 0.0f;
	float _windowEnd = 0.0f;

	float _currentPosition = 0.0f;
	float _currentSize = 0.0f;

	Vector<Item> _nodes;

	bool _infoDirty = true;
	bool _keepNodes = false;

	float _animationPadding = 0.0f;
	float _savedSize = 0.0f;

	RebuildCallback _callback;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_SCROLL_XL2DSCROLLCONTROLLER_H_ */
