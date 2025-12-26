/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "XLCoreResource.h"
#include "SPBitmap.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

struct Resource::ResourceData : memory::AllocPool {
	HashTable<BufferData *> buffers;
	HashTable<ImageData *> images;

	const Queue *owner = nullptr;
	bool compiled = false;
	bool externalPool = false;
	StringView key;
	memory::pool_t *pool = nullptr;

	void clear() {
		compiled = false;
		for (auto &it : buffers) {
			it->buffer = nullptr;
			it->atlas = nullptr;
			it->memCallback = nullptr;
			it->stdCallback = nullptr;
		}
		for (auto &it : images) {
			for (auto &iit : it->views) { iit->view = nullptr; }
			it->image = nullptr;
			it->atlas = nullptr;
			it->memCallback = nullptr;
			it->stdCallback = nullptr;
		}
	}
};

static size_t Resource_loadImageDirect(uint8_t *glBuffer, uint64_t expectedSize,
		BytesView encodedImageData, const bitmap::ImageInfo &imageInfo) {
	struct WriteData {
		uint8_t *buffer;
		uint32_t offset;
		uint32_t writableSize;
		uint64_t expectedSize;
	} data;

	data.buffer = glBuffer;
	data.offset = 0;
	data.expectedSize = expectedSize;

	bitmap::BitmapWriter w;
	w.target = &data;

	w.getStride = nullptr;
	w.push = [](void *ptr, const uint8_t *data, uint32_t size) {
		auto writeData = ((WriteData *)ptr);
		memcpy(writeData->buffer + writeData->offset, data, size);
		writeData->offset += size;
	};
	w.resize = [](void *ptr, uint32_t size) {
		auto writeData = ((WriteData *)ptr);
		if (size > writeData->expectedSize) {
			abort();
		}
		writeData->writableSize = size;
	};
	w.getData = [](void *ptr, uint32_t location) {
		auto writeData = ((WriteData *)ptr);
		return writeData->buffer + location;
	};
	w.assign = [](void *ptr, const uint8_t *data, uint32_t size) {
		auto writeData = ((WriteData *)ptr);
		memcpy(writeData->buffer, data, size);
		writeData->offset = size;
	};
	w.clear = [](void *ptr) { };

	imageInfo.format->load(encodedImageData.data(), encodedImageData.size(), w);

	return std::max(data.offset, data.writableSize);
}

static uint64_t Resource_loadImageConverted(StringView path, uint8_t *glBuffer,
		BytesView encodedImageData, const bitmap::ImageInfo &imageInfo, ImageFormat fmt) {
	Bitmap bmp(encodedImageData);

	switch (fmt) {
	case ImageFormat::R8G8B8A8_SRGB:
	case ImageFormat::R8G8B8A8_UNORM:
	case ImageFormat::R8G8B8A8_UINT:
		return bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::RGBA8888);
		break;
	case ImageFormat::R8G8B8_SRGB:
	case ImageFormat::R8G8B8_UNORM:
	case ImageFormat::R8G8B8_UINT:
		return bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::RGB888);
		break;
	case ImageFormat::R8G8_SRGB:
	case ImageFormat::R8G8_UNORM:
	case ImageFormat::R8G8_UINT:
		return bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::IA88);
		break;
	case ImageFormat::R8_SRGB:
	case ImageFormat::R8_UNORM:
	case ImageFormat::R8_UINT:
		if (bmp.alpha() == bitmap::AlphaFormat::Opaque) {
			return bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::I8);
		} else {
			return bmp.convertWithTarget(glBuffer, bitmap::PixelFormat::A8);
		}
		break;
	default:
		log::source().error("Resource", "loadImageConverted: ", path,
				": Invalid image format: ", getImageFormatName(fmt));
		break;
	}
	return 0;
}

static uint64_t Resource_loadImageDefault(StringView path, BytesView encodedImageData,
		ImageFormat fmt, const ImageData::DataCallback &dcb) {
	Bitmap bmp(encodedImageData);

	bool availableFormat = true;
	switch (fmt) {
	case ImageFormat::R8G8B8A8_SRGB:
	case ImageFormat::R8G8B8A8_UNORM:
	case ImageFormat::R8G8B8A8_UINT: bmp.convert(bitmap::PixelFormat::RGBA8888); break;
	case ImageFormat::R8G8B8_SRGB:
	case ImageFormat::R8G8B8_UNORM:
	case ImageFormat::R8G8B8_UINT: bmp.convert(bitmap::PixelFormat::RGB888); break;
	case ImageFormat::R8G8_SRGB:
	case ImageFormat::R8G8_UNORM:
	case ImageFormat::R8G8_UINT: bmp.convert(bitmap::PixelFormat::IA88); break;
	case ImageFormat::R8_SRGB:
	case ImageFormat::R8_UNORM:
	case ImageFormat::R8_UINT: bmp.convert(bitmap::PixelFormat::A8); break;
	default:
		availableFormat = false;
		log::source().error("Resource", "loadImageDefault: ", path,
				": Invalid image format: ", getImageFormatName(fmt));
		break;
	}

	if (availableFormat) {
		dcb(BytesView(bmp.dataPtr(), bmp.data().size()));
		return bmp.data().size();
	} else {
		dcb(BytesView());
	}
	return 0;
}

uint64_t Resource::loadImageMemoryData(uint8_t *ptr, uint64_t expectedSize, BytesView data,
		ImageFormat fmt, const ImageData::DataCallback &dcb) {
	bitmap::ImageInfo info;
	if (!bitmap::getImageInfo(data, info)) {
		log::source().error("Resource", "loadImageMmmoryData: fail to read image info");
	} else {
		if (ptr) {
			// check if we can load directly into GL memory
			switch (info.color) {
			case bitmap::PixelFormat::RGBA8888:
				switch (fmt) {
				case ImageFormat::R8G8B8A8_SRGB:
				case ImageFormat::R8G8B8A8_UNORM:
				case ImageFormat::R8G8B8A8_UINT:
					return Resource_loadImageDirect(ptr, expectedSize, data, info);
					break;
				default:
					return Resource_loadImageConverted(StringView(), ptr, data, info, fmt);
					break;
				}
				break;
			case bitmap::PixelFormat::RGB888:
				switch (fmt) {
				case ImageFormat::R8G8B8_SRGB:
				case ImageFormat::R8G8B8_UNORM:
				case ImageFormat::R8G8B8_UINT:
					return Resource_loadImageDirect(ptr, expectedSize, data, info);
					break;
				default:
					return Resource_loadImageConverted(StringView(), ptr, data, info, fmt);
					break;
				}
				break;
			case bitmap::PixelFormat::IA88:
				switch (fmt) {
				case ImageFormat::R8G8_SRGB:
				case ImageFormat::R8G8_UNORM:
				case ImageFormat::R8G8_UINT:
					return Resource_loadImageDirect(ptr, expectedSize, data, info);
					break;
				default:
					return Resource_loadImageConverted(StringView(), ptr, data, info, fmt);
					break;
				}
				break;
			case bitmap::PixelFormat::I8:
			case bitmap::PixelFormat::A8:
				switch (fmt) {
				case ImageFormat::R8_SRGB:
				case ImageFormat::R8_UNORM:
				case ImageFormat::R8_UINT:
					return Resource_loadImageDirect(ptr, expectedSize, data, info);
					break;
				default:
					return Resource_loadImageConverted(StringView(), ptr, data, info, fmt);
					break;
				}
				break;
			default:
				log::source().error("Resource", "loadImageMemoryData: Unknown format");
				dcb(BytesView());
				break;
			}
		} else {
			// use data callback
			return Resource_loadImageDefault(StringView(), data, fmt, dcb);
		}
	}
	return 0;
}

uint64_t Resource::loadImageFileData(uint8_t *ptr, uint64_t expectedSize, FileInfo path,
		ImageFormat fmt, const ImageData::DataCallback &dcb) {
	return memory::perform_temporary([&]() -> uint64_t {
		auto f = filesystem::openForReading(path);
		if (f) {
			auto fsize = f.size();
			auto mem = (uint8_t *)memory::pool::palloc(memory::pool::acquire(), fsize);
			f.seek(0, io::Seek::Set);
			f.read(mem, fsize);
			f.close();

			return loadImageMemoryData(ptr, expectedSize, BytesView(mem, fsize), fmt, dcb);
		} else {
			log::source().error("Resource", "loadImageFileData: ", path, ": fail to load file");
			dcb(BytesView());
		}
		return 0;
	});
};

Resource::Resource() { }

Resource::~Resource() {
	if (_data) {
		_data->clear();
		if (!_data->externalPool) {
			auto p = _data->pool;
			memory::pool::destroy(p);
		}
		_data = nullptr;
	}
}

bool Resource::init(Builder &&buf) {
	_data = buf._data;
	buf._data = nullptr;
	for (auto &it : _data->images) {
		it->resource = this;
		for (auto &iit : it->views) { iit->resource = this; }
	}
	for (auto &it : _data->buffers) { it->resource = this; }
	return true;
}

void Resource::clear() { _data->clear(); }

bool Resource::isCompiled() const { return _data->compiled; }

void Resource::setCompiled(bool value) { _data->compiled = value; }

const Queue *Resource::getOwner() const { return _data->owner; }

void Resource::setOwner(const Queue *q) { _data->owner = q; }

const HashTable<BufferData *> &Resource::getBuffers() const { return _data->buffers; }

const HashTable<ImageData *> &Resource::getImages() const { return _data->images; }

const BufferData *Resource::getBuffer(StringView key) const { return _data->buffers.get(key); }

const ImageData *Resource::getImage(StringView key) const { return _data->images.get(key); }

StringView Resource::getName() const { return _data->key; }

memory::pool_t *Resource::getPool() const { return _data->pool; }


template <typename T>
static T *Resource_conditionalInsert(HashTable<T *> &vec, StringView key, const Callback<T *()> &cb,
		memory::pool_t *pool) {
	auto it = vec.find(key);
	if (it == vec.end()) {
		T *obj = nullptr;
		perform([&] { obj = cb(); }, pool);
		if (obj) {
			return *vec.emplace(obj).first;
		}
	}
	return nullptr;
}

template <typename T>
static T *Resource_conditionalInsert(memory::vector<T *> &vec, StringView key,
		const Callback<T *()> &cb, memory::pool_t *pool) {
	T *obj = nullptr;
	perform([&] { obj = cb(); }, pool);
	if (obj) {
		return vec.emplace_back(obj);
	}
	return nullptr;
}

static void Resource_loadFileData(uint8_t *ptr, uint64_t size, StringView path,
		const BufferData::DataCallback &dcb) {
	memory::perform_temporary([&] {
		auto f = filesystem::openForReading(FileInfo(path));
		if (f) {
			uint64_t fsize = f.size();
			f.seek(0, io::Seek::Set);
			if (ptr) {
				f.read(ptr, std::min(fsize, size));
				f.close();
			} else {
				auto mem = (uint8_t *)memory::pool::palloc(memory::pool::acquire(), fsize);
				f.read(mem, fsize);
				f.close();
				dcb(BytesView(mem, fsize));
			}
		} else {
			dcb(BytesView());
		}
	});
};

Resource::Builder::Builder(StringView name)
: Builder(memory::pool::create((memory::pool_t *)nullptr), name) {
	_data->externalPool = false;
}

Resource::Builder::Builder(memory::pool_t *p, StringView name) {
	memory::perform([&] {
		_data = new (p) ResourceData;
		_data->pool = p;
		_data->key = name.pdup(p);
		_data->externalPool = true;
	}, p);
}

Resource::Builder::~Builder() {
	if (_data && !_data->externalPool) {
		auto p = _data->pool;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

const BufferData *Resource::Builder::addBufferByRef(StringView key, BufferInfo &&info,
		BytesView data, Rc<DataAtlas> &&atlas, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key,
			[&, this]() -> BufferData * {
		auto buf = new (_data->pool) BufferData();
		static_cast<BufferInfo &>(*buf) = move(info);
		buf->key = key.pdup(_data->pool);
		buf->data = data;
		buf->size = data.size();
		buf->atlas = move(atlas);
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}
const BufferData *Resource::Builder::addBuffer(StringView key, BufferInfo &&info,
		const FileInfo &path, Rc<DataAtlas> &&atlas, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	String npath;
	filesystem::enumeratePaths(path, filesystem::Access::Read,
			[&](StringView resourcePath, FileFlags) {
		npath = resourcePath.str<Interface>();
		return false;
	});

	if (npath.empty()) {
		log::source().error("Resource", "Fail to add buffer: ", key, ", file not found: ", path);
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key,
			[&, this]() -> BufferData * {
		auto fpath = StringView(npath).pdup(_data->pool);
		auto buf = new (_data->pool) BufferData;
		static_cast<BufferInfo &>(*buf) = move(info);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = [fpath](uint8_t *ptr, uint64_t size,
								   const BufferData::DataCallback &dcb) {
			Resource_loadFileData(ptr, size, fpath, dcb);
		};
		filesystem::Stat stat;
		if (filesystem::stat(FileInfo(npath), stat)) {
			buf->size = stat.size;
		}
		buf->atlas = move(atlas);
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}

const BufferData *Resource::Builder::addBuffer(StringView key, BufferInfo &&info, BytesView data,
		Rc<DataAtlas> &&atlas, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key,
			[&, this]() -> BufferData * {
		auto buf = new (_data->pool) BufferData();
		static_cast<BufferInfo &>(*buf) = move(info);
		buf->key = key.pdup(_data->pool);
		buf->data = data.pdup(_data->pool);
		buf->size = data.size();
		buf->atlas = move(atlas);
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}
const BufferData *Resource::Builder::addBuffer(StringView key, BufferInfo &&info,
		const memory::function<void(uint8_t *, uint64_t, const BufferData::DataCallback &)> &cb,
		Rc<DataAtlas> &&atlas, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add buffer: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<BufferData>(_data->buffers, key,
			[&, this]() -> BufferData * {
		auto buf = new (_data->pool) BufferData;
		static_cast<BufferInfo &>(*buf) = move(info);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = cb;
		buf->atlas = move(atlas);
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Buffer already added: ", key);
		return nullptr;
	}
	return p;
}

const ImageData *Resource::Builder::addBitmapImage(StringView key, ImageInfo &&img, BytesView data,
		AttachmentLayout layout, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&, this]() -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->data = data.pdup(_data->pool);
		buf->targetLayout = layout;
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

const ImageData *Resource::Builder::addEncodedImageByRef(StringView key, ImageInfo &&img,
		BytesView data, AttachmentLayout layout, AccessType access) {
	Extent3 extent;
	extent.depth = 1;
	CoderSource source(data);
	if (!bitmap::getImageSize(source, extent.width, extent.height)) {
		log::source().error("Resource", "Fail to add image: ", key,
				", fail to find image dimensions from data provided");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&, this]() -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = [data, format = img.format](uint8_t *ptr, uint64_t size,
								   const ImageData::DataCallback &dcb) {
			Resource::loadImageMemoryData(ptr, size, data, format, dcb);
		};
		buf->extent = extent;
		buf->targetLayout = layout;
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

const ImageData *Resource::Builder::addEncodedImage(StringView key, ImageInfo &&img, BytesView data,
		AttachmentLayout layout, AccessType access) {
	Extent3 extent;
	extent.depth = 1;

	CoderSource source(data);
	if (!bitmap::getImageSize(source, extent.width, extent.height)) {
		log::source().error("Resource", "Fail to add image: ", key,
				", fail to find image dimensions from data provided");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&, this]() -> ImageData * {
		auto d = data.pdup(_data->pool);
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = [d, format = img.format](uint8_t *ptr, uint64_t size,
								   const ImageData::DataCallback &dcb) {
			Resource::loadImageMemoryData(ptr, size, d, format, dcb);
		};
		buf->extent = extent;
		buf->targetLayout = layout;
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

const ImageData *Resource::Builder::addImage(StringView key, ImageInfo &&img, const FileInfo &path,
		AttachmentLayout layout, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	String npath;
	filesystem::enumeratePaths(path, filesystem::Access::Read,
			[&](StringView resourcePath, FileFlags) {
		npath = resourcePath.str<Interface>();
		return false;
	});

	if (npath.empty()) {
		log::source().error("Resource", "Fail to add image: ", key, ", file not found: ", path);
		return nullptr;
	}

	Extent3 extent;
	extent.depth = 1;
	if (!bitmap::getImageSize(path, extent.width, extent.height)) {
		log::source().error("Resource", "Fail to add image: ", key,
				", fail to find image dimensions: ", path);
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&, this]() -> ImageData * {
		auto fpath = FileInfo(path.path.pdup(_data->pool), path.category);
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = [fpath, format = img.format](uint8_t *ptr, uint64_t size,
								   const ImageData::DataCallback &dcb) {
			Resource::loadImageFileData(ptr, size, fpath, format, dcb);
		};
		buf->extent = extent;
		buf->targetLayout = layout;
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

const ImageData *Resource::Builder::addImage(StringView key, ImageInfo &&img,
		SpanView<FileInfo> data, AttachmentLayout layout, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	struct LoadableImageInfo {
		StringView path;
		Extent3 extent = {1, 1, 1};
	};

	Vector<LoadableImageInfo> images;

	for (auto &it : data) {
		String npath;
		filesystem::enumeratePaths(it, filesystem::Access::Read,
				[&](StringView resourcePath, FileFlags) {
			npath = resourcePath.str<Interface>();
			return false;
		});

		if (npath.empty()) {
			log::source().error("Resource", "Fail to add image: ", key, ", file not found: ", it);
			return nullptr;
		}

		Extent3 extent;
		extent.depth = 1;
		if (!bitmap::getImageSize(FileInfo(npath), extent.width, extent.height)) {
			log::source().error("Resource", "Fail to add image: ", key,
					", fail to find image dimensions: ", it);
			return nullptr;
		}

		if (!images.empty()) {
			if (images.front().extent != extent) {
				log::source().error("Resource", "Fail to add image: ", key,
						", fail to find image layer: ", it,
						", all images should have same extent (", images.front().extent,
						"), but layer have ", extent);
				return nullptr;
			}
		}
		images.emplace_back(LoadableImageInfo{StringView(npath).pdup(_data->pool), extent});
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&, this]() -> ImageData * {
		auto imagesData = makeSpanView(images).pdup(_data->pool);

		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = [imagesData, format = img.format](uint8_t *ptr, uint64_t size,
								   const ImageData::DataCallback &dcb) {
			for (auto &it : imagesData) {
				auto ret = Resource::loadImageFileData(ptr, size, FileInfo(it.path), format, dcb);
				if (ptr) {
					if (size >= ret) {
						ptr += ret;
						size -= ret;
					} else {
						break;
					}
				}
			}
		};
		buf->extent = imagesData.front().extent;
		if (buf->imageType == ImageType::Image3D) {
			buf->extent.depth = imagesData.size();
		} else {
			// assume image2d array
			buf->imageType = ImageType::Image2D;
			buf->arrayLayers = ArrayLayers(imagesData.size());
		}
		buf->targetLayout = layout;
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

const ImageData *Resource::Builder::addBitmapImageByRef(StringView key, ImageInfo &&img,
		BytesView data, AttachmentLayout layout, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&, this]() -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->data = data;
		buf->targetLayout = layout;
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}
const ImageData *Resource::Builder::addImage(StringView key, ImageInfo &&img,
		const memory::function<void(uint8_t *, uint64_t, const ImageData::DataCallback &)> &cb,
		AttachmentLayout layout, AccessType access) {
	if (!_data) {
		log::source().error("Resource", "Fail to add image: ", key, ", not initialized");
		return nullptr;
	}

	auto p = Resource_conditionalInsert<ImageData>(_data->images, key, [&, this]() -> ImageData * {
		auto buf = new (_data->pool) ImageData;
		static_cast<ImageInfo &>(*buf) = move(img);
		buf->key = key.pdup(_data->pool);
		buf->memCallback = cb;
		buf->targetLayout = layout;
		buf->targetAccess = access;
		return buf;
	}, _data->pool);
	if (!p) {
		log::source().error("Resource", _data->key, ": Image already added: ", key);
		return nullptr;
	}
	return p;
}

const ImageViewData *Resource::Builder::addImageView(const ImageData *data, ImageViewInfo &&info) {
	auto image = getImage(data->key);
	if (!image) {
		log::source().error("Resource", "Fail to add image view: no image for key: ", data->key);
		return nullptr;
	}

	for (auto &it : image->views) {
		if (info == *it) {
			log::source().error("Resource", "Fail to add image view: already exists: ", data->key);
			return it;
		}
	}

	return perform([&] {
		auto view = new (_data->pool) ImageViewData;
		if (info == ImageViewInfo()) {
			view->setup(*image);
		} else {
			static_cast<ImageViewInfo &>(*view) = data->getViewInfo(info);
		}
		const_cast<ImageData *>(image)->views.emplace_back(view);
		return view;
	}, _data->pool);
}

const BufferData *Resource::Builder::getBuffer(StringView key) const {
	return _data->buffers.get(key);
}

const ImageData *Resource::Builder::getImage(StringView key) const {
	return _data->images.get(key);
}

bool Resource::Builder::empty() const { return _data->buffers.empty() && _data->images.empty(); }

memory::pool_t *Resource::Builder::getPool() const { return _data->pool; }

} // namespace stappler::xenolith::core
