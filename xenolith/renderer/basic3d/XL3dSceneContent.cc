/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

bool SceneContent3d::init() {
	if (!SceneContent2d::init()) {
		return false;
	}

	// replaces the 2d context SceneContent2d made: the 3d one is a 2d one as well
	_3dContext = Rc<FrameContext3d>::create();
	_2dContext = _3dContext;
	_frameContext = _3dContext;
	return true;
}

} // namespace stappler::xenolith::basic3d
