/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DMATERIAL_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DMATERIAL_H_

#include "XLTexture.h"

// What a material of a schema is to the pass: its bytes in std430 and its textures. The schema's
// header (xlkdl gpu, docs/usage/tools/xlkdl-gpu.adoc) describes each material with a
// MaterialSchema3d and makes it with a builder of its own; this header includes nothing of basic3d,
// so a schema's header can include it.
namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

enum class MaterialTextureKind3d : uint8_t {
	Texture2d, // a slot of the texture array, binding 0
	TextureArray, // a slot of the layered one, binding 1
};

// A texture field of a material: the uint at `offset` the pass writes the texture's slot into
struct MaterialTextureField3d {
	uint32_t offset = 0;
	MaterialTextureKind3d kind = MaterialTextureKind3d::Texture2d;
};

// A material's record: its std430 size and alignment, and its texture fields in their order.
// Generated as a constant, so a pipeline and a material compare their schemas by address.
struct MaterialSchema3d {
	const char *name = nullptr;
	uint32_t size = 0;
	uint32_t align = 0;
	const MaterialTextureField3d *textures = nullptr;
	uint32_t textureCount = 0;
};

using MaterialTexture3d = Rc<Texture>;

/* A material of a schema: the bytes of its record and its textures, one for each texture field of
the schema in their order (null for none). Immutable once made, shared by the draws that use it
(Material3d::data) as a mesh is: a frame writes it once, however many draws it has. */
class SP_PUBLIC MaterialData3d : public Ref {
public:
	// a builder's record and textures, as the generated builders call it
	template <typename Record>
	static Rc<MaterialData3d> make(const MaterialSchema3d *schema, const Record &record,
			std::initializer_list<MaterialTexture3d> textures) {
		return Rc<MaterialData3d>::create(schema,
				BytesView(reinterpret_cast<const uint8_t *>(&record), sizeof(Record)),
				Vector<MaterialTexture3d>(textures));
	}

	virtual ~MaterialData3d() = default;

	// false when the bytes are not the schema's size or the textures not its texture fields
	bool init(const MaterialSchema3d *, BytesView, Vector<MaterialTexture3d> &&);

	const MaterialSchema3d *getSchema() const { return _schema; }
	BytesView getData() const { return _data; }
	SpanView<MaterialTexture3d> getTextures() const { return _textures; }

protected:
	const MaterialSchema3d *_schema = nullptr;
	Bytes _data;
	Vector<MaterialTexture3d> _textures;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DMATERIAL_H_ */
