/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLGlesObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// Clear callbacks run on whatever thread drops the last reference to the object, so they may not
// call GL here (the context belongs to the loop thread). They hand a single delete call to the
// device's deferred queue instead; when the device is already gone its teardown reclaims every
// name and there is nothing to do.
static void GlesObject_clearBuffer(core::Device *dev, core::ObjectType, core::ObjectHandle handle,
		void *) {
	auto d = static_cast<Device *>(dev);
	if (!d || !d->isAlive()) { return; }

	const GLuint name = glObjectName(handle);
	d->scheduleRelease([t = &d->getTable(), name]() { t->glDeleteBuffers(1, &name); });
}

static void GlesObject_clearImage(core::Device *dev, core::ObjectType, core::ObjectHandle handle,
		void *) {
	auto d = static_cast<Device *>(dev);
	if (!d || !d->isAlive()) { return; }

	const GLuint name = glObjectName(handle);
	d->scheduleRelease([t = &d->getTable(), name]() { t->glDeleteTextures(1, &name); });
}

static void GlesObject_clearSampler(core::Device *dev, core::ObjectType,
		core::ObjectHandle handle, void *) {
	auto d = static_cast<Device *>(dev);
	if (!d || !d->isAlive()) { return; }

	const GLuint name = glObjectName(handle);
	d->scheduleRelease([t = &d->getTable(), name]() { t->glDeleteSamplers(1, &name); });
}

static void GlesObject_clearFramebuffer(core::Device *dev, core::ObjectType,
		core::ObjectHandle handle, void *) {
	auto d = static_cast<Device *>(dev);
	if (!d || !d->isAlive()) { return; }

	const GLuint name = glObjectName(handle);
	d->scheduleRelease([t = &d->getTable(), name]() { t->glDeleteFramebuffers(1, &name); });
}

// The fence dies without its sync having been reset (it was still armed): delete the GLsync so it
// does not outlive everything that could have signalled it.
static void GlesObject_clearFence(core::Device *dev, core::ObjectType, core::ObjectHandle,
		void *ptr) {
	auto fence = static_cast<Fence *>(ptr);
	auto d = static_cast<Device *>(dev);
	if (!fence || !d || !d->isAlive()) { return; }

	const GLsync sync = fence->getGlSync();
	fence->setGlSync(nullptr);
	d->scheduleRelease([t = &d->getTable(), sync]() { if (sync) { t->glDeleteSync(sync); } });
}

bool Buffer::init(Device &dev, const core::BufferData &data) {
	_info = data;

	auto &table = dev.getTable();
	if (!table.glGenBuffers || !table.glBufferData) {
		log::source().error("gles::Buffer", "GL buffer entrypoints are missing");
		return false;
	}

	GLuint name = 0;
	table.glGenBuffers(1, &name);
	if (name == 0) {
		log::source().error("gles::Buffer", "Fail to allocate a GL buffer, error ",
				EGLint(table.eglGetError()));
		return false;
	}
	_glBuffer = name;

	const auto target = GL_COPY_WRITE_BUFFER;
	auto usage = data.persistent ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;

	if (!data.data.empty() || data.memCallback || data.stdCallback) {
		Bytes staging;
		staging.resize(size_t(data.size), uint8_t(0));
		data.writeData(staging.data(), size_t(data.size));

		table.glBindBuffer(target, name);
		table.glBufferData(target, GLsizei(data.size), staging.data(), usage);
	} else if (data.size > 0) {
		// No source to fill from: the storage exists with undefined content. Nothing in M1 reads
		// such a buffer back, and zeroing a large allocation here would cost more than it saves.
		table.glBindBuffer(target, name);
		table.glBufferData(target, GLsizei(data.size), nullptr, usage);
	}

	table.glBindBuffer(target, 0);

	return core::BufferObject::init(dev, GlesObject_clearBuffer, core::ObjectType::Buffer,
			glObjectHandle(_glBuffer));
}

bool Image::setup(Device &dev, const core::ImageInfoData &info,
		const Callback<size_t(uint8_t *, uint64_t)> *fill, uint64_t requestedIndex) {
	_info = info;

	auto fmt = getGlFormat(info.format);
	if (fmt.internalFormat == 0) {
		log::source().error("gles::Image", "Unsupported image format: ",
				core::getImageFormatName(info.format));
		return false;
	}

	// M1 renders into single-layer 2D textures only; layered and 3D images come with the draw path.
	if (info.imageType != core::ImageType::Image2D
			|| sprt::max(uint32_t(info.arrayLayers.get()), uint32_t(1)) > 1
			|| info.extent.depth > 1) {
		log::source().error("gles::Image", "Layered or 3D images are not supported in M1");
		return false;
	}

	if (sprt::max(uint32_t(info.mipLevels.get()), uint32_t(1)) > 1) {
		log::source().error("gles::Image", "Mipmapped textures are not supported in M1");
		return false;
	}

	auto width = uint32_t(info.extent.width);
	auto height = uint32_t(info.extent.height);
	if (width == 0 || height == 0) {
		log::source().error("gles::Image", "Zero-sized image extent");
		return false;
	}

	auto &table = dev.getTable();
	if (!table.glGenTextures || !table.glTexStorage2D || !table.glBindTexture) {
		log::source().error("gles::Image", "GL texture entrypoints are missing");
		return false;
	}

	GLuint name = 0;
	table.glGenTextures(1, &name);
	if (name == 0) {
		log::source().error("gles::Image", "Fail to allocate a GL texture, error ",
				EGLint(table.eglGetError()));
		return false;
	}
	_glTexture = name;

	table.glBindTexture(GL_TEXTURE_2D, name);
	table.glTexStorage2D(GL_TEXTURE_2D, 1, fmt.internalFormat, GLsizei(width), GLsizei(height));

	// A sampled image must never read garbage: with no fill callback the allocation is zeroed on
	// the host and uploaded like any other (glClearTexImage is not in the resolved set).
	auto pixelSize = getGlPixelSize(info.format);
	Bytes staging;
	staging.resize(size_t(width) * size_t(height) * size_t(pixelSize), uint8_t(0));
	if (fill) {
		(*fill)(staging.data(), uint64_t(staging.size()));
	}

	table.glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // R8 rows are not naturally aligned
	table.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GLsizei(width), GLsizei(height), fmt.format,
			fmt.type, staging.data());

	table.glBindTexture(GL_TEXTURE_2D, 0);

	return core::ImageObject::init(dev, GlesObject_clearImage, core::ObjectType::Image,
			glObjectHandle(_glTexture), nullptr,
			requestedIndex != 0 ? requestedIndex : dev.getNextObjectIndex());
}

bool Image::init(Device &dev, StringView name, const core::ImageInfoData &info) {
	if (!setup(dev, info, nullptr)) {
		return false;
	}
	setName(name);
	return true;
}

bool Image::init(Device &dev, StringView name, const core::ImageInfoData &info, uint64_t index) {
	if (!setup(dev, info, nullptr, index)) {
		return false;
	}
	setName(name);
	return true;
}

bool Image::init(Device &dev, StringView name, const core::ImageInfoData &info, BytesView initialData) {
	if (initialData.empty()) {
		return init(dev, name, info);
	}

	auto fill = [data = initialData](uint8_t *mem, uint64_t size) -> size_t {
		if (size == 0 || !data.data()) { return 0; }
		const auto n = sprt::min(size, uint64_t(data.size()));
		sprt::memcpy(mem, data.data(), size_t(n));
		return size_t(n);
	};

	// Callback does not own the lambda it wraps (it points at it), so both must live until setup
	// has called fill synchronously.
	auto cb = Callback<size_t(uint8_t *, uint64_t)>(fill);
	if (!setup(dev, info, &cb)) {
		return false;
	}
	setName(name);
	return true;
}

bool Image::init(Device &dev, const core::ImageData &data) {
	bool hasSource = !data.data.empty() || data.memCallback != nullptr || data.stdCallback != nullptr;
	auto fill = [&](uint8_t *mem, uint64_t size) -> size_t { return data.writeData(mem, size_t(size)); };

	// Callback does not own the lambda it wraps (it points at it), so both must live until setup
	// has called fill synchronously.
	auto cb = Callback<size_t(uint8_t *, uint64_t)>(fill);
	if (!setup(dev, core::ImageInfoData(data), hasSource ? &cb : nullptr)) {
		return false;
	}
	_atlas = data.atlas;
	setName(data.key);
	return true;
}

bool ImageView::init(Device &dev, const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	_image = image;
	_info = image->getViewInfo(info);

	// Views are keyed by index in the framebuffer cache; a fresh one per view is required.
	_index = dev.getNextObjectIndex();

	return core::ImageView::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::ImageView, core::ObjectHandle::zero());
}

bool Sampler::init(Device &dev, const core::SamplerInfo &info) {
	_info = info;

	auto mapFilter = [](core::Filter filter, GLint &out) -> bool {
		switch (filter) {
		case core::Filter::Nearest: out = GL_NEAREST; return true; break;
		case core::Filter::Linear: out = GL_LINEAR; return true; break;
		default: return false; // Cubic has no GLES counterpart
		}
	};

	auto mapWrap = [](core::SamplerAddressMode mode) -> GLint {
		switch (mode) {
		case core::SamplerAddressMode::Repeat: return GL_REPEAT;
		case core::SamplerAddressMode::MirroredRepeat: return GL_MIRRORED_REPEAT;
		default: // ClampToEdge and the border variant, which needs no border colour in M1
			return GL_CLAMP_TO_EDGE;
		}
	};

	GLint minFilter = 0;
	GLint magFilter = 0;
	if (!mapFilter(info.minFilter, minFilter) || !mapFilter(info.magFilter, magFilter)) {
		log::source().error("gles::Sampler", "Cubic filtering is not supported");
		return false;
	}

	auto &table = dev.getTable();
	if (!table.glGenSamplers || !table.glSamplerParameteri) {
		log::source().error("gles::Sampler", "GL sampler entrypoints are missing");
		return false;
	}

	GLuint name = 0;
	table.glGenSamplers(1, &name);
	if (name == 0) {
		log::source().error("gles::Sampler", "Fail to allocate a GL sampler, error ",
				EGLint(table.eglGetError()));
		return false;
	}
	_glSampler = name;

	// Single-level textures only in M1: the min filter is the plain one, whatever the mipmap mode
	// asked for - there are no mip levels to choose between.
	table.glSamplerParameteri(name, GL_TEXTURE_MIN_FILTER, minFilter);
	table.glSamplerParameteri(name, GL_TEXTURE_MAG_FILTER, magFilter);
	table.glSamplerParameteri(name, GL_TEXTURE_WRAP_S, mapWrap(info.addressModeU));
	table.glSamplerParameteri(name, GL_TEXTURE_WRAP_T, mapWrap(info.addressModeV));

	return core::Sampler::init(dev, GlesObject_clearSampler, core::ObjectType::Sampler,
			glObjectHandle(_glSampler));
}

bool Framebuffer::init(Device &dev, const core::QueuePassData *pass,
		SpanView<Rc<core::ImageView>> views) {
	_renderPass = pass->impl;
	if (!views.empty()) {
		auto extent = views.front()->getFramebufferExtent();
		_extent = Extent2(extent.width, extent.height);
		_layerCount = extent.depth;
	}
	for (auto &it : views) {
		_imageViews.emplace_back(it);
		_viewIds.emplace_back(it->getIndex());
	}

	auto &table = dev.getTable();
	if (!table.glGenFramebuffers || !table.glBindFramebuffer || !table.glCheckFramebufferStatus
			|| !table.glFramebufferTexture2D) {
		log::source().error("gles::Framebuffer", "GL framebuffer entrypoints are missing");
		return false;
	}

	GLuint name = 0;
	table.glGenFramebuffers(1, &name);
	if (name == 0) {
		log::source().error("gles::Framebuffer", "Fail to allocate a GL framebuffer, error ",
				EGLint(table.eglGetError()));
		return false;
	}
	_glFbo = name;

	auto fail = [&](StringView reason) -> bool {
		log::source().error("gles::Framebuffer", "Incomplete framebuffer: ", reason);
		table.glDeleteFramebuffers(1, &_glFbo); // init runs on the loop thread, this is safe
		_glFbo = 0;
		return false;
	};

	bool attached = true;
	for (size_t i = 0; i < views.size(); ++i) {
		auto image = views[i]->getImage().get_cast<Image>();
		if (!image) {
			log::source().error("gles::Framebuffer", "Attachment ", i, " has no GLES texture");
			attached = false;
			break;
		}

		table.glBindFramebuffer(GL_FRAMEBUFFER, name);
		table.glFramebufferTexture2D(GL_FRAMEBUFFER, GLenum(GL_COLOR_ATTACHMENT0 + i),
				GL_TEXTURE_2D, image->getGlName(), 0);
	}
	if (!attached) {
		return fail("an attachment is not a texture");
	}

	table.glBindFramebuffer(GL_FRAMEBUFFER, name);
	_complete = table.glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	table.glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (!_complete) {
		return fail(core::getImageFormatName(views.front()->getInfo().format));
	}

	return core::Framebuffer::init(dev, GlesObject_clearFramebuffer, core::ObjectType::Framebuffer,
			glObjectHandle(_glFbo));
}

bool Semaphore::init(Device &dev) {
	return core::Semaphore::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Semaphore, core::ObjectHandle::zero());
}

bool Fence::init(Device &dev, core::FenceType type) {
	_type = type;

	return core::Object::init(dev, GlesObject_clearFence, core::ObjectType::Fence,
			core::ObjectHandle::zero(), this);
}

Status Fence::doCheckFence(bool lockfree) {
	if (_signaled.load()) {
		return Status::Ok;
	}

	auto device = static_cast<Device *>(_object.device);
	if (!_glSync || !device) {
		return Status::Suspended;
	}

	// The context is gone only when the device ends, and everything it queued has completed by
	// then - report success so the frame graph can retire instead of spinning on a dead driver.
	if (!device->isAlive()) {
		_signaled.store(true);
		return Status::Ok;
	}

	auto &table = device->getTable();
	if (table.glClientWaitSync(_glSync, 0, 0) == GL_ALREADY_SIGNALED) {
		_signaled.store(true);
		return Status::Ok;
	}

	return Status::Suspended;
}

void Fence::doResetFence() {
	auto device = static_cast<Device *>(_object.device);
	if (_glSync && device && device->isAlive()) {
		const GLsync sync = _glSync;
		device->scheduleRelease([t = &device->getTable(), sync]() { t->glDeleteSync(sync); });
	}

	_glSync = nullptr;
	_signaled.store(true);
}

} // namespace stappler::xenolith::gles
