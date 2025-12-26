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

#ifndef XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALMENUSOURCE_H_
#define XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALMENUSOURCE_H_

#include "XL2dIcons.h"
#include "SPSubscription.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Node;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class Button;
class MenuSourceButton;

class SP_PUBLIC MenuSourceItem : public Subscription {
public:
	enum class Type {
		Separator,
		Button,
		Custom,
	};

	using AttachCallback = Function<void(MenuSourceItem *, Node *)>;

	virtual bool init();
	virtual Rc<MenuSourceItem> copy() const;

	virtual void setCustomData(const Value &);
	virtual void setCustomData(Value &&);
	virtual const Value &getCustomData() const;

	virtual MenuSourceItem *setAttachCallback(const AttachCallback &);
	virtual MenuSourceItem *setDetachCallback(const AttachCallback &);

	virtual Type getType() const;

	virtual void handleNodeAttached(Node *);
	virtual void handleNodeDetached(Node *);

	void setDirty();

protected:
	Type _type = Type::Separator;
	Value _customData;

	AttachCallback _attachCallback;
	AttachCallback _detachCallback;
};

class SP_PUBLIC MenuSourceCustom : public MenuSourceItem {
public:
	using FactoryFunction = Function<Rc<Node>(Node *, MenuSourceCustom *)>;
	using HeightFunction = Function<float(Node *, float)>;

	virtual bool init() override;
	virtual bool init(float h, const FactoryFunction &func, float minWidth = 0.0f);
	virtual bool init(const HeightFunction &h, const FactoryFunction &func, float minWidth = 0.0f);

	virtual Rc<MenuSourceItem> copy() const override;

	virtual float getMinWidth() const;
	virtual float getHeight(Node *, float) const;
	virtual const HeightFunction &getHeightFunction() const;
	virtual const FactoryFunction &getFactoryFunction() const;

protected:
	float _minWidth = 0.0f;
	HeightFunction _heightFunction = nullptr;
	FactoryFunction _function = nullptr;
};

class SP_PUBLIC MenuSource : public Subscription {
public:
	using Callback = Function<void(Button *b, MenuSourceButton *)>;
	using FactoryFunction = MenuSourceCustom::FactoryFunction;
	using HeightFunction = MenuSourceCustom::HeightFunction;

	virtual bool init() { return true; }
	virtual ~MenuSource();

	void setHintCount(size_t);
	size_t getHintCount() const;

	Rc<MenuSource> copy() const;

	void addItem(MenuSourceItem *);
	MenuSourceButton *addButton(StringView, Callback && = nullptr);
	MenuSourceButton *addButton(StringView, IconName, Callback && = nullptr);
	MenuSourceButton *addButton(StringView, IconName, Rc<MenuSource> &&);
	MenuSourceCustom *addCustom(float h, const FactoryFunction &func, float minWidth = 0.0f);
	MenuSourceCustom *addCustom(const HeightFunction &h, const FactoryFunction &func,
			float minWidth = 0.0f);
	MenuSourceItem *addSeparator();

	void clear();

	uint32_t count();

	const Vector<Rc<MenuSourceItem>> &getItems() const;

protected:
	Vector<Rc<MenuSourceItem>> _items;
	size_t _hintCount = 0;
};

class SP_PUBLIC MenuSourceButton : public MenuSourceItem {
public:
	using Callback = Function<void(Button *b, MenuSourceButton *)>;

	virtual ~MenuSourceButton();

	virtual bool init(StringView, IconName, Callback &&);
	virtual bool init(StringView, IconName, Rc<MenuSource> &&);
	virtual bool init() override;
	virtual Rc<MenuSourceItem> copy() const override;

	virtual void setName(StringView);
	virtual const String &getName() const;

	virtual void setValue(StringView);
	virtual const String &getValue() const;

	virtual void setNameIcon(IconName icon);
	virtual IconName getNameIcon() const;

	virtual void setValueIcon(IconName icon);
	virtual IconName getValueIcon() const;

	virtual void setCallback(Callback &&);
	virtual const Callback &getCallback() const;

	virtual void setNextMenu(MenuSource *);
	virtual MenuSource *getNextMenu() const;

	virtual void setSelected(bool value);
	virtual bool isSelected() const;

protected:
	String _name;
	String _value;

	IconName _nameIcon = IconName::None;
	IconName _valueIcon = IconName::None;

	Rc<MenuSource> _nextMenu;
	Callback _callback = nullptr;
	bool _selected = false;
};

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALMENUSOURCE_H_ */
