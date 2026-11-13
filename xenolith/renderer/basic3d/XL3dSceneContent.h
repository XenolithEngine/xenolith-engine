/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DSCENECONTENT_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DSCENECONTENT_H_

#include "XL3dFrameContext.h"
#include "XL2dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

// The content of a Scene3d: a SceneContent2d whose frames also carry the 3d views
class SP_PUBLIC SceneContent3d : public basic2d::SceneContent2d {
public:
	virtual ~SceneContent3d() = default;

	virtual bool init() override;

protected:
	Rc<FrameContext3d> _3dContext;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DSCENECONTENT_H_ */
