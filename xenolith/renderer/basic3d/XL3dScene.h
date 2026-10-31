/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DSCENE_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DSCENE_H_

#include "XL3d.h"
#include "XL2dScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

/* A 2d scene with a 3d layer under it: the stock basic2d queue (the Default type, with damage
tracking off - the 3d pass repaints the whole frame) and the 3d pass before its 2d pass, drawing
into the same Output. Its content is a SceneContent3d; View3d nodes in it draw the 3d views.

Local only: in a remote client the queue is the server's, and basic3d does not add its pass there. */
class SP_PUBLIC Scene3d : public basic2d::Scene2d {
public:
	virtual ~Scene3d() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &constraints) override;

protected:
	using basic2d::Scene2d::init;

	virtual void describeQueue(QueueInfo &) override;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DSCENE_H_ */
