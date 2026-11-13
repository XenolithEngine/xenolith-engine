/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XLCommon.h" // IWYU pragma: keep

#include "XL3dCamera.cc"
#include "XL3dMaterial.cc"
#include "XL3dVertex.cc"
#include "XL3dMesh.cc"
#include "XL3dMeshPack.cc"
#include "XL3dFrameContext.cc"
#include "XL3dSceneContent.cc"
#include "XL3dScene.cc"
#include "XL3dView.cc"

#ifdef MODULE_XENOLITH_BACKEND_VK
#include "backend/vk/XL3dVkPass.cc"
#endif
