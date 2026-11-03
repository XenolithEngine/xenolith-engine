/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DVIEW_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DVIEW_H_

#include "XL3dCamera.h"
#include "XL3dFrameContext.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

/* A rectangle of the scene that shows a 3d world through a Camera. It is an ordinary node of a
SceneContent3d: its rect is the view's viewport, and the 2d nodes drawn after it are over it.

It hands the 3d pass its draws once a frame, from draw(). It has no 2d children: they would be
drawn as 2d content of the same frame, under nothing and over everything, which is never meant.

The camera and the draws change through the setters, which ask for a frame; a subclass with draws
of its own (a terrain from chunks) gives them in collectDraws. */
class SP_PUBLIC View3d : public Node {
public:
	virtual ~View3d() = default;

	virtual bool init() override;

	// the textures of the draws are loaded while the view is in a running scene
	virtual void handleEnter(xenolith::Scene *) override;
	virtual void handleExit() override;

	const Camera &getCamera() const { return _camera; }
	void setCamera(const Camera &);

	// ambient light, and the sun: a direction toward it and its colour times its intensity
	void setAmbient(const Color4F &);
	void setSun(const Vec3 &towardSun, const Color4F &);

	const Color4F &getAmbient() const { return _ambient; }
	const Vec3 &getSunDirection() const { return _sunDirection; }
	const Color4F &getSunColor() const { return _sunColor; }

	/* A pack of meshes (XL3dMeshPack.h) loads while a view retains it: once for every mesh that
	draws from it. The view's own draws retain the packs of their meshes by themselves (addDraw,
	clearDraws); a subclass with draws of its own retains and releases the packs of its meshes.
	A view out of a scene retains nothing: its packs are retained again when it enters one */
	void retainPack(MeshPack3d *);
	void releasePack(MeshPack3d *);

	// point lights; a frame takes up to MaxLights of them
	void addLight(const Light3d &);
	void clearLights();
	SpanView<Light3d> getLights() const { return _lights; }

	// the index of the draw, valid until clearDraws
	uint32_t addDraw(Draw3d &&);
	void setDrawTransform(uint32_t, const Mat4 &);
	void setDrawMaterial(uint32_t, Material3d &&);
	void clearDraws();

	SpanView<Draw3d> getDraws() const { return _draws; }

	// the view's block of a schema (its builder's make()), the same for every draw of a frame:
	// the pipelines that read one read it by block()
	void setBlock(Rc<MaterialData3d> &&);
	const Rc<MaterialData3d> &getBlock() const { return _block; }

	virtual void draw(FrameInfo &, NodeVisitFlags flags) override;

protected:
	// the draws and the lights of this frame: the ones added, by default
	virtual void collectDraws(ViewInput3d &);

	// a texture asks to be loaded while it has an owner: the frame context, as for a sprite
	void retainTexture(const Rc<Texture> &);
	void releaseTexture(const Rc<Texture> &);

	// every texture of a material, its data's too; of a material's data or a block
	void retainMaterial(const Material3d &);
	void releaseMaterial(const Material3d &);
	void retainData(const Rc<MaterialData3d> &);
	void releaseData(const Rc<MaterialData3d> &);

	Camera _camera;
	Vector<Draw3d> _draws;
	Vector<Rc<MeshPack3d>> _drawPacks; // the pack each of the draws retained, null for none
	Vector<Light3d> _lights;
	Rc<MaterialData3d> _block;
	Color4F _ambient = Color4F(0.3f, 0.3f, 0.35f, 1.0f);
	Vec3 _sunDirection = Vec3(-0.4f, 0.3f, 0.85f);
	Color4F _sunColor = Color4F(0.8f, 0.78f, 0.7f, 1.0f);
	bool _contextWarned = false;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DVIEW_H_ */
