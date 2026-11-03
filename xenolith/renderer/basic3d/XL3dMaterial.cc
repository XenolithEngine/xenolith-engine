/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dMaterial.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

bool MaterialData3d::init(const MaterialSchema3d *schema, BytesView data,
		Vector<MaterialTexture3d> &&textures) {
	if (!schema || data.size() != schema->size || textures.size() != schema->textureCount) {
		log::source().error("basic3d::MaterialData3d", "a material of ",
				schema ? schema->name : "no schema", " has ", data.size(), " bytes and ",
				textures.size(), " textures, not ", schema ? schema->size : 0, " and ",
				schema ? schema->textureCount : 0);
		return false;
	}
	_schema = schema;
	_data = data.bytes<Interface>();
	_textures = sp::move(textures);
	return true;
}

} // namespace stappler::xenolith::basic3d
