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

#include "XLRemoteSerialize.h"
#include "XLCoreQueuePass.h" // core::QueuePass stub for the mirror
#include "XLCoreAttachment.h" // complete core::Attachment (AttachmentData holds Rc<Attachment>)
#include "XLCoreMaterial.h" // complete core::MaterialSet/Material/MaterialAttachment for the codec

#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using DataValue = data::ValueTemplate<memory::StandartInterface>;

static constexpr uint32_t kCodecVersion = 3;

// --- small helpers ---------------------------------------------------------

template <typename T>
static int64_t ei(T v) {
	return int64_t(sprt::toInt(v));
}

static DataValue bytesValue(BytesView v) { return DataValue(Bytes(v.data(), v.data() + v.size())); }

// indexed access into a flat-array node; bounds-checked, returns Null when out of range
static const DataValue &at(const DataValue &n, size_t i) { return n.getValue(i); }

// read a node-table reference index stored as a plain integer element. IMPORTANT: never use the
// single-argument DataValue::getInteger(def) here -- for a single integer argument the keyed
// getInteger(key, def) template overload is a better match and would treat the argument as an array
// index (returning the default) instead of returning the element's value. Read via the no-arg form
// and fall back to -1 (the "null ref" sentinel deref() expects) only when the element is absent.
static int64_t refAt(const DataValue &n, size_t i) {
	auto &v = at(n, i);
	return v.isInteger() ? v.getInteger() : -1;
}

static BytesView bytesAt(const DataValue &n, size_t i) {
	auto &v = at(n, i);
	if (v.isBytes()) {
		auto &b = v.getBytes();
		return BytesView(b.data(), b.size());
	}
	return BytesView();
}

// All POD nodes below are encoded as compact flat arrays with a fixed field order
// (no per-field keys -> minimal CBOR footprint), same style as serializeFrameConstraints.

static DataValue depToValue(const core::AttachmentDependencyInfo &d) {
	// [iStage, iAccess, fStage, fAccess, req, lock]
	DataValue v(DataValue::Type::ARRAY);
	v.addInteger(ei(d.initialUsageStage));
	v.addInteger(ei(d.initialAccessMask));
	v.addInteger(ei(d.finalUsageStage));
	v.addInteger(ei(d.finalAccessMask));
	v.addInteger(ei(d.requiredRenderPassState));
	v.addInteger(ei(d.lockedRenderPassState));
	return v;
}

static core::AttachmentDependencyInfo valueToDep(const DataValue &v) {
	core::AttachmentDependencyInfo d;
	d.initialUsageStage = core::PipelineStage(at(v, 0).getInteger());
	d.initialAccessMask = core::AccessType(at(v, 1).getInteger());
	d.finalUsageStage = core::PipelineStage(at(v, 2).getInteger());
	d.finalAccessMask = core::AccessType(at(v, 3).getInteger());
	d.requiredRenderPassState = core::FrameRenderPassState(at(v, 4).getInteger());
	d.lockedRenderPassState = core::FrameRenderPassState(at(v, 5).getInteger());
	return d;
}

static DataValue samplerToValue(const core::SamplerInfo &s) {
	// [magFilter, minFilter, mipmapMode, u, v, w, mipLodBias, aniso, maxAniso,
	//  cmpEnable, cmpOp, minLod, maxLod]
	DataValue v(DataValue::Type::ARRAY);
	v.addInteger(ei(s.magFilter));
	v.addInteger(ei(s.minFilter));
	v.addInteger(ei(s.mipmapMode));
	v.addInteger(ei(s.addressModeU));
	v.addInteger(ei(s.addressModeV));
	v.addInteger(ei(s.addressModeW));
	v.addDouble(s.mipLodBias);
	v.addBool(s.anisotropyEnable);
	v.addDouble(s.maxAnisotropy);
	v.addBool(s.compareEnable);
	v.addInteger(ei(s.compareOp));
	v.addDouble(s.minLod);
	v.addDouble(s.maxLod);
	return v;
}

static core::SamplerInfo valueToSampler(const DataValue &v) {
	core::SamplerInfo s;
	s.magFilter = core::Filter(at(v, 0).getInteger());
	s.minFilter = core::Filter(at(v, 1).getInteger());
	s.mipmapMode = core::SamplerMipmapMode(at(v, 2).getInteger());
	s.addressModeU = core::SamplerAddressMode(at(v, 3).getInteger());
	s.addressModeV = core::SamplerAddressMode(at(v, 4).getInteger());
	s.addressModeW = core::SamplerAddressMode(at(v, 5).getInteger());
	s.mipLodBias = float(at(v, 6).getDouble());
	s.anisotropyEnable = at(v, 7).getBool();
	s.maxAnisotropy = float(at(v, 8).getDouble());
	s.compareEnable = at(v, 9).getBool();
	s.compareOp = core::CompareOp(at(v, 10).getInteger());
	s.minLod = float(at(v, 11).getDouble());
	s.maxLod = float(at(v, 12).getDouble());
	return s;
}

static DataValue imageViewInfoToValue(const core::ImageViewInfo &i) {
	// [format, type, r, g, b, a, baseLayer, layerCount]
	DataValue v(DataValue::Type::ARRAY);
	v.addInteger(ei(i.format));
	v.addInteger(ei(i.type));
	v.addInteger(ei(i.r));
	v.addInteger(ei(i.g));
	v.addInteger(ei(i.b));
	v.addInteger(ei(i.a));
	v.addInteger(int64_t(i.baseArrayLayer.get()));
	v.addInteger(int64_t(i.layerCount.get()));
	return v;
}

static core::ImageViewInfo valueToImageViewInfo(const DataValue &v) {
	core::ImageViewInfo i;
	i.format = core::ImageFormat(at(v, 0).getInteger());
	i.type = core::ImageViewType(at(v, 1).getInteger());
	i.r = core::ComponentMapping(at(v, 2).getInteger());
	i.g = core::ComponentMapping(at(v, 3).getInteger());
	i.b = core::ComponentMapping(at(v, 4).getInteger());
	i.a = core::ComponentMapping(at(v, 5).getInteger());
	i.baseArrayLayer = core::BaseArrayLayer(uint32_t(at(v, 6).getInteger()));
	i.layerCount = core::ArrayLayers(uint32_t(at(v, 7).getInteger()));
	return i;
}

// --- Resource codec --------------------------------------------------------

static DataValue encodeResourceValue(const core::Resource &res, ObjectRegistry &reg) {
	// [name, buffers[], images[]]
	DataValue root(DataValue::Type::ARRAY);
	root.addString(res.getName());

	DataValue buffers(DataValue::Type::ARRAY);
	for (auto bd : res.getBuffers()) {
		// [key, flags, usage, type, size, persistent, access, data, id]
		DataValue b(DataValue::Type::ARRAY);
		b.addString(bd->key);
		b.addInteger(ei(bd->flags));
		b.addInteger(ei(bd->usage));
		b.addInteger(ei(bd->type));
		b.addInteger(int64_t(bd->size));
		b.addBool(bd->persistent);
		b.addInteger(ei(bd->targetAccess));
		// Raw buffer data is not required by client -> null placeholder
		// b.addValue(bytesValue(bd->data));
		b.addValue(DataValue());
		b.addInteger(int64_t(reg.share(bd->buffer.get())));
		buffers.addValue(sp::move(b));
	}
	root.addValue(sp::move(buffers));

	DataValue images(DataValue::Type::ARRAY);
	for (auto id : res.getImages()) {
		// [key, format, iflags, imageType, w, h, d, mips, layers, samples, tiling,
		//  usage, passType, hints, access, layout, data, id, views[]]
		DataValue im(DataValue::Type::ARRAY);
		im.addString(id->key);
		im.addInteger(ei(id->format));
		im.addInteger(ei(id->flags));
		im.addInteger(ei(id->imageType));
		im.addInteger(int64_t(id->extent.width));
		im.addInteger(int64_t(id->extent.height));
		im.addInteger(int64_t(id->extent.depth));
		im.addInteger(int64_t(id->mipLevels.get()));
		im.addInteger(int64_t(id->arrayLayers.get()));
		im.addInteger(ei(id->samples));
		im.addInteger(ei(id->tiling));
		im.addInteger(ei(id->usage));
		im.addInteger(ei(id->type));
		im.addInteger(ei(id->hints));
		im.addInteger(ei(id->targetAccess));
		im.addInteger(ei(id->targetLayout));
		// Raw image data is not required by client -> null placeholder
		// im.addValue(bytesValue(id->data));
		im.addValue(DataValue());
		im.addInteger(int64_t(reg.share(id->image.get())));

		DataValue views(DataValue::Type::ARRAY);
		for (auto vd : id->views) {
			// [format, type, r, g, b, a, baseLayer, layerCount, id]
			DataValue vv(DataValue::Type::ARRAY);
			vv.addInteger(ei(vd->format));
			vv.addInteger(ei(vd->type));
			vv.addInteger(ei(vd->r));
			vv.addInteger(ei(vd->g));
			vv.addInteger(ei(vd->b));
			vv.addInteger(ei(vd->a));
			vv.addInteger(int64_t(vd->baseArrayLayer.get()));
			vv.addInteger(int64_t(vd->layerCount.get()));
			vv.addInteger(int64_t(reg.share(vd->view.get())));
			views.addValue(sp::move(vv));
		}
		im.addValue(sp::move(views));
		images.addValue(sp::move(im));
	}
	root.addValue(sp::move(images));
	return root;
}

static Rc<core::Resource> decodeResourceValue(const DataValue &root, ObjectFactory &factory) {
	core::Resource::Builder builder(at(root, 0).getString());
	auto pool = builder.getPool();

	for (auto &b : at(root, 1).getArray()) {
		core::BufferInfo bi;
		bi.flags = core::BufferFlags(at(b, 1).getInteger());
		bi.usage = core::BufferUsage(at(b, 2).getInteger());
		bi.type = core::PassType(at(b, 3).getInteger());
		bi.size = uint64_t(at(b, 4).getInteger());
		bi.persistent = at(b, 5).getBool();
		auto access = core::AccessType(at(b, 6).getInteger());
		auto data = bytesAt(b, 7).pdup(pool);
		auto id = uint64_t(at(b, 8).getInteger());

		core::BufferInfo biCopy = bi;
		auto bd = builder.addBufferByRef(at(b, 0).getString(), sp::move(bi), data,
				Rc<core::DataAtlas>(), access);
		if (bd) {
			const_cast<core::BufferData *>(bd)->buffer = factory.makeBuffer(id, biCopy);
		}
	}

	for (auto &im : at(root, 2).getArray()) {
		core::ImageInfo ii;
		ii.format = core::ImageFormat(at(im, 1).getInteger());
		ii.flags = core::ImageFlags(at(im, 2).getInteger());
		ii.imageType = core::ImageType(at(im, 3).getInteger());
		ii.extent = Extent3(uint32_t(at(im, 4).getInteger()), uint32_t(at(im, 5).getInteger()),
				uint32_t(at(im, 6).getInteger()));
		ii.mipLevels = core::MipLevels(uint32_t(at(im, 7).getInteger()));
		ii.arrayLayers = core::ArrayLayers(uint32_t(at(im, 8).getInteger()));
		ii.samples = core::SampleCount(at(im, 9).getInteger());
		ii.tiling = core::ImageTiling(at(im, 10).getInteger());
		ii.usage = core::ImageUsage(at(im, 11).getInteger());
		ii.type = core::PassType(at(im, 12).getInteger());
		ii.hints = core::ImageHints(at(im, 13).getInteger());
		auto access = core::AccessType(at(im, 14).getInteger());
		auto layout = core::AttachmentLayout(at(im, 15).getInteger());
		auto data = bytesAt(im, 16).pdup(pool);
		auto id = uint64_t(at(im, 17).getInteger());

		core::ImageInfoData iiData = ii;
		auto imd = builder.addBitmapImageByRef(at(im, 0).getString(), sp::move(ii), data, layout,
				access);
		if (imd) {
			auto mutImd = const_cast<core::ImageData *>(imd);
			mutImd->image = factory.makeImage(id, iiData);
			// Let material images resolve their owning ImageData back from the gAPI object id.
			factory.registerImageData(id, imd);

			for (auto &vv : at(im, 18).getArray()) {
				core::ImageViewInfo vi;
				vi.format = core::ImageFormat(at(vv, 0).getInteger());
				vi.type = core::ImageViewType(at(vv, 1).getInteger());
				vi.r = core::ComponentMapping(at(vv, 2).getInteger());
				vi.g = core::ComponentMapping(at(vv, 3).getInteger());
				vi.b = core::ComponentMapping(at(vv, 4).getInteger());
				vi.a = core::ComponentMapping(at(vv, 5).getInteger());
				vi.baseArrayLayer = core::BaseArrayLayer(uint32_t(at(vv, 6).getInteger()));
				vi.layerCount = core::ArrayLayers(uint32_t(at(vv, 7).getInteger()));
				auto viCopy = vi;
				auto vd = builder.addImageView(imd, sp::move(vi));
				if (vd) {
					const_cast<core::ImageViewData *>(vd)->view = factory.makeImageView(
							uint64_t(at(vv, 8).getInteger()), mutImd->image, viCopy);
				}
			}
		}
	}

	auto res = Rc<core::Resource>::create(sp::move(builder));
	if (res) {
		res->setCompiled(true);
	}
	return res;
}

Bytes QueueCodec::encodeResource(const core::Resource &res, ObjectRegistry &registry) {
	return data::write(encodeResourceValue(res, registry), data::EncodeFormat::Cbor);
}

Rc<core::Resource> QueueCodec::decodeResource(BytesView bytes, ObjectFactory &factory) {
	auto root = data::read<memory::StandartInterface>(bytes);
	if (!root.isArray()) {
		return nullptr;
	}
	return decodeResourceValue(root, factory);
}

Value serializeFrameConstraints(const core::FrameConstraints &c) {
	// Compact flat array (no per-field keys -> minimal CBOR/JSON footprint). Fixed field order:
	// [w, h, d, padTop, padRight, padBottom, padLeft, transform, density, surfaceDensity, frameInterval]
	Value v(Value::Type::ARRAY);
	v.addInteger(int64_t(c.extent.width));
	v.addInteger(int64_t(c.extent.height));
	v.addInteger(int64_t(c.extent.depth));
	v.addDouble(c.contentPadding.top);
	v.addDouble(c.contentPadding.right);
	v.addDouble(c.contentPadding.bottom);
	v.addDouble(c.contentPadding.left);
	v.addInteger(ei(c.transform));
	v.addDouble(c.density);
	v.addDouble(c.surfaceDensity);
	v.addInteger(int64_t(c.frameInterval));
	return v;
}

core::FrameConstraints deserializeFrameConstraints(const Value &v) {
	core::FrameConstraints c;
	if (!v.isArray()) {
		return c; // malformed: keep defaults
	}
	auto &arr = v.getArray();
	auto at = [&](size_t i) -> const Value & {
		static const Value nil;
		return i < arr.size() ? arr[i] : nil;
	};
	c.extent.width = uint32_t(at(0).getInteger());
	c.extent.height = uint32_t(at(1).getInteger());
	c.extent.depth = uint32_t(at(2).getInteger());
	c.contentPadding.top = float(at(3).getDouble());
	c.contentPadding.right = float(at(4).getDouble());
	c.contentPadding.bottom = float(at(5).getDouble());
	c.contentPadding.left = float(at(6).getDouble());
	c.transform = sprt::window::SurfaceTransformFlags(at(7).getInteger());
	c.density = float(at(8).getDouble());
	c.surfaceDensity = float(at(9).getDouble());
	c.frameInterval = uint64_t(at(10).getInteger());
	return c;
}

// --- WindowInfo / SwapchainConfig codecs (same compact flat-array style) ----

static Value fullscreenToValue(const sprt::window::FullscreenInfo &f) {
	// [monitorName, edidVendorId, edidModel, edidSerial, modeW, modeH, modeRate, modeScale, flags]
	// EdidInfo::vendor is a derived lookup cache (from vendorId) -> not serialized.
	Value v(Value::Type::ARRAY);
	v.addString(StringView(f.id.name));
	v.addString(StringView(f.id.edid.vendorId));
	v.addString(StringView(f.id.edid.model));
	v.addString(StringView(f.id.edid.serial));
	v.addInteger(int64_t(f.mode.width));
	v.addInteger(int64_t(f.mode.height));
	v.addInteger(int64_t(f.mode.rate));
	v.addDouble(f.mode.scale);
	v.addInteger(ei(f.flags));
	return v;
}

static sprt::window::FullscreenInfo valueToFullscreen(const Value &v) {
	sprt::window::FullscreenInfo f;
	if (!v.isArray()) {
		return f;
	}
	auto &arr = v.getArray();
	auto at = [&](size_t i) -> const Value & {
		static const Value nil;
		return i < arr.size() ? arr[i] : nil;
	};
	f.id.name = StringView(at(0).getString()).str<sprt::window::String>();
	f.id.edid.vendorId = StringView(at(1).getString()).str<sprt::window::String>();
	f.id.edid.model = StringView(at(2).getString()).str<sprt::window::String>();
	f.id.edid.serial = StringView(at(3).getString()).str<sprt::window::String>();
	f.mode.width = uint32_t(at(4).getInteger());
	f.mode.height = uint32_t(at(5).getInteger());
	f.mode.rate = uint32_t(at(6).getInteger());
	f.mode.scale = float(at(7).getDouble());
	f.flags = sprt::window::FullscreenFlags(at(8).getInteger());
	return f;
}

Value serializeWindowInfo(const sprt::window::WindowInfo &c) {
	// Compact flat array (small CBOR/JSON footprint). Fixed field order:
	// [id, title, rectX, rectY, rectW, rectH, density, flags, fullscreen[], preferredPresentMode,
	//  imageFormat, colorSpace, capabilities, state, decoTop, decoRight, decoBottom, decoLeft]
	Value v(Value::Type::ARRAY);
	v.addString(StringView(c.id));
	v.addString(StringView(c.title));
	v.addInteger(int64_t(c.rect.x));
	v.addInteger(int64_t(c.rect.y));
	v.addInteger(int64_t(c.rect.width));
	v.addInteger(int64_t(c.rect.height));
	v.addDouble(c.density);
	v.addInteger(ei(c.flags));
	v.addValue(fullscreenToValue(c.fullscreen));
	v.addInteger(ei(c.preferredPresentMode));
	v.addInteger(ei(c.imageFormat));
	v.addInteger(ei(c.colorSpace));
	v.addInteger(ei(c.capabilities));
	v.addInteger(ei(c.state));
	v.addDouble(c.decorationInsets.top);
	v.addDouble(c.decorationInsets.right);
	v.addDouble(c.decorationInsets.bottom);
	v.addDouble(c.decorationInsets.left);
	return v;
}

Rc<sprt::window::WindowInfo> deserializeWindowInfo(const Value &v) {
	auto info = Rc<sprt::window::WindowInfo>::alloc();
	if (!v.isArray()) {
		return info; // malformed: default-constructed WindowInfo
	}
	auto &arr = v.getArray();
	auto at = [&](size_t i) -> const Value & {
		static const Value nil;
		return i < arr.size() ? arr[i] : nil;
	};
	info->id = StringView(at(0).getString()).str<sprt::window::String>();
	info->title = StringView(at(1).getString()).str<sprt::window::String>();
	info->rect.x = int32_t(at(2).getInteger());
	info->rect.y = int32_t(at(3).getInteger());
	info->rect.width = uint32_t(at(4).getInteger());
	info->rect.height = uint32_t(at(5).getInteger());
	info->density = float(at(6).getDouble());
	info->flags = sprt::window::WindowCreationFlags(at(7).getInteger());
	info->fullscreen = valueToFullscreen(at(8));
	info->preferredPresentMode = sprt::window::PresentMode(at(9).getInteger());
	info->imageFormat = sprt::window::ImageFormat(at(10).getInteger());
	info->colorSpace = sprt::window::ColorSpace(at(11).getInteger());
	info->capabilities = sprt::window::WindowCapabilities(uint64_t(at(12).getInteger()));
	info->state = sprt::window::WindowState(uint64_t(at(13).getInteger()));
	info->decorationInsets.top = float(at(14).getDouble());
	info->decorationInsets.right = float(at(15).getDouble());
	info->decorationInsets.bottom = float(at(16).getDouble());
	info->decorationInsets.left = float(at(17).getDouble());
	return info;
}

Value serializeSwapchainConfig(const core::SwapchainConfig &c) {
	// Compact flat array. Fixed field order:
	// [presentMode, presentModeFast, imageFormat, colorSpace, alpha, transform, imageCount,
	//  extentW, extentH, clipped, transfer, liveResize, fullscreenMode]
	// fullscreenHandle is a raw void* (server-local) -> not serialized.
	Value v(Value::Type::ARRAY);
	v.addInteger(ei(c.presentMode));
	v.addInteger(ei(c.presentModeFast));
	v.addInteger(ei(c.imageFormat));
	v.addInteger(ei(c.colorSpace));
	v.addInteger(ei(c.alpha));
	v.addInteger(ei(c.transform));
	v.addInteger(int64_t(c.imageCount));
	v.addInteger(int64_t(c.extent.width));
	v.addInteger(int64_t(c.extent.height));
	v.addBool(c.clipped);
	v.addBool(c.transfer);
	v.addBool(c.liveResize);
	v.addInteger(ei(c.fullscreenMode));
	return v;
}

core::SwapchainConfig deserializeSwapchainConfig(const Value &v) {
	core::SwapchainConfig c;
	if (!v.isArray()) {
		return c; // malformed: keep defaults
	}
	auto &arr = v.getArray();
	auto at = [&](size_t i) -> const Value & {
		static const Value nil;
		return i < arr.size() ? arr[i] : nil;
	};
	c.presentMode = sprt::window::PresentMode(at(0).getInteger());
	c.presentModeFast = sprt::window::PresentMode(at(1).getInteger());
	c.imageFormat = sprt::window::ImageFormat(at(2).getInteger());
	c.colorSpace = sprt::window::ColorSpace(at(3).getInteger());
	c.alpha = sprt::window::CompositeAlphaFlags(at(4).getInteger());
	c.transform = sprt::window::SurfaceTransformFlags(at(5).getInteger());
	c.imageCount = uint32_t(at(6).getInteger());
	c.extent.width = uint32_t(at(7).getInteger());
	c.extent.height = uint32_t(at(8).getInteger());
	c.clipped = at(9).getBool();
	c.transfer = at(10).getBool();
	c.liveResize = at(11).getBool();
	c.fullscreenMode = sprt::window::FullScreenExclusiveMode(at(12).getInteger());
	return c;
}

// --- CompileMaterials MaterialImage codec ----------------------------------
//
// Distinct from the MaterialSet codec's encodeMaterialImage above: there the image is a server-minted
// registry id (server -> client). Here the client forwards a runtime material to the server, so the
// image is keyed by its stable wire index and the server resolves the real image (font atlas / static
// resource) itself; only the descriptor binding + view info are carried.

Value serializeMaterialImage(const core::MaterialImage &mi) {
	Value v;
	v.setInteger(int64_t(mi.image && mi.image->image ? mi.image->image->getIndex() : 0), "i");
	v.setInteger(int64_t(mi.sampler), "s");
	v.setInteger(int64_t(mi.set), "set");
	v.setInteger(int64_t(mi.descriptor), "d");
	v.setValue(imageViewInfoToValue(mi.info), "vi");
	return v;
}

core::MaterialImage deserializeMaterialImage(const Value &v, uint64_t &outImageId) {
	outImageId = uint64_t(v.getInteger("i"));
	core::MaterialImage mi;
	mi.sampler = uint16_t(v.getInteger("s"));
	mi.set = uint32_t(v.getInteger("set"));
	mi.descriptor = uint32_t(v.getInteger("d"));
	mi.info = valueToImageViewInfo(v.getValue("vi"));
	return mi;
}

// --- Queue codec: node tables ----------------------------------------------

template <typename T>
struct NodeTable {
	Vector<const T *> items;
	Map<const void *, uint32_t> index;

	// register, returns true if newly inserted
	bool add(const T *p) {
		if (!p) {
			return false;
		}
		if (index.find((const void *)p) != index.end()) {
			return false;
		}
		index.emplace((const void *)p, uint32_t(items.size()));
		items.emplace_back(p);
		return true;
	}
	int64_t ref(const T *p) const {
		if (!p) {
			return -1;
		}
		auto it = index.find((const void *)p);
		return (it != index.end()) ? int64_t(it->second) : -1;
	}
	size_t size() const { return items.size(); }
};

namespace {

using namespace core;

struct QueueEncoder {
	ObjectRegistry &reg;

	NodeTable<ProgramData> programs;
	NodeTable<GraphicPipelineData> graphicPipelines;
	NodeTable<ComputePipelineData> computePipelines;
	NodeTable<PipelineFamilyData> pipelineFamilies;
	NodeTable<TextureSetLayoutData> textureSets;
	NodeTable<AttachmentData> attachments;
	NodeTable<AttachmentPassData> attachmentPasses;
	NodeTable<AttachmentSubpassData> attachmentSubpasses;
	NodeTable<SubpassData> subpasses;
	NodeTable<PipelineLayoutData> pipelineLayouts;
	NodeTable<DescriptorSetData> descriptorSets;
	NodeTable<PipelineDescriptor> descriptors;
	NodeTable<QueuePassData> passes;

	QueueEncoder(ObjectRegistry &r) : reg(r) { }

	// --- collection (closure over the whole graph) ---
	void collectProgram(const ProgramData *p) { programs.add(p); }

	void collectTextureSetLayout(const TextureSetLayoutData *t) {
		if (textureSets.add(t)) {
			for (auto l : t->bindingLayouts) { collectPipelineLayout(l); }
		}
	}

	void collectDescriptor(const PipelineDescriptor *d) {
		if (descriptors.add(d)) {
			collectAttachmentPass(d->attachment);
			collectDescriptorSet(d->set);
		}
	}

	void collectDescriptorSet(const DescriptorSetData *s) {
		if (descriptorSets.add(s)) {
			collectPipelineLayout(s->layout);
			for (auto d : s->descriptors) { collectDescriptor(d); }
		}
	}

	void collectPipelineFamily(const PipelineFamilyData *f) {
		// The family's pipelines and its owning layout are collected through the layout itself; the
		// family node only needs an entry in the table so layouts/pipelines can reference it.
		if (f) {
			pipelineFamilies.add(f);
		}
	}

	void collectPipelineLayout(const PipelineLayoutData *l) {
		if (pipelineLayouts.add(l)) {
			collectPass(l->pass);
			collectTextureSetLayout(l->textureSetLayout);
			for (auto s : l->sets) { collectDescriptorSet(s); }
			for (auto p : l->graphicPipelines) { collectGraphicPipeline(p); }
			for (auto p : l->computePipelines) { collectComputePipeline(p); }
			collectPipelineFamily(l->defaultFamily);
			for (auto f : l->families) { collectPipelineFamily(f); }
		}
	}

	void collectGraphicPipeline(const GraphicPipelineData *p) {
		if (graphicPipelines.add(p)) {
			collectSubpass(p->subpass);
			collectPipelineLayout(p->layout);
			for (auto &s : p->shaders) { collectProgram(s.data); }
		}
	}

	void collectComputePipeline(const ComputePipelineData *p) {
		if (computePipelines.add(p)) {
			collectSubpass(p->subpass);
			collectPipelineLayout(p->layout);
			collectProgram(p->shader.data);
		}
	}

	void collectAttachmentSubpass(const AttachmentSubpassData *a) {
		if (attachmentSubpasses.add(a)) {
			collectAttachmentPass(a->pass);
			collectSubpass(a->subpass);
		}
	}

	void collectSubpass(const SubpassData *s) {
		if (subpasses.add(s)) {
			collectPass(s->pass);
			for (auto p : s->graphicPipelines) { collectGraphicPipeline(p); }
			for (auto p : s->computePipelines) { collectComputePipeline(p); }
			for (auto a : s->inputImages) { collectAttachmentSubpass(a); }
			for (auto a : s->outputImages) { collectAttachmentSubpass(a); }
			for (auto a : s->resolveImages) { collectAttachmentSubpass(a); }
			collectAttachmentSubpass(s->depthStencil);
		}
	}

	void collectAttachmentPass(const AttachmentPassData *a) {
		if (attachmentPasses.add(a)) {
			collectAttachment(a->attachment);
			collectPass(a->pass);
			for (auto d : a->descriptors) { collectDescriptor(d); }
			for (auto s : a->subpasses) { collectAttachmentSubpass(s); }
		}
	}

	void collectAttachment(const AttachmentData *a) {
		if (attachments.add(a)) {
			for (auto p : a->passes) { collectAttachmentPass(p); }
		}
	}

	void collectPass(const QueuePassData *p) {
		if (passes.add(p)) {
			for (auto a : p->attachments) { collectAttachmentPass(a); }
			for (auto s : p->subpasses) { collectSubpass(s); }
			for (auto l : p->pipelineLayouts) { collectPipelineLayout(l); }
		}
	}

	// --- emit ---
	template <typename Table>
	static DataValue refArray(const Table &tbl, const auto &vec) {
		DataValue arr(DataValue::Type::ARRAY);
		for (auto p : vec) { arr.addInteger(tbl.ref(p)); }
		return arr;
	}

	// Every node is a compact flat array with a fixed field order; the order is documented
	// in a comment at the head of each emitter and mirrored by the decode loops below.

	DataValue emitProgram(const ProgramData *p) {
		// [key, stage, data, id]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(p->key);
		v.addInteger(ei(p->stage));
		// Raw shader data is not required by client -> null placeholder
		// v.addValue(bytesValue(BytesView(reinterpret_cast<const uint8_t *>(p->data.data()),
		//		p->data.size() * sizeof(uint32_t))));
		v.addValue(DataValue());
		v.addInteger(int64_t(reg.share(p->program.get())));
		return v;
	}

	DataValue emitGraphicPipeline(const GraphicPipelineData *p) {
		// [key, dynamicState, material, subpass, layout, shaders[], id, family]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(p->key);
		v.addInteger(ei(p->dynamicState));
		v.addValue(bytesValue(BytesView(reinterpret_cast<const uint8_t *>(&p->material),
				sizeof(PipelineMaterialInfo))));
		v.addInteger(subpasses.ref(p->subpass));
		v.addInteger(pipelineLayouts.ref(p->layout));
		DataValue shaders(DataValue::Type::ARRAY);
		for (auto &s : p->shaders) { shaders.addInteger(programs.ref(s.data)); }
		v.addValue(sp::move(shaders));
		v.addInteger(int64_t(reg.share(p->pipeline.get())));
		v.addInteger(pipelineFamilies.ref(static_cast<const PipelineFamilyData *>(p->family)));
		return v;
	}

	DataValue emitComputePipeline(const ComputePipelineData *p) {
		// [key, subpass, layout, shader, id, lx, ly, lz, family]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(p->key);
		v.addInteger(subpasses.ref(p->subpass));
		v.addInteger(pipelineLayouts.ref(p->layout));
		v.addInteger(programs.ref(p->shader.data));
		v.addInteger(int64_t(reg.share(p->pipeline.get())));
		uint32_t lx = 0, ly = 0, lz = 0;
		if (p->pipeline) {
			lx = p->pipeline->getLocalX();
			ly = p->pipeline->getLocalY();
			lz = p->pipeline->getLocalZ();
		}
		v.addInteger(int64_t(lx));
		v.addInteger(int64_t(ly));
		v.addInteger(int64_t(lz));
		v.addInteger(pipelineFamilies.ref(static_cast<const PipelineFamilyData *>(p->family)));
		return v;
	}

	DataValue emitPipelineFamily(const PipelineFamilyData *f) {
		// [key, layout, graphicPipelines[], computePipelines[]]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(f->key);
		v.addInteger(pipelineLayouts.ref(f->layout));
		v.addValue(refArray(graphicPipelines, f->graphicPipelines));
		v.addValue(refArray(computePipelines, f->computePipelines));
		return v;
	}

	DataValue emitTextureSetLayout(const TextureSetLayoutData *t) {
		// [key, imageCount, imageCountIndexed, bufferCount, bufferCountIndexed, samplers[], id,
		//  bindingLayouts[]]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(t->key);
		v.addInteger(int64_t(t->imageCount));
		v.addInteger(int64_t(t->imageCountIndexed));
		v.addInteger(int64_t(t->bufferCount));
		v.addInteger(int64_t(t->bufferCountIndexed));
		DataValue samplers(DataValue::Type::ARRAY);
		for (auto &s : t->samplers) { samplers.addValue(samplerToValue(s)); }
		v.addValue(sp::move(samplers));
		v.addInteger(int64_t(reg.share(t->layout.get())));
		v.addValue(refArray(pipelineLayouts, t->bindingLayouts));
		return v;
	}

	DataValue emitAttachment(const AttachmentData *a) {
		// [key, aid, ops, type, usage, outputState, transient, passes[]]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(a->key);
		v.addInteger(int64_t(a->id));
		v.addInteger(ei(a->ops));
		v.addInteger(ei(a->type));
		v.addInteger(ei(a->usage));
		v.addInteger(ei(a->outputState));
		v.addBool(a->transient);
		v.addValue(refArray(attachmentPasses, a->passes));
		return v;
	}

	DataValue emitAttachmentPass(const AttachmentPassData *a) {
		// [key, attachment, pass, index, ops, initialLayout, finalLayout, loadOp, storeOp,
		//  stencilLoadOp, stencilStoreOp, colorMode, dependency, descriptors[], subpasses[]]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(a->key);
		v.addInteger(attachments.ref(a->attachment));
		v.addInteger(passes.ref(a->pass));
		v.addInteger(int64_t(a->index));
		v.addInteger(ei(a->ops));
		v.addInteger(ei(a->initialLayout));
		v.addInteger(ei(a->finalLayout));
		v.addInteger(ei(a->loadOp));
		v.addInteger(ei(a->storeOp));
		v.addInteger(ei(a->stencilLoadOp));
		v.addInteger(ei(a->stencilStoreOp));
		v.addInteger(int64_t(a->colorMode.toInt()));
		v.addValue(depToValue(a->dependency));
		v.addValue(refArray(descriptors, a->descriptors));
		v.addValue(refArray(attachmentSubpasses, a->subpasses));
		return v;
	}

	DataValue emitAttachmentSubpass(const AttachmentSubpassData *a) {
		// [key, pass, subpass, layout, usage, ops, dependency, blend]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(a->key);
		v.addInteger(attachmentPasses.ref(a->pass));
		v.addInteger(subpasses.ref(a->subpass));
		v.addInteger(ei(a->layout));
		v.addInteger(ei(a->usage));
		v.addInteger(ei(a->ops));
		v.addValue(depToValue(a->dependency));
		v.addInteger(int64_t(reinterpret_cast<const uint32_t &>(a->blendInfo)));
		return v;
	}

	DataValue emitSubpass(const SubpassData *s) {
		// [key, pass, index, graphicPipelines[], computePipelines[], inputImages[],
		//  outputImages[], resolveImages[], depthStencil, preserve[]]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(s->key);
		v.addInteger(passes.ref(s->pass));
		v.addInteger(int64_t(s->index));
		v.addValue(refArray(graphicPipelines, s->graphicPipelines));
		v.addValue(refArray(computePipelines, s->computePipelines));
		v.addValue(refArray(attachmentSubpasses, s->inputImages));
		v.addValue(refArray(attachmentSubpasses, s->outputImages));
		v.addValue(refArray(attachmentSubpasses, s->resolveImages));
		v.addInteger(attachmentSubpasses.ref(s->depthStencil));
		DataValue preserve(DataValue::Type::ARRAY);
		for (auto p : s->preserve) { preserve.addInteger(int64_t(p)); }
		v.addValue(sp::move(preserve));
		return v;
	}

	DataValue emitPipelineLayout(const PipelineLayoutData *l) {
		// [key, pass, index, textureSetLayout, sets[], graphicPipelines[], computePipelines[],
		//  defaultFamily, families[]]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(l->key);
		v.addInteger(passes.ref(l->pass));
		v.addInteger(int64_t(l->index));
		v.addInteger(textureSets.ref(l->textureSetLayout));
		v.addValue(refArray(descriptorSets, l->sets));
		v.addValue(refArray(graphicPipelines, l->graphicPipelines));
		v.addValue(refArray(computePipelines, l->computePipelines));
		v.addInteger(pipelineFamilies.ref(l->defaultFamily));
		v.addValue(refArray(pipelineFamilies, l->families));
		return v;
	}

	DataValue emitDescriptorSet(const DescriptorSetData *s) {
		// [key, layout, index, descriptors[]]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(s->key);
		v.addInteger(pipelineLayouts.ref(s->layout));
		v.addInteger(int64_t(s->index));
		v.addValue(refArray(descriptors, s->descriptors));
		return v;
	}

	DataValue emitDescriptor(const PipelineDescriptor *d) {
		// [key, set, attachment, type, stages, layout, count, index, requestFlags, deviceFlags]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(d->key);
		v.addInteger(descriptorSets.ref(d->set));
		v.addInteger(attachmentPasses.ref(d->attachment));
		v.addInteger(ei(d->type));
		v.addInteger(ei(d->stages));
		v.addInteger(ei(d->layout));
		v.addInteger(int64_t(d->count));
		v.addInteger(int64_t(d->index));
		v.addInteger(ei(d->requestFlags));
		v.addInteger(ei(d->deviceFlags));
		return v;
	}

	DataValue emitPass(const QueuePassData *p) {
		// [key, type, ordering, hasUpdateAfterBind, acquireTimestamps, attachments[],
		//  subpasses[], pipelineLayouts[], dependencies[], id, implIndex]
		DataValue v(DataValue::Type::ARRAY);
		v.addString(p->key);
		v.addInteger(ei(p->type));
		v.addInteger(int64_t(p->ordering.get()));
		v.addBool(p->hasUpdateAfterBind);
		v.addInteger(int64_t(p->acquireTimestamps));
		v.addValue(refArray(attachmentPasses, p->attachments));
		v.addValue(refArray(subpasses, p->subpasses));
		v.addValue(refArray(pipelineLayouts, p->pipelineLayouts));
		DataValue deps(DataValue::Type::ARRAY);
		for (auto &d : p->dependencies) {
			// [srcSubpass, srcStage, srcAccess, dstSubpass, dstStage, dstAccess, byRegion]
			DataValue dv(DataValue::Type::ARRAY);
			dv.addInteger(int64_t(d.srcSubpass));
			dv.addInteger(ei(d.srcStage));
			dv.addInteger(ei(d.srcAccess));
			dv.addInteger(int64_t(d.dstSubpass));
			dv.addInteger(ei(d.dstStage));
			dv.addInteger(ei(d.dstAccess));
			dv.addBool(d.byRegion);
			deps.addValue(sp::move(dv));
		}
		v.addValue(sp::move(deps));
		v.addInteger(int64_t(reg.share(p->impl.get())));
		v.addInteger(int64_t(p->impl ? p->impl->getIndex() : 0));
		return v;
	}

	template <typename Table, typename Emit>
	static DataValue emitTable(const Table &tbl, Emit &&emit) {
		DataValue arr(DataValue::Type::ARRAY);
		for (auto p : tbl.items) { arr.addValue(emit(p)); }
		return arr;
	}
};

} // namespace

// --- Material codec --------------------------------------------------------
//
// Encoded from the `materials` argument only (the live MaterialSet snapshots the server chose to
// share), never re-derived from the queue graph. Each material set references its owning attachment
// and its target texture-set layout by node-table index; gAPI objects (image/view/buffer) become
// server ids via the registry, resolved back to thin handles on decode. NOT mirrored: the per-image
// `dynamic` instance, the material atlas/owned-data and opaque user `_data`, and the per-layout
// gAPI TextureSet (the client mirror has no texture-set handle).

static DataValue encodeMaterialImage(const core::MaterialImage &mi, ObjectRegistry &reg) {
	// [imageId, viewId, sampler, set, descriptor, info]
	DataValue v(DataValue::Type::ARRAY);
	v.addInteger(int64_t(reg.share(mi.image ? mi.image->image.get() : nullptr)));
	v.addInteger(int64_t(reg.share(mi.view.get())));
	v.addInteger(int64_t(mi.sampler));
	v.addInteger(int64_t(mi.set));
	v.addInteger(int64_t(mi.descriptor));
	v.addValue(imageViewInfoToValue(mi.info));
	return v;
}

static DataValue encodeMaterial(const core::Material *m, ObjectRegistry &reg) {
	// [id, layoutIndex, pipelineKey, bufferId, images[]]
	// Pipelines are referenced by key (not node index) so the same material encoding works both
	// embedded in a queue blob and standalone in a material update against an existing mirror.
	DataValue v(DataValue::Type::ARRAY);
	v.addInteger(int64_t(m->getId()));
	v.addInteger(int64_t(m->getLayoutIndex()));
	v.addString(m->getPipeline() ? StringView(m->getPipeline()->key) : StringView());
	v.addInteger(int64_t(reg.share(m->getBuffer())));
	DataValue images(DataValue::Type::ARRAY);
	for (auto &mi : m->getImages()) { images.addValue(encodeMaterialImage(mi, reg)); }
	v.addValue(sp::move(images));
	return v;
}

static DataValue encodeMaterialLayout(const core::MaterialLayout &l, ObjectRegistry &reg) {
	// [usedImageSlots, slots[]]; slot = [viewId, refCount]
	DataValue v(DataValue::Type::ARRAY);
	v.addInteger(int64_t(l.usedImageSlots));
	DataValue slots(DataValue::Type::ARRAY);
	for (auto &s : l.imageSlots) {
		DataValue sv(DataValue::Type::ARRAY);
		sv.addInteger(int64_t(reg.share(s.image.get())));
		sv.addInteger(int64_t(s.refCount));
		slots.addValue(sp::move(sv));
	}
	v.addValue(sp::move(slots));
	return v;
}

static DataValue encodeMaterialSet(const core::MaterialAttachment *att, core::MaterialSet *set,
		const QueueEncoder &enc, ObjectRegistry &reg) {
	// [owner, targetLayout, imagesInSet, generation, materials[], layouts[], updated[], predefined[]]
	DataValue v(DataValue::Type::ARRAY);
	v.addInteger(enc.attachments.ref(att->getData()));
	v.addInteger(enc.textureSets.ref(att->getTargetLayout()));
	v.addInteger(int64_t(set->getImagesInSet()));
	v.addInteger(int64_t(set->getGeneration()));

	DataValue mats(DataValue::Type::ARRAY);
	for (auto &it : set->getMaterials()) { mats.addValue(encodeMaterial(it.second.get(), reg)); }
	v.addValue(sp::move(mats));

	DataValue layouts(DataValue::Type::ARRAY);
	for (auto &l : set->getLayouts()) { layouts.addValue(encodeMaterialLayout(l, reg)); }
	v.addValue(sp::move(layouts));

	DataValue updated(DataValue::Type::ARRAY);
	set->foreachUpdated([&](core::MaterialId id, NotNull<core::Material>) {
		updated.addInteger(int64_t(id));
	}, false);
	v.addValue(sp::move(updated));

	// The attachment's predefined materials (e.g. SolidImage/EmptyImage) are referenced by id; they
	// also live in `materials[]` above. The mirror needs the predefined LIST (not just the set) so
	// FrameContext::readMaterials can register them for lookup -- otherwise sprites that resolve a
	// predefined texture (SolidImage) find no material and render nothing.
	DataValue predefined(DataValue::Type::ARRAY);
	for (auto &m : att->getPredefinedMaterials()) { predefined.addInteger(int64_t(m->getId())); }
	v.addValue(sp::move(predefined));
	return v;
}

Bytes QueueCodec::encodeQueue(const core::Queue &queue,
		const HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> &materials,
		ObjectRegistry &registry) {
	QueueEncoder enc(registry);

	// reach into the queue via the public getters (encode is read-only)
	for (auto a : queue.getAttachments()) { enc.collectAttachment(a); }
	for (auto p : queue.getPasses()) { enc.collectPass(p); }
	for (auto p : queue.getPrograms()) { enc.collectProgram(p); }
	for (auto p : queue.getGraphicPipelines()) { enc.collectGraphicPipeline(p); }
	for (auto p : queue.getComputePipelines()) { enc.collectComputePipeline(p); }
	for (auto t : queue.getTextureSetLayouts()) { enc.collectTextureSetLayout(t); }
	for (auto a : queue.getInputAttachments()) { enc.collectAttachment(a); }
	for (auto a : queue.getOutputAttachments()) { enc.collectAttachment(a); }

	DataValue root(DataValue::Type::DICTIONARY);
	root.setInteger(int64_t(kCodecVersion), "v");
	root.setString(queue.getName(), "name");
	root.setInteger(ei(queue.getDefaultSyncPassState()), "syncState");

	root.setValue(QueueEncoder::emitTable(enc.programs,
						  [&](const ProgramData *p) { return enc.emitProgram(p); }),
			"programs");
	root.setValue(QueueEncoder::emitTable(enc.graphicPipelines,
						  [&](const GraphicPipelineData *p) { return enc.emitGraphicPipeline(p); }),
			"graphicPipelines");
	root.setValue(QueueEncoder::emitTable(enc.computePipelines,
						  [&](const ComputePipelineData *p) { return enc.emitComputePipeline(p); }),
			"computePipelines");
	root.setValue(QueueEncoder::emitTable(enc.pipelineFamilies,
						  [&](const PipelineFamilyData *f) { return enc.emitPipelineFamily(f); }),
			"pipelineFamilies");
	root.setValue(
			QueueEncoder::emitTable(enc.textureSets,
					[&](const TextureSetLayoutData *t) { return enc.emitTextureSetLayout(t); }),
			"textureSets");
	root.setValue(QueueEncoder::emitTable(enc.attachments,
						  [&](const AttachmentData *a) { return enc.emitAttachment(a); }),
			"attachments");
	root.setValue(QueueEncoder::emitTable(enc.attachmentPasses,
						  [&](const AttachmentPassData *a) { return enc.emitAttachmentPass(a); }),
			"attachmentPasses");
	root.setValue(
			QueueEncoder::emitTable(enc.attachmentSubpasses,
					[&](const AttachmentSubpassData *a) { return enc.emitAttachmentSubpass(a); }),
			"attachmentSubpasses");
	root.setValue(QueueEncoder::emitTable(enc.subpasses,
						  [&](const SubpassData *s) { return enc.emitSubpass(s); }),
			"subpasses");
	root.setValue(QueueEncoder::emitTable(enc.pipelineLayouts,
						  [&](const PipelineLayoutData *l) { return enc.emitPipelineLayout(l); }),
			"pipelineLayouts");
	root.setValue(QueueEncoder::emitTable(enc.descriptorSets,
						  [&](const DescriptorSetData *s) { return enc.emitDescriptorSet(s); }),
			"descriptorSets");
	root.setValue(QueueEncoder::emitTable(enc.descriptors,
						  [&](const PipelineDescriptor *d) { return enc.emitDescriptor(d); }),
			"descriptors");
	root.setValue(QueueEncoder::emitTable(enc.passes,
						  [&](const QueuePassData *p) { return enc.emitPass(p); }),
			"passes");

	DataValue input(DataValue::Type::ARRAY);
	for (auto a : queue.getInputAttachments()) { input.addInteger(enc.attachments.ref(a)); }
	root.setValue(sp::move(input), "input");

	DataValue output(DataValue::Type::ARRAY);
	for (auto a : queue.getOutputAttachments()) { output.addInteger(enc.attachments.ref(a)); }
	root.setValue(sp::move(output), "output");

	// resources
	if (auto res = queue.getInternalResource()) {
		root.setValue(encodeResourceValue(*res, registry), "internalResource");
	}
	DataValue linked(DataValue::Type::ARRAY);
	for (auto &it : queue.getLinkedResources()) {
		linked.addValue(encodeResourceValue(*it, registry));
	}
	root.setValue(sp::move(linked), "linkedResources");

	// QueueData also caches pointers to a few built-in resource entries by name
	if (auto res = queue.getInternalResource()) {
		// names are looked back up after the internal resource is rebuilt on decode
		for (auto img : res->getImages()) {
			if (img == res->getImage(core::EmptyTextureName)) {
				root.setString(img->key, "emptyImage");
			}
			if (img == res->getImage(core::SolidTextureName)) {
				root.setString(img->key, "solidImage");
			}
		}
		for (auto buf : res->getBuffers()) {
			if (buf == res->getBuffer(core::EmptyBufferName)) {
				root.setString(buf->key, "emptyBuffer");
			}
		}
	}

	// materials (from the argument only; one entry per shared MaterialSet)
	DataValue materialsArr(DataValue::Type::ARRAY);
	for (auto &it : materials) {
		if (it.second) {
			materialsArr.addValue(encodeMaterialSet(it.first, it.second.get(), enc, registry));
		}
	}
	root.setValue(sp::move(materialsArr), "materials");

	return data::write(root, data::EncodeFormat::Cbor);
}

// --- Queue decode ----------------------------------------------------------

namespace {

using namespace core;

// deref a ref-index into a freshly-allocated node table
template <typename T>
static T *deref(const Vector<T *> &tbl, int64_t idx) {
	return (idx >= 0 && size_t(idx) < tbl.size()) ? tbl[idx] : nullptr;
}

template <typename T, typename Out, typename Fn>
static void derefArray(const DataValue &node, size_t idx, const Vector<T *> &tbl, Out &out,
		Fn &&push) {
	for (auto &e : node.getValue(idx).getArray()) {
		if (auto p = deref(tbl, e.getInteger())) {
			push(out, p);
		}
	}
}

// --- shared material rebuild (gAPI objects resolve by id; all fields below are public) ---

static MaterialImage decodeMaterialImage(const DataValue &in, ObjectFactory &factory) {
	// [imageId, viewId, sampler, set, descriptor, info]; the view is a thin id-handle. The owning
	// ImageData is resolved from the resource decode (registered by gAPI object id) so material info
	// (`it.image->image->getIndex()`) works for predefined materials -- it stays null only if the
	// referenced image is not part of any decoded resource.
	auto imageId = uint64_t(at(in, 0).getInteger());
	MaterialImage mi;
	mi.image = factory.resolveImageData(imageId);
	mi.sampler = uint16_t(at(in, 2).getInteger());
	mi.set = uint32_t(at(in, 3).getInteger());
	mi.descriptor = uint32_t(at(in, 4).getInteger());
	mi.info = valueToImageViewInfo(at(in, 5));
	auto imgObj = static_cast<ImageObject *>(factory.resolveObject(imageId));
	mi.view = factory.makeImageView(uint64_t(at(in, 1).getInteger()), Rc<ImageObject>(imgObj),
			mi.info);
	return mi;
}

static MaterialLayout decodeMaterialLayout(const DataValue &ln, ObjectFactory &factory) {
	// [usedImageSlots, slots[]]; slot = [viewId, refCount]; slot views resolve from the factory
	// cache populated while decoding this set's material images. The gAPI TextureSet stays null.
	MaterialLayout lay;
	lay.usedImageSlots = uint32_t(at(ln, 0).getInteger());
	for (auto &sn : at(ln, 1).getArray()) {
		MaterialImageSlot slot;
		slot.image = Rc<ImageView>(
				static_cast<ImageView *>(factory.resolveObject(uint64_t(at(sn, 0).getInteger()))));
		slot.refCount = uint32_t(at(sn, 1).getInteger());
		lay.imageSlots.emplace_back(sp::move(slot));
	}
	return lay;
}

} // namespace

bool QueueCodec::decodeQueue(core::Queue &queue, BytesView bytes, ObjectFactory &factory) {
	auto root = data::read<memory::StandartInterface>(bytes);
	if (!root.isDictionary() || root.getInteger("v") != int64_t(kCodecVersion)) {
		return false;
	}

	auto data = queue._data;
	auto pool = data->pool;

	// Hoisted out of the perform block so the material pass (after resources) can resolve owner
	// attachments and target texture-set layouts by node-table index (material pipelines resolve by
	// key against the queue itself).
	Vector<TextureSetLayoutData *> textureSets;
	Vector<AttachmentData *> attachments;

	memory::perform([&] {
		data->key = StringView(root.getString("name")).pdup(pool);
		data->compiled = true;
		data->defaultSyncPassState = core::FrameRenderPassState(root.getInteger("syncState"));

		// 1) allocate every node empty; build index -> ptr tables
		auto alloc = [&](const char *key, auto &tbl, auto makeNode) {
			auto &arr = root.getValue(key).getArray();
			tbl.reserve(arr.size());
			for (size_t i = 0; i < arr.size(); ++i) { tbl.emplace_back(makeNode()); }
		};

		Vector<ProgramData *> programs;
		Vector<GraphicPipelineData *> graphicPipelines;
		Vector<ComputePipelineData *> computePipelines;
		Vector<PipelineFamilyData *> pipelineFamilies;
		Vector<AttachmentPassData *> attachmentPasses;
		Vector<AttachmentSubpassData *> attachmentSubpasses;
		Vector<SubpassData *> subpasses;
		Vector<PipelineLayoutData *> pipelineLayouts;
		Vector<DescriptorSetData *> descriptorSets;
		Vector<PipelineDescriptor *> descriptors;
		Vector<QueuePassData *> passes;

		alloc("programs", programs, [&] { return new (pool) ProgramData(); });
		alloc("graphicPipelines", graphicPipelines,
				[&] { return new (pool) GraphicPipelineData(); });
		alloc("computePipelines", computePipelines,
				[&] { return new (pool) ComputePipelineData(); });
		alloc("pipelineFamilies", pipelineFamilies,
				[&] { return new (pool) PipelineFamilyData(); });
		alloc("textureSets", textureSets, [&] { return new (pool) TextureSetLayoutData(); });
		alloc("attachments", attachments, [&] { return new (pool) AttachmentData(); });
		alloc("attachmentPasses", attachmentPasses,
				[&] { return new (pool) AttachmentPassData(); });
		alloc("attachmentSubpasses", attachmentSubpasses,
				[&] { return new (pool) AttachmentSubpassData(); });
		alloc("subpasses", subpasses, [&] { return new (pool) SubpassData(); });
		alloc("pipelineLayouts", pipelineLayouts, [&] { return new (pool) PipelineLayoutData(); });
		alloc("descriptorSets", descriptorSets, [&] { return new (pool) DescriptorSetData(); });
		alloc("descriptors", descriptors, [&] { return new (pool) PipelineDescriptor(); });
		alloc("passes", passes, [&] { return new (pool) QueuePassData(); });

		auto keyOf = [&](const DataValue &n) {
			return StringView(at(n, 0).getString()).pdup(pool);
		};

		// 2) fill nodes (field order documented at the matching emitters in QueueEncoder)
		auto &progArr = root.getValue("programs").getArray();
		for (size_t i = 0; i < programs.size(); ++i) {
			auto &n = progArr[i];
			auto p = programs[i];
			p->key = keyOf(n);
			p->stage = core::ProgramStage(at(n, 1).getInteger());
			auto spv = bytesAt(n, 2).pdup(pool);
			p->data = SpanView<uint32_t>(reinterpret_cast<const uint32_t *>(spv.data()),
					spv.size() / sizeof(uint32_t));
			p->program = factory.makeShader(uint64_t(at(n, 3).getInteger()), p->stage);
		}

		auto &tslArr = root.getValue("textureSets").getArray();
		for (size_t i = 0; i < textureSets.size(); ++i) {
			auto &n = tslArr[i];
			auto t = textureSets[i];
			t->key = keyOf(n);
			t->queue = data;
			t->imageCount = uint32_t(at(n, 1).getInteger());
			t->imageCountIndexed = uint32_t(at(n, 2).getInteger());
			t->bufferCount = uint32_t(at(n, 3).getInteger());
			t->bufferCountIndexed = uint32_t(at(n, 4).getInteger());
			for (auto &sv : at(n, 5).getArray()) { t->samplers.emplace_back(valueToSampler(sv)); }
			t->layout = factory.makeTextureSetLayout(uint64_t(at(n, 6).getInteger()), t->imageCount,
					uint32_t(t->samplers.size()));
			derefArray(n, 7, pipelineLayouts, t->bindingLayouts,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		auto &gpArr = root.getValue("graphicPipelines").getArray();
		for (size_t i = 0; i < graphicPipelines.size(); ++i) {
			auto &n = gpArr[i];
			auto p = graphicPipelines[i];
			p->key = keyOf(n);
			p->dynamicState = core::DynamicState(at(n, 1).getInteger());
			auto mat = bytesAt(n, 2);
			if (mat.size() >= sizeof(PipelineMaterialInfo)) {
				PipelineMaterialInfo mi;
				memcpy(&mi, mat.data(), sizeof(PipelineMaterialInfo));
				p->material = mi;
			}
			p->subpass = deref(subpasses, refAt(n, 3));
			p->layout = deref(pipelineLayouts, refAt(n, 4));
			for (auto &sv : at(n, 5).getArray()) {
				if (auto prog = deref(programs, sv.getInteger())) {
					p->shaders.emplace_back(SpecializationInfo(prog));
				}
			}
			p->pipeline = factory.makeGraphicPipeline(uint64_t(at(n, 6).getInteger()));
			p->family = deref(pipelineFamilies, refAt(n, 7));
		}

		auto &cpArr = root.getValue("computePipelines").getArray();
		for (size_t i = 0; i < computePipelines.size(); ++i) {
			auto &n = cpArr[i];
			auto p = computePipelines[i];
			p->key = keyOf(n);
			p->subpass = deref(subpasses, refAt(n, 1));
			p->layout = deref(pipelineLayouts, refAt(n, 2));
			if (auto prog = deref(programs, refAt(n, 3))) {
				p->shader = SpecializationInfo(prog);
			}
			p->pipeline = factory.makeComputePipeline(uint64_t(at(n, 4).getInteger()),
					uint32_t(at(n, 5).getInteger()), uint32_t(at(n, 6).getInteger()),
					uint32_t(at(n, 7).getInteger()));
			p->family = deref(pipelineFamilies, refAt(n, 8));
		}

		auto &apArr = root.getValue("attachmentPasses").getArray();
		auto &asArr = root.getValue("attachmentSubpasses").getArray();
		auto &spArr = root.getValue("subpasses").getArray();
		auto &plArr = root.getValue("pipelineLayouts").getArray();
		auto &dsArr = root.getValue("descriptorSets").getArray();
		auto &dArr = root.getValue("descriptors").getArray();
		auto &atArr = root.getValue("attachments").getArray();
		auto &passArr = root.getValue("passes").getArray();

		// attachments
		for (size_t i = 0; i < attachments.size(); ++i) {
			auto &n = atArr[i];
			auto a = attachments[i];
			a->key = keyOf(n);
			a->queue = data;
			a->id = uint64_t(at(n, 1).getInteger());
			a->ops = core::AttachmentOps(at(n, 2).getInteger());
			a->type = core::AttachmentType(at(n, 3).getInteger());
			a->usage = core::AttachmentUsage(at(n, 4).getInteger());
			a->outputState = core::FrameRenderPassState(at(n, 5).getInteger());
			a->transient = at(n, 6).getBool();
			derefArray(n, 7, attachmentPasses, a->passes,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// attachment passes
		for (size_t i = 0; i < attachmentPasses.size(); ++i) {
			auto &n = apArr[i];
			auto a = attachmentPasses[i];
			a->key = keyOf(n);
			a->attachment = deref(attachments, refAt(n, 1));
			a->pass = deref(passes, refAt(n, 2));
			a->index = uint32_t(at(n, 3).getInteger());
			a->ops = core::AttachmentOps(at(n, 4).getInteger());
			a->initialLayout = core::AttachmentLayout(at(n, 5).getInteger());
			a->finalLayout = core::AttachmentLayout(at(n, 6).getInteger());
			a->loadOp = core::AttachmentLoadOp(at(n, 7).getInteger());
			a->storeOp = core::AttachmentStoreOp(at(n, 8).getInteger());
			a->stencilLoadOp = core::AttachmentLoadOp(at(n, 9).getInteger());
			a->stencilStoreOp = core::AttachmentStoreOp(at(n, 10).getInteger());
			uint32_t cm = uint32_t(at(n, 11).getInteger());
			a->colorMode = reinterpret_cast<core::ColorMode &>(cm);
			a->dependency = valueToDep(at(n, 12));
			derefArray(n, 13, descriptors, a->descriptors,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, 14, attachmentSubpasses, a->subpasses,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// attachment subpasses
		for (size_t i = 0; i < attachmentSubpasses.size(); ++i) {
			auto &n = asArr[i];
			auto a = attachmentSubpasses[i];
			a->key = keyOf(n);
			a->pass = deref(attachmentPasses, refAt(n, 1));
			a->subpass = deref(subpasses, refAt(n, 2));
			a->layout = core::AttachmentLayout(at(n, 3).getInteger());
			a->usage = core::AttachmentUsage(at(n, 4).getInteger());
			a->ops = core::AttachmentOps(at(n, 5).getInteger());
			a->dependency = valueToDep(at(n, 6));
			uint32_t bl = uint32_t(at(n, 7).getInteger());
			a->blendInfo = reinterpret_cast<core::BlendInfo &>(bl);
		}

		// subpasses
		for (size_t i = 0; i < subpasses.size(); ++i) {
			auto &n = spArr[i];
			auto s = subpasses[i];
			s->key = keyOf(n);
			s->pass = deref(passes, refAt(n, 1));
			s->index = uint32_t(at(n, 2).getInteger());
			for (auto &e : at(n, 3).getArray()) {
				if (auto p = deref(graphicPipelines, e.getInteger())) {
					s->graphicPipelines.emplace(p);
				}
			}
			for (auto &e : at(n, 4).getArray()) {
				if (auto p = deref(computePipelines, e.getInteger())) {
					s->computePipelines.emplace(p);
				}
			}
			derefArray(n, 5, attachmentSubpasses, s->inputImages,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, 6, attachmentSubpasses, s->outputImages,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, 7, attachmentSubpasses, s->resolveImages,
					[](auto &out, auto p) { out.emplace_back(p); });
			s->depthStencil = deref(attachmentSubpasses, refAt(n, 8));
			for (auto &e : at(n, 9).getArray()) {
				s->preserve.emplace_back(uint32_t(e.getInteger()));
			}
		}

		// pipeline layouts
		for (size_t i = 0; i < pipelineLayouts.size(); ++i) {
			auto &n = plArr[i];
			auto l = pipelineLayouts[i];
			l->key = keyOf(n);
			l->pass = deref(passes, refAt(n, 1));
			l->index = uint32_t(at(n, 2).getInteger());
			l->textureSetLayout = deref(textureSets, refAt(n, 3));
			derefArray(n, 4, descriptorSets, l->sets,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, 5, graphicPipelines, l->graphicPipelines,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, 6, computePipelines, l->computePipelines,
					[](auto &out, auto p) { out.emplace_back(p); });
			l->defaultFamily = deref(pipelineFamilies, refAt(n, 7));
			derefArray(n, 8, pipelineFamilies, l->families,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// pipeline families (graphicPipelines/computePipelines/layout are all references into the
		// tables allocated above; readMaterials walks layout->families->graphicPipelines)
		auto &pfArr = root.getValue("pipelineFamilies").getArray();
		for (size_t i = 0; i < pipelineFamilies.size(); ++i) {
			auto &n = pfArr[i];
			auto f = pipelineFamilies[i];
			f->key = keyOf(n);
			f->layout = deref(pipelineLayouts, refAt(n, 1));
			derefArray(n, 2, graphicPipelines, f->graphicPipelines,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, 3, computePipelines, f->computePipelines,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// descriptor sets
		for (size_t i = 0; i < descriptorSets.size(); ++i) {
			auto &n = dsArr[i];
			auto s = descriptorSets[i];
			s->key = keyOf(n);
			s->layout = deref(pipelineLayouts, refAt(n, 1));
			s->index = uint32_t(at(n, 2).getInteger());
			derefArray(n, 3, descriptors, s->descriptors,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// descriptors
		for (size_t i = 0; i < descriptors.size(); ++i) {
			auto &n = dArr[i];
			auto d = descriptors[i];
			d->key = keyOf(n);
			d->set = deref(descriptorSets, refAt(n, 1));
			d->attachment = deref(attachmentPasses, refAt(n, 2));
			d->type = core::DescriptorType(at(n, 3).getInteger());
			d->stages = core::ProgramStage(at(n, 4).getInteger());
			d->layout = core::AttachmentLayout(at(n, 5).getInteger());
			d->count = uint32_t(at(n, 6).getInteger());
			d->index = uint32_t(at(n, 7).getInteger());
			d->requestFlags = core::DescriptorFlags(at(n, 8).getInteger());
			d->deviceFlags = core::DescriptorFlags(at(n, 9).getInteger());
		}

		// passes
		for (size_t i = 0; i < passes.size(); ++i) {
			auto &n = passArr[i];
			auto p = passes[i];
			p->key = keyOf(n);
			p->queue = data;
			p->type = core::PassType(at(n, 1).getInteger());
			p->ordering = core::RenderOrdering(uint32_t(at(n, 2).getInteger()));
			p->hasUpdateAfterBind = at(n, 3).getBool();
			p->acquireTimestamps = uint32_t(at(n, 4).getInteger());
			derefArray(n, 5, attachmentPasses, p->attachments,
					[](auto &out, auto x) { out.emplace_back(x); });
			derefArray(n, 6, subpasses, p->subpasses,
					[](auto &out, auto x) { out.emplace_back(x); });
			derefArray(n, 7, pipelineLayouts, p->pipelineLayouts,
					[](auto &out, auto x) { out.emplace_back(x); });
			for (auto &dv : at(n, 8).getArray()) {
				core::SubpassDependency dep;
				dep.srcSubpass = uint32_t(at(dv, 0).getInteger());
				dep.srcStage = core::PipelineStage(at(dv, 1).getInteger());
				dep.srcAccess = core::AccessType(at(dv, 2).getInteger());
				dep.dstSubpass = uint32_t(at(dv, 3).getInteger());
				dep.dstStage = core::PipelineStage(at(dv, 4).getInteger());
				dep.dstAccess = core::AccessType(at(dv, 5).getInteger());
				dep.byRegion = at(dv, 6).getBool();
				p->dependencies.emplace_back(dep);
			}
			p->impl = factory.makeRenderPass(uint64_t(at(n, 9).getInteger()), p->type,
					uint64_t(at(n, 10).getInteger()));
			// stub QueuePass so QueueData::clear() can invalidate it safely on teardown
			p->pass = Rc<core::QueuePass>::alloc();
		}

		// 3) top-level tables and lists
		for (auto p : programs) { data->programs.emplace(p); }
		for (auto p : graphicPipelines) { data->graphicPipelines.emplace(p); }
		for (auto p : computePipelines) { data->computePipelines.emplace(p); }
		for (auto t : textureSets) { data->textureSets.emplace(t); }
		for (auto a : attachments) { data->attachments.emplace(a); }
		for (auto p : passes) { data->passes.emplace(p); }

		for (auto &e : root.getValue("input").getArray()) {
			if (auto a = deref(attachments, e.getInteger())) {
				data->input.emplace_back(a);
			}
		}
		for (auto &e : root.getValue("output").getArray()) {
			if (auto a = deref(attachments, e.getInteger())) {
				data->output.emplace_back(a);
			}
		}
	}, queue._data->pool);

	// 4) resources (rebuilt as standalone core::Resource objects, then linked)
	if (root.getValue("internalResource").isArray()) {
		data->resource = decodeResourceValue(root.getValue("internalResource"), factory);
	}
	for (auto &lr : root.getValue("linkedResources").getArray()) {
		if (auto r = decodeResourceValue(lr, factory)) {
			memory::perform([&] { data->linked.emplace(sp::move(r)); }, pool);
		}
	}
	if (data->resource) {
		if (root.hasValue("emptyImage")) {
			data->emptyImage = data->resource->getImage(root.getString("emptyImage"));
		}
		if (root.hasValue("solidImage")) {
			data->solidImage = data->resource->getImage(root.getString("solidImage"));
		}
		if (root.hasValue("emptyBuffer")) {
			data->emptyBuffer = data->resource->getBuffer(root.getString("emptyBuffer"));
		}
	}

	// 5) materials (rebuilt after resources so the factory's image/view cache is populated; each
	// shared MaterialSet is installed onto a mirror MaterialAttachment placed in its owner
	// AttachmentData::attachment, mirroring the server-side getAttachments()->getMaterials() path and
	// released by QueueData::clear() on teardown). gAPI objects resolve back through the factory by
	// id; the per-image ImageData, dynamic instances, atlas, owned data and gAPI TextureSets stay
	// null on the mirror (see the encode comment).
	for (auto &m : root.getValue("materials").getArray()) {
		auto attData = deref(attachments, refAt(m, 0));
		if (!attData) {
			continue;
		}
		auto layout = deref(textureSets, refAt(m, 1));

		auto att = Rc<core::MaterialAttachment>::alloc();
		att->_data = attData;
		att->_targetLayout = layout;

		auto set = Rc<core::MaterialSet>::create(uint32_t(at(m, 2).getInteger()), att.get());
		set->_generation = uint64_t(at(m, 3).getInteger());

		// highest server-assigned id in this set; the client owns the queue and allocates new
		// material ids itself, so the mirror's counter must resume just past the shared range
		core::MaterialId maxId = 0;

		// materials first: makeImageView caches id -> view so the layout slots below resolve them
		for (auto &mn : at(m, 4).getArray()) {
			auto id = core::MaterialId(at(mn, 0).getInteger());
			if (id != core::Material::MaterialIdInitial && id > maxId) {
				maxId = id;
			}
			auto layoutIndex = uint32_t(at(mn, 1).getInteger());
			auto pipeline = queue.getGraphicPipeline(at(mn, 2).getString());
			auto bufferId = uint64_t(at(mn, 3).getInteger());

			Vector<core::MaterialImage> images;
			for (auto &in : at(mn, 4).getArray()) {
				images.emplace_back(decodeMaterialImage(in, factory));
			}

			auto mat = Rc<core::Material>::create(id, pipeline, sp::move(images), Rc<Ref>());
			mat->_layoutIndex = layoutIndex;
			if (auto buf = factory.resolveObject(bufferId)) {
				mat->_buffer = Rc<core::BufferObject>(static_cast<core::BufferObject *>(buf));
			}
			set->_materials.emplace(mat->getId(), sp::move(mat));
		}

		for (auto &ln : at(m, 5).getArray()) {
			set->getLayouts().emplace_back(decodeMaterialLayout(ln, factory));
		}

		for (auto &un : at(m, 6).getArray()) {
			set->_updatedMaterials.emplace_back(core::MaterialId(un.getInteger()));
		}

		// resume allocation a number next after the max shared MaterialId
		att->_attachmentMaterialId = maxId + 1;

		att->setMaterials(set);

		// Re-bind the predefined materials by id to the SAME Material objects now in the set (do not
		// call addPredefinedMaterials -- it would re-allocate their ids). FrameContext::readMaterials
		// registers exactly this list for material lookup, so a missing list means predefined textures
		// like SolidImage resolve to no material.
		for (auto &pid : at(m, 7).getArray()) {
			auto mIt = set->getMaterials().find(core::MaterialId(pid.getInteger()));
			if (mIt != set->getMaterials().end()) {
				att->_predefinedMaterials.emplace_back(mIt->second);
			}
		}

		attData->attachment = att;
	}

	// 6) wrap in a Queue (friend access to adopt the prebuilt data; bypasses Queue::init)
	if (data->resource) {
		data->resource->setOwner(&queue);
	}

	return true;
}

// --- Material update codec (server -> client push for an already-shared queue) ---------------

Bytes QueueCodec::encodeMaterials(uint64_t queueId, core::MaterialSet &set,
		ObjectRegistry &registry) {
	auto owner = set.getOwner();
	if (!owner || !owner->getData()) {
		return Bytes();
	}

	DataValue root(DataValue::Type::DICTIONARY);
	root.setInteger(int64_t(kCodecVersion), "v");
	root.setInteger(int64_t(queueId), "queue");
	root.setString(owner->getData()->key, "owner");
	root.setInteger(int64_t(set.getImagesInSet()), "imagesInSet");
	root.setInteger(int64_t(set.getGeneration()), "generation");

	DataValue mats(DataValue::Type::ARRAY);
	for (auto &it : set.getMaterials()) { mats.addValue(encodeMaterial(it.second.get(), registry)); }
	root.setValue(sp::move(mats), "materials");

	DataValue layouts(DataValue::Type::ARRAY);
	for (auto &l : set.getLayouts()) { layouts.addValue(encodeMaterialLayout(l, registry)); }
	root.setValue(sp::move(layouts), "layouts");

	DataValue updated(DataValue::Type::ARRAY);
	set.foreachUpdated([&](core::MaterialId id, NotNull<core::Material>) {
		updated.addInteger(int64_t(id));
	}, false);
	root.setValue(sp::move(updated), "updated");

	return data::write(root, data::EncodeFormat::Cbor);
}

bool QueueCodec::decodeMaterials(BytesView bytes, ObjectFactory &factory) {
	auto root = data::read<memory::StandartInterface>(bytes);
	if (!root.isDictionary() || root.getInteger("v") != int64_t(kCodecVersion)) {
		return false;
	}

	auto queue = factory.resolveQueue(uint64_t(root.getInteger("queue")));
	if (!queue) {
		return false;
	}

	// resolve the existing mirror MaterialAttachment by key and replace its MaterialSet in place
	auto attData = queue->getAttachment(root.getString("owner"));
	if (!attData || attData->type != core::AttachmentType::Material || !attData->attachment) {
		return false;
	}
	auto att = static_cast<core::MaterialAttachment *>(attData->attachment.get());

	auto set = Rc<core::MaterialSet>::create(uint32_t(root.getInteger("imagesInSet")), att);
	set->_generation = uint64_t(root.getInteger("generation"));

	core::MaterialId maxId = 0;
	// materials first: makeImageView caches id -> view so the layout slots below resolve them
	for (auto &mn : root.getValue("materials").getArray()) {
		auto id = core::MaterialId(at(mn, 0).getInteger());
		if (id != core::Material::MaterialIdInitial && id > maxId) {
			maxId = id;
		}
		auto layoutIndex = uint32_t(at(mn, 1).getInteger());
		auto pipeline = queue->getGraphicPipeline(at(mn, 2).getString());
		auto bufferId = uint64_t(at(mn, 3).getInteger());

		Vector<core::MaterialImage> images;
		for (auto &in : at(mn, 4).getArray()) {
			images.emplace_back(decodeMaterialImage(in, factory));
		}

		auto mat = Rc<core::Material>::create(id, pipeline, sp::move(images), Rc<Ref>());
		mat->_layoutIndex = layoutIndex;
		if (auto buf = factory.resolveObject(bufferId)) {
			mat->_buffer = Rc<core::BufferObject>(static_cast<core::BufferObject *>(buf));
		}
		set->_materials.emplace(mat->getId(), sp::move(mat));
	}

	for (auto &ln : root.getValue("layouts").getArray()) {
		set->getLayouts().emplace_back(decodeMaterialLayout(ln, factory));
	}

	for (auto &un : root.getValue("updated").getArray()) {
		set->_updatedMaterials.emplace_back(core::MaterialId(un.getInteger()));
	}

	// resume allocation a number next after the max shared MaterialId
	att->_attachmentMaterialId = maxId + 1;

	att->setMaterials(set);
	return true;
}

} // namespace stappler::xenolith::remote
