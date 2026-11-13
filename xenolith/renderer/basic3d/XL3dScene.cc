/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dScene.h"
#include "XLAppThread.h"

#ifdef MODULE_XENOLITH_BACKEND_VK
#include "backend/vk/XL3dVkPass.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

bool Scene3d::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints) {
	return init(app, window, constraints, Vector<Pipeline3dInfo>());
}

bool Scene3d::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints, Vector<Pipeline3dInfo> &&pipelines) {
	if (!app->isServerThread()) {
		log::source().error("basic3d::Scene3d",
				"a remote client is not supported: the 3d pass is not in the server's queue");
	}

	auto extent = Extent2(constraints.extent.width, constraints.extent.height);
	return basic2d::Scene2d::init(app, window, [&](core::Queue::Builder &builder) {
#ifdef MODULE_XENOLITH_BACKEND_VK
		if (!vk::Pass3d::addToQueue(app->getGlLoop(), builder, extent, pipelines)) {
			log::source().error("basic3d::Scene3d", "fail to add the 3d pass");
		}
#else
		log::source().error("basic3d::Scene3d", "no backend for the 3d pass in this build");
#endif
	}, constraints);
}

void Scene3d::describeQueue(QueueInfo &info) {
	basic2d::Scene2d::describeQueue(info);

	// The 2d pass loads what the 3d pass drew only in the Default queue; and partial redraw would
	// leave the 2d content outside its damage erased by the full 3d repaint.
	info.type = basic2d::Scene2d::QueueType::Default;
	info.damage = core::QueueDamageFlags::None;
}

} // namespace stappler::xenolith::basic3d
