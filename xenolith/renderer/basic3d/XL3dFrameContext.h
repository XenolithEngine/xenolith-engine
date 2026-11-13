/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DFRAMECONTEXT_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DFRAMECONTEXT_H_

#include "XL3d.h"
#include "XL2dFrameContext.h"
#include "XL2dCommandList.h"
#include "XLAppThread.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

/* A frame of a scene with a 3d layer: everything a 2d frame has, so every basic2d node below works
unchanged, and the views the 3d pass draws.
The views hold the app's objects - meshes, and through them their packs, and textures - and the
frame ends on another thread, maybe after the app let go of them: the handle hands the views back
to the app thread, so their last references go there. */
struct SP_PUBLIC FrameContextHandle3d : public basic2d::FrameContextHandle2d {
	virtual ~FrameContextHandle3d();

	Vector<ViewInput3d> views;
	Rc<AppThread> app;
};

/* The frame context of SceneContent3d. Besides the 2d inputs it feeds the 3d pass's input
attachment every frame, empty when no View3d was drawn: an unfed input would stall the frame. */
class SP_PUBLIC FrameContext3d : public basic2d::FrameContext2d {
public:
	static constexpr StringView InputAttachmentName = "Scene3dInput";

	virtual ~FrameContext3d() = default;

	virtual void onEnter(xenolith::Scene *) override;

	virtual Rc<FrameContextHandle> makeHandle(FrameInfo &) override;
	virtual void submitHandle(FrameInfo &, FrameContextHandle *) override;

protected:
	const core::AttachmentData *_inputAttachmentData = nullptr;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DFRAMECONTEXT_H_ */
