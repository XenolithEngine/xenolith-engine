/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef XENOLITH_CORE_XLCOREMATERIAL_H_
#define XENOLITH_CORE_XLCOREMATERIAL_H_

#include "XLCoreObject.h"
#include "XLCoreResource.h"
#include "XLCoreAttachment.h"
#include "XLCoreDynamicImage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class Material;
class MaterialSet;
class MaterialAttachment;

using MaterialId = uint32_t;

struct SP_PUBLIC MaterialInputData : AttachmentInputData {
	const MaterialAttachment *attachment;
	Vector<Rc<Material>> materialsToAddOrUpdate;
	Vector<MaterialId> materialsToRemove;
	Vector<MaterialId> dynamicMaterialsToUpdate;
	Function<void()> callback;
};

struct SP_PUBLIC MaterialImage {
	const ImageData *image;
	Rc<DynamicImageInstance> dynamic;
	ImageViewInfo info;
	Rc<ImageView> view;
	uint16_t sampler = 0;
	uint32_t set;
	uint32_t descriptor;

	bool canAlias(const MaterialImage &) const;
};

class SP_PUBLIC MaterialSet final : public Ref {
public:
	using ImageSlot = MaterialImageSlot;

	virtual ~MaterialSet() = default;

	bool init(uint32_t imagesInSet, const MaterialAttachment * = nullptr);
	bool init(const Rc<MaterialSet> &);

	Vector<Rc<Material>> updateMaterials(const Rc<MaterialInputData> &,
			const Callback<Rc<ImageView>(const MaterialImage &)> &);
	Vector<Rc<Material>> updateMaterials(SpanView<Rc<Material>> materials,
			SpanView<MaterialId> dynamic, SpanView<MaterialId> remove,
			const Callback<Rc<ImageView>(const MaterialImage &)> &);

	uint32_t getImagesInSet() const { return _imagesInSet; }
	uint64_t getGeneration() const { return _generation; }
	const HashMap<MaterialId, Rc<Material>> &getMaterials() const { return _materials; }

	const MaterialAttachment *getOwner() const { return _owner; }

	Vector<MaterialLayout> &getLayouts() { return _layouts; }
	const MaterialLayout *getLayout(uint32_t) const;
	const Material *getMaterialById(MaterialId) const;

	const TextureSetLayoutData *getTargetLayout() const;

	void foreachUpdated(const Callback<void(MaterialId, NotNull<Material>)> &, bool clear);

protected:
	void removeMaterial(Material *oldMaterial);
	void emplaceMaterialImages(Material *oldMaterial, Material *newMaterial,
			const Callback<Rc<ImageView>(const MaterialImage &)> &);

	uint32_t _imagesInSet = 16;

	uint32_t _generation = 1;
	HashMap<MaterialId, Rc<Material>> _materials;

	// describes image location in descriptor sets
	// all images from same material must be in one set
	Vector<MaterialLayout> _layouts;
	const MaterialAttachment *_owner = nullptr;
	Vector<MaterialId> _updatedMaterials;
};

class SP_PUBLIC Material final : public Ref {
public:
	// Использовать только для определения встроенных в аттачмент материалов
	static constexpr auto MaterialIdInitial = maxOf<uint32_t>();

	using PipelineData = GraphicPipelineData;

	virtual ~Material();

	// view for image must be empty
	bool init(MaterialId, const PipelineData *, Vector<MaterialImage> &&, Rc<Ref> && = Rc<Ref>());
	bool init(MaterialId, const PipelineData *, const Rc<DynamicImageInstance> &,
			Rc<Ref> && = Rc<Ref>());
	bool init(MaterialId, const PipelineData *, const ImageData *, Rc<Ref> && = Rc<Ref>(),
			bool ownedData = false);
	bool init(MaterialId, const PipelineData *, const ImageData *, ColorMode,
			Rc<Ref> && = Rc<Ref>(), bool ownedData = false);
	bool init(const Material *, Rc<ImageObject> &&, Rc<DataAtlas> &&, Rc<Ref> && = Rc<Ref>());
	bool init(const Material *, Vector<MaterialImage> &&);

	MaterialId getId() const { return _id; }
	const PipelineData *getPipeline() const { return _pipeline; }
	const Vector<MaterialImage> &getImages() const { return _images; }

	uint32_t getLayoutIndex() const { return _layoutIndex; }

	DataAtlas *getAtlas() const { return _atlas; }
	BufferObject *getBuffer() const { return _buffer; }

	const ImageData *getOwnedData() const { return _ownedData; }

protected:
	friend class MaterialSet;
	friend class MaterialAttachment;

	void setLayoutIndex(uint32_t);
	void setBuffer(Rc<BufferObject> &&);

	bool _dirty = true;
	MaterialId _id = 0;
	uint32_t _layoutIndex = 0; // set after compilation
	const PipelineData *_pipeline;
	Vector<MaterialImage> _images;
	Rc<DataAtlas> _atlas;
	Rc<BufferObject> _buffer;
	Rc<Ref> _data;
	const ImageData *_ownedData = nullptr;
};

// this attachment should provide material data buffer for rendering
class SP_PUBLIC MaterialAttachment : public GenericAttachment {
public:
	virtual ~MaterialAttachment();

	virtual bool init(AttachmentBuilder &builder, const TextureSetLayoutData *);

	void addPredefinedMaterials(Vector<Rc<Material>> &&);

	const Rc<MaterialSet> &getMaterials() const;
	void setMaterials(const Rc<MaterialSet> &) const;

	SpanView<Rc<Material>> getPredefinedMaterials() const { return _predefinedMaterials; }

	const TextureSetLayoutData *getTargetLayout() const { return _targetLayout; }

	virtual Bytes getMaterialData(NotNull<Material>) const;

	virtual Rc<BufferObject> allocateMaterialPersistentBuffer(NotNull<Material>) const;

	virtual Rc<MaterialSet> allocateSet(const Device &, uint32_t imageCount) const;
	virtual Rc<MaterialSet> cloneSet(const Rc<MaterialSet> &) const;

	virtual void addDynamicTracker(MaterialId, const Rc<DynamicImage> &) const;
	virtual void removeDynamicTracker(MaterialId, const Rc<DynamicImage> &) const;

	virtual void updateDynamicImage(Loop &, const DynamicImage *,
			const Vector<Rc<DependencyEvent>> & = Vector<Rc<DependencyEvent>>()) const;

	MaterialId getNextMaterialId() const;

	void setCompiler(Queue *);
	Queue *getCompiler() const;

protected:
	using GenericAttachment::init;

	void setMaterialBuffer(NotNull<Material>, Rc<BufferObject> &&) const;

	struct DynamicImageTracker {
		uint32_t refCount;
		Map<MaterialId, uint32_t> materials;
	};

	Queue *_compiler = nullptr;
	const TextureSetLayoutData *_targetLayout = nullptr;
	mutable std::atomic<MaterialId> _attachmentMaterialId = 1;
	mutable Rc<MaterialSet> _materialSet;
	Vector<Rc<Material>> _predefinedMaterials;

	mutable Mutex _dynamicMutex;
	mutable Map<Rc<DynamicImage>, DynamicImageTracker> _dynamicTrackers;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREMATERIAL_H_ */
