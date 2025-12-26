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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DSCENECONTENT_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DSCENECONTENT_H_

#include "XLSceneContent.h"
#include "XL2dFrameContext.h"
#include "XL2dSceneLayout.h"
#include "XL2dSceneLight.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class WindowHeader;

class SP_PUBLIC SceneContent2d : public SceneContent {
public:
	using DecorationMask = core::ViewConstraints;

	virtual ~SceneContent2d();

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleContentSizeDirty() override;

	// replaced node will be alone in stack, so, no need for exit transition
	virtual void replaceLayout(SceneLayout2d *);
	virtual void pushLayout(SceneLayout2d *);

	virtual void replaceTopLayout(SceneLayout2d *);
	virtual void popLayout(SceneLayout2d *);

	virtual bool pushOverlay(SceneLayout2d *);
	virtual bool popOverlay(SceneLayout2d *);

	virtual SceneLayout2d *getTopLayout();
	virtual SceneLayout2d *getPrevLayout();

	virtual bool popTopLayout();
	virtual bool isActive() const;

	virtual bool handleBackButton() override;
	virtual size_t getLayoutsCount() const;

	virtual const Vector<Rc<SceneLayout2d>> &getLayouts() const;
	virtual const Vector<Rc<SceneLayout2d>> &getOverlays() const;

	virtual void updateLayoutNode(SceneLayout2d *);

	// drops all lights and sets system defaults
	virtual void setDefaultLights();

	virtual bool addLight(SceneLight *, uint64_t tag = InvalidTag, StringView name = StringView());

	virtual SceneLight *getLightByTag(uint64_t) const;
	virtual SceneLight *getLightByName(StringView) const;

	virtual void removeLight(SceneLight *);
	virtual void removeLightByTag(uint64_t);
	virtual void removeLightByName(StringView);

	virtual void removeAllLights();
	virtual void removeAllLightsByType(SceneLightType);

	virtual void setGlobalLight(const Color4F &);
	virtual const Color4F &getGlobalLight() const;

	virtual bool visitGeometry(FrameInfo &, NodeVisitFlags parentFlags) override;

	virtual void draw(FrameInfo &, NodeVisitFlags flags) override;

protected:
	virtual void pushNodeInternal(SceneLayout2d *node, Function<void()> &&cb);

	virtual void eraseLayout(SceneLayout2d *);
	virtual void eraseOverlay(SceneLayout2d *);
	virtual void replaceNodes();
	virtual void updateNodesVisibility();

	Vector<Rc<SceneLight>>::iterator removeLight(Vector<Rc<SceneLight>>::iterator);

	Vector<Rc<SceneLayout2d>> _layouts;
	Vector<Rc<SceneLayout2d>> _overlays;

	Rc<FrameContext2d> _2dContext;

	float _shadowDensity = 0.5f;
	uint32_t _lightsAmbientCount = 0;
	uint32_t _lightsDirectCount = 0;
	Vector<Rc<SceneLight>> _lights;
	Map<uint64_t, SceneLight *> _lightsByTag;
	Map<StringView, SceneLight *> _lightsByName;

	Color4F _globalLight = Color4F(1.0f, 1.0f, 1.0f, 1.0f);

	Vector<Function<void()>> _visitNotification;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DSCENECONTENT_H_ */
