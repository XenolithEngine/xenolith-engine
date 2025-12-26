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

#ifndef XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALTABBAR_H_
#define XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALTABBAR_H_

#include "MaterialMenuSource.h"
#include "MaterialButton.h"
#include "MaterialLabel.h"
#include "XLSubscriptionListener.h"
#include "XL2dScrollView.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC TabBar : public Surface {
public:
	enum class ButtonStyle {
		Title,
		Icon,
		TitleIcon,
	};

	enum class BarStyle {
		Layout,
		Scroll,
	};

	using Alignment = Label::TextAlign;

	virtual ~TabBar() { }

	virtual bool init(MenuSource *, ButtonStyle = ButtonStyle::Title, BarStyle = BarStyle::Layout, Alignment = Alignment::Center);
	virtual void handleContentSizeDirty() override;

	virtual void setMenuSource(MenuSource *);
	virtual MenuSource *getMenuSource() const;

	virtual void setAccentColor(const ColorRole &);
	virtual ColorRole getAccentColor() const;

	virtual void setButtonStyle(const ButtonStyle &);
	virtual const ButtonStyle & getButtonStyle() const;

	virtual void setBarStyle(const BarStyle &);
	virtual const BarStyle & getBarStyle() const;

	virtual void setAlignment(const Alignment &);
	virtual const Alignment & getAlignment() const;

	virtual void setSelectedIndex(size_t);
	virtual size_t getSelectedIndex() const;

	virtual void setProgress(float prog);

protected:
	virtual void setSelectedTabIndex(size_t);
	virtual void onMenuSource();
	virtual void onScrollPosition();
	virtual void onScrollPositionProgress(float);

	virtual float getItemSize(const String &, bool extended = false, bool selected = false) const;
	virtual Rc<Node> onItem(MenuSourceButton *, bool wrapped);
	virtual void onTabButton(Button *, MenuSourceButton *);

	virtual void applyStyle(StyleContainer *, const SurfaceStyleData &style) override;

	Alignment _alignment = Alignment::Center;
	ButtonStyle _buttonStyle = ButtonStyle::Title;
	BarStyle _barStyle = BarStyle::Layout;
	ColorRole _accentColor = ColorRole::PrimaryContainer;
	ScrollView *_scroll = nullptr;

	Layer *_layer = nullptr;
	IconSprite *_left = nullptr;
	IconSprite *_right = nullptr;
	DataListener<MenuSource> *_source = nullptr;
	Rc<MenuSource> _extra;
	size_t _buttonCount = 0;
	float _scrollWidth = 0.0f;

	size_t _selectedIndex = maxOf<size_t>();
	Vector<Pair<float, float>> _positions;
};

class SP_PUBLIC TabBarButton : public Button {
public:
	using TabButtonCallback = Function<void(Button *, MenuSourceButton *)>;

	virtual bool init(MenuSourceButton *, const TabButtonCallback &cb, TabBar::ButtonStyle, bool swallow, bool wrapped);
	virtual bool init(MenuSource *, const TabButtonCallback &cb, TabBar::ButtonStyle, bool swallow, bool wrapped);

protected:
	virtual void initialize(const TabButtonCallback &, TabBar::ButtonStyle, bool swallow, bool wrapped);

	virtual void onTabButton();

	virtual void layoutContent() override;

	TabBar::ButtonStyle _tabStyle;
	bool _wrapped = false;
	TabButtonCallback _tabButtonCallback = nullptr;
};

}

#endif /* XENOLITH_RENDERER_MATERIAL2D_COMPONENTS_MENU_MATERIALTABBAR_H_ */
