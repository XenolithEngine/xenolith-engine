/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

// Excluded from documentation/codegen tool
///@ SP_EXCLUDE

#include "XL3dShaders.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d::shaders {

#include "xl_3d_mesh.vert.h"
#include "xl_3d_mesh.frag.h"

SpanView<uint32_t> MeshVert(reinterpret_cast<const uint32_t *>(xl_3d_mesh_vert),
		sizeof(xl_3d_mesh_vert) / sizeof(uint32_t));
SpanView<uint32_t> MeshFrag(reinterpret_cast<const uint32_t *>(xl_3d_mesh_frag),
		sizeof(xl_3d_mesh_frag) / sizeof(uint32_t));

} // namespace stappler::xenolith::basic3d::shaders
