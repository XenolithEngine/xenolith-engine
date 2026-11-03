/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XLCommon.h"
#include "XL3d.h"
#include "Basic3dTestData.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

using test::check;

template <typename T>
static T readAt(BytesView data, size_t offset) {
	T ret;
	sprt::memcpy(&ret, data.data() + offset, sizeof(T));
	return ret;
}

// What xlkdl gpu makes of a material (glsl/basic3d-test.kdl): its schema, and the bytes and the
// textures its builder hands the pass
void performMaterialTests() {
	using checks::Terrain3dTestMaterial;

	auto &schema = Terrain3dTestMaterial::Schema;
	check(schema.size == 80 && schema.align == 16 && StringView(schema.name) == "Terrain3dTest",
			"material: the schema is the record's std430 size and alignment",
			toString(schema.size, " ", schema.align));
	check(schema.textureCount == 2 && schema.textures[0].offset == 16
					&& schema.textures[0].kind == MaterialTextureKind3d::TextureArray
					&& schema.textures[1].offset == 20
					&& schema.textures[1].kind == MaterialTextureKind3d::Texture2d,
			"material: its texture fields in their order, with their offsets and kinds");

	auto data = Terrain3dTestMaterial{
		.tint = Vec4(0.25f, 0.5f, 0.75f, 1.0f),
		.scale = Vec2(2.0f, 3.0f),
		.params = {Vec4(1.0f, 2.0f, 3.0f, 4.0f), Vec4(5.0f, 6.0f, 7.0f, 8.0f)},
		.flags = 0x15,
	}.make();
	check(data && data->getSchema() == &schema && data->getData().size() == 80,
			"material: the builder makes the schema's bytes");
	if (!data) {
		return;
	}

	auto bytes = data->getData();
	check(readAt<Vec4>(bytes, 0) == Vec4(0.25f, 0.5f, 0.75f, 1.0f)
					&& readAt<Vec2>(bytes, 24) == Vec2(2.0f, 3.0f)
					&& readAt<Vec4>(bytes, 48) == Vec4(5.0f, 6.0f, 7.0f, 8.0f)
					&& readAt<uint32_t>(bytes, 64) == 0x15,
			"material: its fields at their offsets, the array's second element at 48");
	check(readAt<uint32_t>(bytes, 16) == NoTexture && readAt<uint32_t>(bytes, 20) == NoTexture,
			"material: no slot in the texture fields until a frame writes one");
	check(readAt<uint32_t>(bytes, 68) == 0 && readAt<uint32_t>(bytes, 72) == 0
					&& readAt<uint32_t>(bytes, 76) == 0,
			"material: the padding is zero");
	check(data->getTextures().size() == 2, "material: a texture for each texture field");

	// bytes or textures of another size than the schema's are refused
	Bytes short_(16);
	check(!Rc<MaterialData3d>::create(&schema, short_, Vector<MaterialTexture3d>(2)),
			"material: bytes of another size are refused");
	Bytes whole(80);
	check(!Rc<MaterialData3d>::create(&schema, whole, Vector<MaterialTexture3d>(1)),
			"material: a texture short is refused");
}

} // namespace stappler::xenolith::basic3d
