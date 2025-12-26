/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALDECORATEDLAYOUT_H_
#define XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALDECORATEDLAYOUT_H_

#include "MaterialLayerSurface.h"
#include "XL2dSceneLayout.h"
#include "XL2dScrollView.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

enum class ViewDecorationFlags : uint32_t {
	None,
	Visible = 1 << 0,
	Tracked = 1 << 1
};

SP_DEFINE_ENUM_AS_MASK(ViewDecorationFlags)

class SP_PUBLIC DecoratedLayout : public SceneLayout2d {
public:
	virtual ~DecoratedLayout() = default;

	virtual bool init(ColorRole decorationColorRole = ColorRole::PrimaryContainer);
	virtual void handleContentSizeDirty() override;
	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	virtual void setDecorationColorRole(ColorRole);
	virtual ColorRole getDecorationColorRole() const;

	virtual void setDecorationElevation(Elevation);
	virtual Elevation getDecorationElevation() const;

	virtual void setViewDecorationFlags(ViewDecorationFlags value);
	virtual ViewDecorationFlags getViewDecorationFlags() const;

	virtual void handleForeground(SceneContent2d *l, SceneLayout2d *overlay) override;

	LayerSurface *getBackground() const { return _background; }

	virtual float getMaxDepthIndex() const override;

protected:
	virtual void updateStatusBar(const SurfaceStyleData &style);

	Node *_decorationRoot = nullptr;
	LayerSurface *_decorationLeft = nullptr;
	LayerSurface *_decorationRight = nullptr;
	LayerSurface *_decorationTop = nullptr;
	LayerSurface *_decorationBottom = nullptr;
	LayerSurface *_background = nullptr;
	ViewDecorationFlags _viewDecoration = ViewDecorationFlags::None;
};

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALDECORATEDLAYOUT_H_ */
