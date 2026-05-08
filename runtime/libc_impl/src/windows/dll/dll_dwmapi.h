#include "dlltable.h"

namespace sprt {

struct DwmapiTable : DllTable {
	DwmapiTable() : DllTable(L"dwapi.dll", &__dwmapi_begin, &__dwmapi_end) { }

	DllTableRecord __dwmapi_begin;
	SPWIN_DEFINE_PROTO(DwmpDxGetWindowSharedSurface)
	SPWIN_DEFINE_PROTO(DwmpDxUpdateWindowSharedSurface)
	SPWIN_DEFINE_PROTO(DwmEnableComposition)
	SPWIN_DEFINE_PROTO(DwmAttachMilContent)
	SPWIN_DEFINE_PROTO(DwmDefWindowProc)
	SPWIN_DEFINE_PROTO(DwmDetachMilContent)
	SPWIN_DEFINE_PROTO(DwmEnableBlurBehindWindow)
	SPWIN_DEFINE_PROTO(DwmEnableMMCSS)
	SPWIN_DEFINE_PROTO(DwmpDxBindSwapChain)
	SPWIN_DEFINE_PROTO(DwmpDxUnbindSwapChain)
	SPWIN_DEFINE_PROTO(DwmpDxgiIsThreadDesktopComposited)
	SPWIN_DEFINE_PROTO(DwmpDxUpdateWindowRedirectionBltSurface)
	SPWIN_DEFINE_PROTO(DwmpRenderFlick)
	SPWIN_DEFINE_PROTO(DwmpAllocateSecurityDescriptor)
	SPWIN_DEFINE_PROTO(DwmpFreeSecurityDescriptor)
	SPWIN_DEFINE_PROTO(DwmpEnableDDASupport)
	SPWIN_DEFINE_PROTO(DwmExtendFrameIntoClientArea)
	SPWIN_DEFINE_PROTO(DwmTetherTextContact)
	SPWIN_DEFINE_PROTO(DwmFlush)
	SPWIN_DEFINE_PROTO(DwmGetColorizationColor)
	SPWIN_DEFINE_PROTO(DwmGetCompositionTimingInfo)
	SPWIN_DEFINE_PROTO(DwmGetGraphicsStreamClient)
	SPWIN_DEFINE_PROTO(DwmGetGraphicsStreamTransformHint)
	SPWIN_DEFINE_PROTO(DwmGetTransportAttributes)
	SPWIN_DEFINE_PROTO(DwmGetWindowAttribute)
	SPWIN_DEFINE_PROTO(DwmInvalidateIconicBitmaps)
	SPWIN_DEFINE_PROTO(DwmIsCompositionEnabled)
	SPWIN_DEFINE_PROTO(DwmModifyPreviousDxFrameDuration)
	SPWIN_DEFINE_PROTO(DwmQueryThumbnailSourceSize)
	SPWIN_DEFINE_PROTO(DwmRegisterThumbnail)
	SPWIN_DEFINE_PROTO(DwmSetDxFrameDuration)
	SPWIN_DEFINE_PROTO(DwmSetIconicLivePreviewBitmap)
	SPWIN_DEFINE_PROTO(DwmSetIconicThumbnail)
	SPWIN_DEFINE_PROTO(DwmSetPresentParameters)
	SPWIN_DEFINE_PROTO(DwmSetWindowAttribute)
	SPWIN_DEFINE_PROTO(DwmShowContact)
	SPWIN_DEFINE_PROTO(DwmUnregisterThumbnail)
	SPWIN_DEFINE_PROTO(DwmUpdateThumbnailProperties)
	DllTableRecord __dwmapi_end;
};

} // namespace sprt
