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

// What xlkdl gpu makes of a material and of a view's block (glsl/basic3d-test.kdl): their schemas,
// and the bytes and the textures their builders hand the pass
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

	// a view's block is made as a material is
	using checks::Field3dTestBlock;
	auto &blockSchema = Field3dTestBlock::Schema;
	check(blockSchema.size == 24 && blockSchema.align == 8 && blockSchema.textureCount == 1
					&& blockSchema.textures[0].offset == 8
					&& blockSchema.textures[0].kind == MaterialTextureKind3d::Texture2d,
			"block: its schema, its texture field at 8",
			toString(blockSchema.size, " ", blockSchema.align));
	auto block = Field3dTestBlock{.origin = Vec2(16.0f, 32.0f), .time = 1.5f,
		.tiles = {1, 2, 3, 4, 5, 6, 7, 9}}.make();
	check(block && block->getSchema() == &blockSchema && block->getData().size() == 24
					&& readAt<Vec2>(block->getData(), 0) == Vec2(16.0f, 32.0f)
					&& readAt<uint32_t>(block->getData(), 8) == NoTexture
					&& readAt<float>(block->getData(), 12) == 1.5f
					&& block->getTextures().size() == 1,
			"block: the builder makes its bytes and its texture");

	// a field of bits: eight 3-bit values in one uint, each masked to its width, read back by the
	// function the header gives C++ and GLSL alike
	auto tiles = block ? readAt<uint32_t>(block->getData(), 16) : 0;
	checks::Field3dTest record;
	record.tiles = tiles;
	check(tiles == (1u | 2u << 3 | 3u << 6 | 4u << 9 | 5u << 12 | 6u << 15 | 7u << 18 | 1u << 21)
					&& checks::tilesAt(record, 2) == 3 && checks::tilesAt(record, 7) == 1,
			"block: a field of bits packs its values, the first lowest, and tilesAt reads them",
			toString(tiles));
}

} // namespace stappler::xenolith::basic3d
