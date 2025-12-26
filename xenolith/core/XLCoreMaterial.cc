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

#include "XLCoreMaterial.h"
#include "XLCoreAttachment.h"
#include "XLCoreDynamicImage.h"
#include "XLCoreLoop.h"
#include "XLCoreDevice.h"
#include "XLCoreTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

bool MaterialSet::init(uint32_t imagesInSet, const MaterialAttachment *owner) {
	_imagesInSet = imagesInSet;
	_owner = owner;
	return true;
}

bool MaterialSet::init(const Rc<MaterialSet> &other) {
	_generation = other->_generation + 1;
	_materials = other->_materials;
	_imagesInSet = other->_imagesInSet;
	_layouts = other->_layouts;
	_owner = other->_owner;
	_updatedMaterials = other->_updatedMaterials;

	for (auto &it : _layouts) { it.set = nullptr; }

	return true;
}

Vector<Rc<Material>> MaterialSet::updateMaterials(const Rc<MaterialInputData> &data,
		const Callback<Rc<ImageView>(const MaterialImage &)> &cb) {
	return updateMaterials(data->materialsToAddOrUpdate, data->dynamicMaterialsToUpdate,
			data->materialsToRemove, cb);
}

Vector<Rc<Material>> MaterialSet::updateMaterials(SpanView<Rc<Material>> materials,
		SpanView<MaterialId> dynamicMaterials, SpanView<MaterialId> materialsToRemove,
		const Callback<Rc<ImageView>(const MaterialImage &)> &cb) {
	Vector<MaterialId> updatedIds;
	updatedIds.reserve(materials.size() + dynamicMaterials.size());
	Vector<Rc<Material>> ret;
	ret.reserve(materials.size());

	for (auto &it : materialsToRemove) {
		auto mIt = _materials.find(it);
		if (mIt != _materials.end()) {
			ret.emplace_back(mIt->second);
			removeMaterial(mIt->second);
			for (auto &it : mIt->second->getImages()) {
				if (it.dynamic && _owner) {
					_owner->removeDynamicTracker(mIt->second->getId(), it.dynamic->image);
				}
			}
			_materials.erase(mIt);
		}
	}

	for (auto &material : materials) {
		bool isImagesValid = true;

		if (!materialsToRemove.empty()) {
			auto it = std::find(materialsToRemove.begin(), materialsToRemove.end(),
					material->getId());
			if (it != materialsToRemove.end()) {
				continue;
			}
		}

		for (auto &it : material->_images) {
			if (!it.image) {
				isImagesValid = false;
			}
			if (it.dynamic) {
				// try to actualize image
				auto current = it.dynamic->image->getInstance();
				if (current != it.dynamic) {
					if (material->_atlas == it.image->atlas) {
						material->_atlas = current->data.atlas;
					}
					it.dynamic = current;
					it.image = &it.dynamic->data;
				}
				if (_owner) {
					_owner->addDynamicTracker(material->getId(), it.dynamic->image);
				}
			}
		}

		if (!isImagesValid) {
			continue;
		}

		updatedIds.emplace_back(material->getId());

		auto mIt = _materials.find(material->getId());
		if (mIt != _materials.end()) {
			emplaceMaterialImages(mIt->second, material.get(), cb);
			mIt->second = move(material);
			ret.emplace_back(mIt->second.get());
			for (auto &it : mIt->second->getImages()) {
				if (it.dynamic && _owner) {
					_owner->removeDynamicTracker(material->getId(), it.dynamic->image);
				}
			}
		} else {
			auto it = _materials.emplace(material->getId(), move(material)).first;
			emplaceMaterialImages(nullptr, it->second.get(), cb);
			ret.emplace_back(it->second.get());
		}
	}

	for (auto &it : dynamicMaterials) {
		if (!materialsToRemove.empty()) {
			auto iit = std::find(materialsToRemove.begin(), materialsToRemove.end(), it);
			if (iit != materialsToRemove.end()) {
				continue;
			}
		}

		auto mIt = _materials.find(it);
		if (mIt != _materials.end()) {
			auto &material = mIt->second;
			bool hasUpdates = false;
			Vector<Rc<DynamicImageInstance>> dynamics;
			dynamics.reserve(mIt->second->getImages().size());

			for (auto &image : mIt->second->getImages()) {
				if (image.dynamic) {
					auto current = image.dynamic->image->getInstance();
					if (current != image.dynamic) {
						hasUpdates = true;
						dynamics.emplace_back(current);
					} else {
						dynamics.emplace_back(nullptr);
					}
				} else {
					dynamics.emplace_back(nullptr);
				}
			}

			if (hasUpdates) {
				// create new material
				Vector<MaterialImage> images = material->getImages();
				size_t i = 0;
				for (auto &it : images) {
					if (auto v = dynamics[i]) {
						it.dynamic = v;
						it.image = &v->data;
					}
					it.view = nullptr;
					++i;
				}

				auto mat = Rc<Material>::create(material, sp::move(images));

				for (auto &it : mat->getImages()) {
					if (it.dynamic && _owner) {
						_owner->addDynamicTracker(mat->getId(), it.dynamic->image);
					}
				}

				emplaceMaterialImages(mIt->second, mat.get(), cb);
				mIt->second = sp::move(mat);
				ret.emplace_back(mIt->second.get());
				for (auto &it : mIt->second->getImages()) {
					if (it.dynamic && _owner) {
						_owner->removeDynamicTracker(material->getId(), it.dynamic->image);
					}
				}
			}
		}
	}

	for (auto &it : ret) { emplace_ordered(_updatedMaterials, it->getId()); }

	return ret;
}

const MaterialLayout *MaterialSet::getLayout(uint32_t idx) const {
	if (idx < _layouts.size()) {
		return &_layouts[idx];
	}
	return nullptr;
}

const Material *MaterialSet::getMaterialById(MaterialId idx) const {
	auto it = _materials.find(idx);
	if (it != _materials.end()) {
		return it->second.get();
	}
	return nullptr;
}

const TextureSetLayoutData *MaterialSet::getTargetLayout() const {
	return _owner->getTargetLayout();
}

void MaterialSet::foreachUpdated(const Callback<void(MaterialId, NotNull<Material>)> &cb,
		bool clear) {
	for (auto &it : _updatedMaterials) {
		auto mIt = _materials.find(it);
		if (mIt != _materials.end()) {
			cb(mIt->first, mIt->second.get());
		}
	}
	if (clear) {
		_updatedMaterials.clear();
	}
}

void MaterialSet::removeMaterial(Material *oldMaterial) {
	auto &oldSet = _layouts[oldMaterial->getLayoutIndex()];
	for (auto &oIt : oldMaterial->_images) {
		--oldSet.imageSlots[oIt.descriptor].refCount;
		if (oldSet.imageSlots[oIt.descriptor].refCount == 0) {
			oldSet.imageSlots[oIt.descriptor].image = nullptr;
		}
		oIt.view = nullptr;
	}
}

void MaterialSet::emplaceMaterialImages(Material *oldMaterial, Material *newMaterial,
		const Callback<Rc<ImageView>(const MaterialImage &)> &cb) {
	Vector<MaterialImage> *oldImages = nullptr;
	uint32_t targetSet = maxOf<uint32_t>();
	if (oldMaterial) {
		targetSet = oldMaterial->getLayoutIndex();
		oldImages = &oldMaterial->_images;
	}

	auto &newImages = newMaterial->_images;
	if (oldImages) {
		auto &oldSet = _layouts[targetSet];
		// remove non-aliased images
		for (auto &oIt : *oldImages) {
			bool hasAlias = false;
			for (auto &nIt : newImages) {
				if (oIt.canAlias(nIt)) {
					hasAlias = true;
					break;
				}
			}
			if (!hasAlias) {
				--oldSet.imageSlots[oIt.descriptor].refCount;
				if (oldSet.imageSlots[oIt.descriptor].refCount == 0) {
					oldSet.imageSlots[oIt.descriptor].image = nullptr;
				}
				oIt.view = nullptr;
			}
		}
	}

	Vector<Pair<const MaterialImage *, Vector<uint32_t>>> uniqueImages;

	// find unique images
	uint32_t imageIdx = 0;
	for (auto &it : newImages) {
		it.info = it.image->getViewInfo(it.info);

		bool isAlias = false;
		for (auto &uit : uniqueImages) {
			if (uit.first->canAlias(it)) {
				uit.second.emplace_back(imageIdx);
				isAlias = true;
			}
		}
		if (!isAlias) {
			uniqueImages.emplace_back(pair(&it, Vector<uint32_t>({imageIdx})));
		}
		++imageIdx;
	}

	auto emplaceMaterial = [&, this](uint32_t setIdx, MaterialLayout &set,
								   Vector<uint32_t> &imageLocations) {
		if (imageLocations.empty()) {
			for (uint32_t imageIdx = 0; imageIdx < uniqueImages.size(); ++imageIdx) {
				imageLocations.emplace_back(imageIdx);
			}
		}

		uint32_t imageIdx = 0;
		for (auto &it : uniqueImages) {
			auto loc = imageLocations[imageIdx];
			if (set.imageSlots[loc].image) {
				// increment slot refcount, if image already exists
				set.imageSlots[loc].refCount += it.second.size();
			} else {
				// fill slot with new ImageView
				set.imageSlots[loc].image = cb(*it.first);
				set.imageSlots[loc].image->setLocation(setIdx, loc);
				set.imageSlots[loc].refCount = uint32_t(it.second.size());
				set.usedImageSlots = std::max(set.usedImageSlots, loc + 1);
			}

			// fill refs
			for (auto &iIt : it.second) {
				newImages[iIt].view = set.imageSlots[loc].image;
				newImages[iIt].set = setIdx;
				newImages[iIt].descriptor = loc;
			}

			++imageIdx;
		}

		newMaterial->setLayoutIndex(setIdx);

		if (oldImages) {
			auto &oldSet = _layouts[targetSet];
			// remove non-aliased images
			for (auto &oIt : *oldImages) {
				if (oIt.view) {
					--oldSet.imageSlots[oIt.descriptor].refCount;
					if (oldSet.imageSlots[oIt.descriptor].refCount == 0) {
						oldSet.imageSlots[oIt.descriptor].image = nullptr;
					}
					oIt.view = nullptr;
				}
			}
		}
	};

	auto tryToEmplaceSet = [&](uint32_t setIndex, MaterialLayout &set) -> bool {
		uint32_t emplacedImages = 0;
		Vector<uint32_t> imagePositions;
		imagePositions.resize(uniqueImages.size(), maxOf<uint32_t>());

		imageIdx = 0;
		// for each unique image, find it's potential place in set
		for (auto &uit : uniqueImages) {
			uint32_t location = 0;
			for (auto &it : set.imageSlots) {
				// check if image can alias with existed
				if (it.image && it.image->getImage() == uit.first->image->image
						&& it.image->getInfo() == uit.first->info) {
					if (imagePositions[imageIdx] == maxOf<uint32_t>()) {
						++emplacedImages; // mark as emplaced only if not emplaced already
					}
					imagePositions[imageIdx] = location;
					break; // stop searching - best variant
				} else if (!it.image || it.refCount == 0) {
					// only if not emplaced
					if (imagePositions[imageIdx] == maxOf<uint32_t>()) {
						if (std::find(imagePositions.begin(), imagePositions.end(), location)
								== imagePositions.end()) {
							++emplacedImages;
							imagePositions[imageIdx] = location;
						}
					}
					// continue searching for possible alias
				}
				++location;
				if (location > set.usedImageSlots + uniqueImages.size()) {
					break;
				}
			}

			++imageIdx;
		}

		// if all images emplaced, perform actual emplace and return
		if (emplacedImages == uniqueImages.size()) {
			emplaceMaterial(setIndex, set, imagePositions);
			return true;
		}
		return false;
	};

	if (targetSet != maxOf<uint32_t>()) {
		if (tryToEmplaceSet(targetSet, _layouts[targetSet])) {
			return;
		}
	}

	// process existed sets
	uint32_t setIndex = 0;
	for (auto &set : _layouts) {
		if (setIndex == targetSet) {
			continue;
		}

		if (tryToEmplaceSet(setIndex, set)) {
			return;
		}

		// or continue to search for appropriate set
		++setIndex;
	}

	// no available set, create new one;
	auto &nIt = _layouts.emplace_back(MaterialLayout());
	nIt.imageSlots.resize(_imagesInSet);

	Vector<uint32_t> imageLocations;
	emplaceMaterial(uint32_t(_layouts.size() - 1), nIt, imageLocations);
}

bool MaterialImage::canAlias(const MaterialImage &other) const {
	return other.image == image && other.info == info;
}

Material::~Material() {
	if (_ownedData) {
		_images.clear();
		delete _ownedData;
		_ownedData = nullptr;
	}
}

bool Material::init(MaterialId id, const PipelineData *pipeline, Vector<MaterialImage> &&images,
		Rc<Ref> &&data) {
	_id = id;
	_pipeline = pipeline;
	_images = sp::move(images);
	_data = move(data);
	for (auto &it : _images) {
		if (it.dynamic && it.dynamic->data.atlas) {
			_atlas = it.dynamic->data.atlas;
		}
	}
	return true;
}

bool Material::init(MaterialId id, const PipelineData *pipeline,
		const Rc<DynamicImageInstance> &image, Rc<Ref> &&data) {
	_id = id;
	_pipeline = pipeline;
	_images = Vector<MaterialImage>({MaterialImage{.image = &image->data, .dynamic = image}});
	_data = move(data);
	_atlas = image->data.atlas;
	return true;
}

bool Material::init(MaterialId id, const PipelineData *pipeline, const ImageData *image,
		Rc<Ref> &&data, bool ownedData) {
	_id = id;
	_pipeline = pipeline;
	_images = Vector<MaterialImage>({MaterialImage({image})});
	_atlas = image->atlas;
	if (ownedData) {
		_ownedData = image;
	}
	_data = move(data);
	return true;
}

bool Material::init(MaterialId id, const PipelineData *pipeline, const ImageData *image,
		ColorMode mode, Rc<Ref> &&data, bool ownedData) {
	_id = id;
	_pipeline = pipeline;

	MaterialImage img({image});

	img.info.setup(*image);
	img.info.setup(mode);

	_images = Vector<MaterialImage>({move(img)});
	_atlas = image->atlas;
	if (ownedData) {
		_ownedData = image;
	}
	_data = move(data);
	return true;
}

bool Material::init(const Material *master, Rc<ImageObject> &&image, Rc<DataAtlas> &&atlas,
		Rc<Ref> &&data) {
	_id = master->getId();
	_pipeline = master->getPipeline();

	auto otherData = master->getOwnedData();
	if (!otherData) {
		auto &images = master->getImages();
		if (images.size() > 0) {
			otherData = images[0].image;
		}
	}

	auto ownedData = new (std::nothrow) ImageData;
	static_cast<ImageInfoData &>(*ownedData) = image->getInfo();
	ownedData->image = move(image);
	ownedData->atlas = move(atlas);
	_ownedData = ownedData;

	_images = Vector<MaterialImage>({MaterialImage({_ownedData})});
	_data = move(data);
	return true;
}

bool Material::init(const Material *master, Vector<MaterialImage> &&images) {
	_id = master->getId();
	_pipeline = master->getPipeline();
	_images = sp::move(images);
	for (auto &it : _images) {
		if (it.image->atlas) {
			_atlas = it.image->atlas;
			break;
		}
	}
	_data = master->_data;
	return true;
}

void Material::setLayoutIndex(uint32_t idx) { _layoutIndex = idx; }

void Material::setBuffer(Rc<BufferObject> &&buf) { _buffer = move(buf); }

MaterialAttachment::~MaterialAttachment() {
	std::unique_lock<Mutex> lock(_dynamicMutex);
	for (auto &it : _dynamicTrackers) { it.first->removeTracker(this); }
}

bool MaterialAttachment::init(AttachmentBuilder &builder, const TextureSetLayoutData *layout) {
	if (!GenericAttachment::init(builder)) {
		return false;
	}

	_targetLayout = layout;
	return true;
}

void MaterialAttachment::addPredefinedMaterials(Vector<Rc<Material>> &&materials) {
	for (auto &it : materials) { it->_id = _attachmentMaterialId.fetch_add(1); }

	if (_predefinedMaterials.empty()) {
		_predefinedMaterials = sp::move(materials);
	} else {
		for (auto &it : materials) { _predefinedMaterials.emplace_back(move(it)); }
	}
}

const Rc<MaterialSet> &MaterialAttachment::getMaterials() const { return _materialSet; }

void MaterialAttachment::setMaterials(const Rc<MaterialSet> &data) const {
	auto tmp = _materialSet;
	_materialSet = data;
}

Bytes MaterialAttachment::getMaterialData(NotNull<Material>) const { return Bytes(); }

Rc<BufferObject> MaterialAttachment::allocateMaterialPersistentBuffer(NotNull<Material>) const {
	return nullptr;
}

Rc<MaterialSet> MaterialAttachment::allocateSet(const Device &dev, uint32_t imageCount) const {
	return Rc<MaterialSet>::create(imageCount, this);
}

Rc<MaterialSet> MaterialAttachment::cloneSet(const Rc<MaterialSet> &other) const {
	return Rc<MaterialSet>::create(other);
}

void MaterialAttachment::addDynamicTracker(MaterialId id, const Rc<DynamicImage> &image) const {
	std::unique_lock<Mutex> lock(_dynamicMutex);
	auto it = _dynamicTrackers.find(image);
	if (it != _dynamicTrackers.end()) {
		++it->second.refCount;
	} else {
		it = _dynamicTrackers.emplace(image, DynamicImageTracker{1}).first;
		image->addTracker(this);
	}

	auto iit = it->second.materials.find(id);
	if (iit != it->second.materials.end()) {
		++iit->second;
	} else {
		it->second.materials.emplace(id, 1);
	}
}

void MaterialAttachment::removeDynamicTracker(MaterialId id, const Rc<DynamicImage> &image) const {
	std::unique_lock<Mutex> lock(_dynamicMutex);
	auto it = _dynamicTrackers.find(image);
	if (it != _dynamicTrackers.end()) {
		auto iit = it->second.materials.find(id);
		if (iit != it->second.materials.end()) {
			--iit->second;
			if (iit->second == 0) {
				it->second.materials.erase(iit);
			}
		}
		--it->second.refCount;
		if (it->second.refCount == 0) {
			_dynamicTrackers.erase(it);
			image->removeTracker(this);
		}
	}
}

void MaterialAttachment::updateDynamicImage(Loop &loop, const DynamicImage *image,
		const Vector<Rc<DependencyEvent>> &deps) const {
	auto input = Rc<MaterialInputData>::alloc();
	input->attachment = this;
	std::unique_lock<Mutex> lock(_dynamicMutex);
	auto it = _dynamicTrackers.find(image);
	if (it != _dynamicTrackers.end()) {
		for (auto &materialIt : it->second.materials) {
			input->dynamicMaterialsToUpdate.emplace_back(materialIt.first);
		}
	}
	for (auto &it : deps) { it->addQueue(getCompiler()); }
	loop.compileMaterials(move(input), deps);
}

MaterialId MaterialAttachment::getNextMaterialId() const {
	return _attachmentMaterialId.fetch_add(1);
}

void MaterialAttachment::setCompiler(Queue *c) { _compiler = c; }

Queue *MaterialAttachment::getCompiler() const { return _compiler; }

void MaterialAttachment::setMaterialBuffer(NotNull<Material> m, Rc<BufferObject> &&buf) const {
	m->setBuffer(move(buf));
}

} // namespace stappler::xenolith::core
