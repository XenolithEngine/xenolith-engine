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

#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using DataValue = data::ValueTemplate<memory::StandartInterface>;

static constexpr uint32_t kCodecVersion = 1;

// --- small helpers ---------------------------------------------------------

template <typename T>
static int64_t ei(T v) {
	return int64_t(sprt::toInt(v));
}

static DataValue bytesValue(BytesView v) { return DataValue(Bytes(v.data(), v.data() + v.size())); }

static BytesView readBytes(const DataValue &node, const char *key) {
	if (node.hasValue(key) && node.getValue(key).isBytes()) {
		auto &b = node.getValue(key).getBytes();
		return BytesView(b.data(), b.size());
	}
	return BytesView();
}

static DataValue depToValue(const core::AttachmentDependencyInfo &d) {
	DataValue v(DataValue::Type::DICTIONARY);
	v.setInteger(ei(d.initialUsageStage), "iStage");
	v.setInteger(ei(d.initialAccessMask), "iAccess");
	v.setInteger(ei(d.finalUsageStage), "fStage");
	v.setInteger(ei(d.finalAccessMask), "fAccess");
	v.setInteger(ei(d.requiredRenderPassState), "req");
	v.setInteger(ei(d.lockedRenderPassState), "lock");
	return v;
}

static core::AttachmentDependencyInfo valueToDep(const DataValue &v) {
	core::AttachmentDependencyInfo d;
	d.initialUsageStage = core::PipelineStage(v.getInteger("iStage"));
	d.initialAccessMask = core::AccessType(v.getInteger("iAccess"));
	d.finalUsageStage = core::PipelineStage(v.getInteger("fStage"));
	d.finalAccessMask = core::AccessType(v.getInteger("fAccess"));
	d.requiredRenderPassState = core::FrameRenderPassState(v.getInteger("req"));
	d.lockedRenderPassState = core::FrameRenderPassState(v.getInteger("lock"));
	return d;
}

static DataValue samplerToValue(const core::SamplerInfo &s) {
	DataValue v(DataValue::Type::DICTIONARY);
	v.setInteger(ei(s.magFilter), "magFilter");
	v.setInteger(ei(s.minFilter), "minFilter");
	v.setInteger(ei(s.mipmapMode), "mipmapMode");
	v.setInteger(ei(s.addressModeU), "u");
	v.setInteger(ei(s.addressModeV), "v");
	v.setInteger(ei(s.addressModeW), "w");
	v.setDouble(s.mipLodBias, "mipLodBias");
	v.setBool(s.anisotropyEnable, "aniso");
	v.setDouble(s.maxAnisotropy, "maxAniso");
	v.setBool(s.compareEnable, "cmpEnable");
	v.setInteger(ei(s.compareOp), "cmpOp");
	v.setDouble(s.minLod, "minLod");
	v.setDouble(s.maxLod, "maxLod");
	return v;
}

static core::SamplerInfo valueToSampler(const DataValue &v) {
	core::SamplerInfo s;
	s.magFilter = core::Filter(v.getInteger("magFilter"));
	s.minFilter = core::Filter(v.getInteger("minFilter"));
	s.mipmapMode = core::SamplerMipmapMode(v.getInteger("mipmapMode"));
	s.addressModeU = core::SamplerAddressMode(v.getInteger("u"));
	s.addressModeV = core::SamplerAddressMode(v.getInteger("v"));
	s.addressModeW = core::SamplerAddressMode(v.getInteger("w"));
	s.mipLodBias = float(v.getDouble("mipLodBias"));
	s.anisotropyEnable = v.getBool("aniso");
	s.maxAnisotropy = float(v.getDouble("maxAniso"));
	s.compareEnable = v.getBool("cmpEnable");
	s.compareOp = core::CompareOp(v.getInteger("cmpOp"));
	s.minLod = float(v.getDouble("minLod"));
	s.maxLod = float(v.getDouble("maxLod"));
	return s;
}

// --- Resource codec --------------------------------------------------------

static DataValue encodeResourceValue(const core::Resource &res, ObjectRegistry &reg) {
	DataValue root(DataValue::Type::DICTIONARY);
	root.setString(res.getName(), "name");

	DataValue buffers(DataValue::Type::ARRAY);
	for (auto bd : res.getBuffers()) {
		DataValue b(DataValue::Type::DICTIONARY);
		b.setString(bd->key, "key");
		b.setInteger(ei(bd->flags), "flags");
		b.setInteger(ei(bd->usage), "usage");
		b.setInteger(ei(bd->type), "type");
		b.setInteger(int64_t(bd->size), "size");
		b.setBool(bd->persistent, "persistent");
		b.setInteger(ei(bd->targetAccess), "access");
		// Raw buffer data is not required by client
		// b.setValue(bytesValue(bd->data), "data");
		b.setInteger(int64_t(reg.getId(bd->buffer.get())), "id");
		buffers.addValue(sp::move(b));
	}
	root.setValue(sp::move(buffers), "buffers");

	DataValue images(DataValue::Type::ARRAY);
	for (auto id : res.getImages()) {
		DataValue im(DataValue::Type::DICTIONARY);
		im.setString(id->key, "key");
		im.setInteger(ei(id->format), "format");
		im.setInteger(ei(id->flags), "iflags");
		im.setInteger(ei(id->imageType), "imageType");
		im.setInteger(int64_t(id->extent.width), "w");
		im.setInteger(int64_t(id->extent.height), "h");
		im.setInteger(int64_t(id->extent.depth), "d");
		im.setInteger(int64_t(id->mipLevels.get()), "mips");
		im.setInteger(int64_t(id->arrayLayers.get()), "layers");
		im.setInteger(ei(id->samples), "samples");
		im.setInteger(ei(id->tiling), "tiling");
		im.setInteger(ei(id->usage), "usage");
		im.setInteger(ei(id->type), "passType");
		im.setInteger(ei(id->hints), "hints");
		im.setInteger(ei(id->targetAccess), "access");
		im.setInteger(ei(id->targetLayout), "layout");
		// Raw image data is not required by client
		// im.setValue(bytesValue(id->data), "data");
		im.setInteger(int64_t(reg.getId(id->image.get())), "id");

		DataValue views(DataValue::Type::ARRAY);
		for (auto vd : id->views) {
			DataValue vv(DataValue::Type::DICTIONARY);
			vv.setInteger(ei(vd->format), "format");
			vv.setInteger(ei(vd->type), "type");
			vv.setInteger(ei(vd->r), "r");
			vv.setInteger(ei(vd->g), "g");
			vv.setInteger(ei(vd->b), "b");
			vv.setInteger(ei(vd->a), "a");
			vv.setInteger(int64_t(vd->baseArrayLayer.get()), "baseLayer");
			vv.setInteger(int64_t(vd->layerCount.get()), "layerCount");
			vv.setInteger(int64_t(reg.getId(vd->view.get())), "id");
			views.addValue(sp::move(vv));
		}
		im.setValue(sp::move(views), "views");
		images.addValue(sp::move(im));
	}
	root.setValue(sp::move(images), "images");
	return root;
}

static Rc<core::Resource> decodeResourceValue(const DataValue &root, ObjectFactory &factory) {
	core::Resource::Builder builder(root.getString("name"));
	auto pool = builder.getPool();

	for (auto &b : root.getValue("buffers").getArray()) {
		core::BufferInfo bi;
		bi.flags = core::BufferFlags(b.getInteger("flags"));
		bi.usage = core::BufferUsage(b.getInteger("usage"));
		bi.type = core::PassType(b.getInteger("type"));
		bi.size = uint64_t(b.getInteger("size"));
		bi.persistent = b.getBool("persistent");
		auto access = core::AccessType(b.getInteger("access"));
		auto data = readBytes(b, "data").pdup(pool);
		auto id = uint64_t(b.getInteger("id"));

		core::BufferInfo biCopy = bi;
		auto bd = builder.addBufferByRef(b.getString("key"), sp::move(bi), data,
				Rc<core::DataAtlas>(), access);
		if (bd) {
			const_cast<core::BufferData *>(bd)->buffer = factory.makeBuffer(id, biCopy);
		}
	}

	for (auto &im : root.getValue("images").getArray()) {
		core::ImageInfo ii;
		ii.format = core::ImageFormat(im.getInteger("format"));
		ii.flags = core::ImageFlags(im.getInteger("iflags"));
		ii.imageType = core::ImageType(im.getInteger("imageType"));
		ii.extent = Extent3(uint32_t(im.getInteger("w")), uint32_t(im.getInteger("h")),
				uint32_t(im.getInteger("d")));
		ii.mipLevels = core::MipLevels(uint32_t(im.getInteger("mips")));
		ii.arrayLayers = core::ArrayLayers(uint32_t(im.getInteger("layers")));
		ii.samples = core::SampleCount(im.getInteger("samples"));
		ii.tiling = core::ImageTiling(im.getInteger("tiling"));
		ii.usage = core::ImageUsage(im.getInteger("usage"));
		ii.type = core::PassType(im.getInteger("passType"));
		ii.hints = core::ImageHints(im.getInteger("hints"));
		auto access = core::AccessType(im.getInteger("access"));
		auto layout = core::AttachmentLayout(im.getInteger("layout"));
		auto data = readBytes(im, "data").pdup(pool);
		auto id = uint64_t(im.getInteger("id"));

		core::ImageInfoData iiData = ii;
		auto imd = builder.addBitmapImageByRef(im.getString("key"), sp::move(ii), data, layout,
				access);
		if (imd) {
			auto mutImd = const_cast<core::ImageData *>(imd);
			mutImd->image = factory.makeImage(id, iiData);

			for (auto &vv : im.getValue("views").getArray()) {
				core::ImageViewInfo vi;
				vi.format = core::ImageFormat(vv.getInteger("format"));
				vi.type = core::ImageViewType(vv.getInteger("type"));
				vi.r = core::ComponentMapping(vv.getInteger("r"));
				vi.g = core::ComponentMapping(vv.getInteger("g"));
				vi.b = core::ComponentMapping(vv.getInteger("b"));
				vi.a = core::ComponentMapping(vv.getInteger("a"));
				vi.baseArrayLayer = core::BaseArrayLayer(uint32_t(vv.getInteger("baseLayer")));
				vi.layerCount = core::ArrayLayers(uint32_t(vv.getInteger("layerCount")));
				auto viCopy = vi;
				auto vd = builder.addImageView(imd, sp::move(vi));
				if (vd) {
					const_cast<core::ImageViewData *>(vd)->view = factory.makeImageView(
							uint64_t(vv.getInteger("id")), mutImd->image, viCopy);
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
	if (!root.isDictionary()) {
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

	void collectPipelineLayout(const PipelineLayoutData *l) {
		if (pipelineLayouts.add(l)) {
			collectPass(l->pass);
			collectTextureSetLayout(l->textureSetLayout);
			for (auto s : l->sets) { collectDescriptorSet(s); }
			for (auto p : l->graphicPipelines) { collectGraphicPipeline(p); }
			for (auto p : l->computePipelines) { collectComputePipeline(p); }
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

	DataValue emitProgram(const ProgramData *p) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(p->key, "key");
		v.setInteger(ei(p->stage), "stage");
		// Raw shared data is not required by client
		// v.setValue(bytesValue(BytesView(reinterpret_cast<const uint8_t *>(p->data.data()),
		//					 p->data.size() * sizeof(uint32_t))),
		//		"data");
		v.setInteger(int64_t(reg.getId(p->program.get())), "id");
		return v;
	}

	DataValue emitGraphicPipeline(const GraphicPipelineData *p) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(p->key, "key");
		v.setInteger(ei(p->dynamicState), "dynamicState");
		v.setValue(bytesValue(BytesView(reinterpret_cast<const uint8_t *>(&p->material),
						   sizeof(PipelineMaterialInfo))),
				"material");
		v.setInteger(subpasses.ref(p->subpass), "subpass");
		v.setInteger(pipelineLayouts.ref(p->layout), "layout");
		DataValue shaders(DataValue::Type::ARRAY);
		for (auto &s : p->shaders) { shaders.addInteger(programs.ref(s.data)); }
		v.setValue(sp::move(shaders), "shaders");
		v.setInteger(int64_t(reg.getId(p->pipeline.get())), "id");
		return v;
	}

	DataValue emitComputePipeline(const ComputePipelineData *p) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(p->key, "key");
		v.setInteger(subpasses.ref(p->subpass), "subpass");
		v.setInteger(pipelineLayouts.ref(p->layout), "layout");
		v.setInteger(programs.ref(p->shader.data), "shader");
		v.setInteger(int64_t(reg.getId(p->pipeline.get())), "id");
		uint32_t lx = 0, ly = 0, lz = 0;
		if (p->pipeline) {
			lx = p->pipeline->getLocalX();
			ly = p->pipeline->getLocalY();
			lz = p->pipeline->getLocalZ();
		}
		v.setInteger(int64_t(lx), "lx");
		v.setInteger(int64_t(ly), "ly");
		v.setInteger(int64_t(lz), "lz");
		return v;
	}

	DataValue emitTextureSetLayout(const TextureSetLayoutData *t) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(t->key, "key");
		v.setInteger(int64_t(t->imageCount), "imageCount");
		v.setInteger(int64_t(t->imageCountIndexed), "imageCountIndexed");
		v.setInteger(int64_t(t->bufferCount), "bufferCount");
		v.setInteger(int64_t(t->bufferCountIndexed), "bufferCountIndexed");
		DataValue samplers(DataValue::Type::ARRAY);
		for (auto &s : t->samplers) { samplers.addValue(samplerToValue(s)); }
		v.setValue(sp::move(samplers), "samplers");
		v.setInteger(int64_t(reg.getId(t->layout.get())), "id");
		return v;
	}

	DataValue emitAttachment(const AttachmentData *a) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(a->key, "key");
		v.setInteger(int64_t(a->id), "aid");
		v.setInteger(ei(a->ops), "ops");
		v.setInteger(ei(a->type), "type");
		v.setInteger(ei(a->usage), "usage");
		v.setInteger(ei(a->outputState), "outputState");
		v.setBool(a->transient, "transient");
		v.setValue(refArray(attachmentPasses, a->passes), "passes");
		return v;
	}

	DataValue emitAttachmentPass(const AttachmentPassData *a) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(a->key, "key");
		v.setInteger(attachments.ref(a->attachment), "attachment");
		v.setInteger(passes.ref(a->pass), "pass");
		v.setInteger(int64_t(a->index), "index");
		v.setInteger(ei(a->ops), "ops");
		v.setInteger(ei(a->initialLayout), "initialLayout");
		v.setInteger(ei(a->finalLayout), "finalLayout");
		v.setInteger(ei(a->loadOp), "loadOp");
		v.setInteger(ei(a->storeOp), "storeOp");
		v.setInteger(ei(a->stencilLoadOp), "stencilLoadOp");
		v.setInteger(ei(a->stencilStoreOp), "stencilStoreOp");
		v.setInteger(int64_t(a->colorMode.toInt()), "colorMode");
		v.setValue(depToValue(a->dependency), "dependency");
		v.setValue(refArray(descriptors, a->descriptors), "descriptors");
		v.setValue(refArray(attachmentSubpasses, a->subpasses), "subpasses");
		return v;
	}

	DataValue emitAttachmentSubpass(const AttachmentSubpassData *a) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(a->key, "key");
		v.setInteger(attachmentPasses.ref(a->pass), "pass");
		v.setInteger(subpasses.ref(a->subpass), "subpass");
		v.setInteger(ei(a->layout), "layout");
		v.setInteger(ei(a->usage), "usage");
		v.setInteger(ei(a->ops), "ops");
		v.setValue(depToValue(a->dependency), "dependency");
		v.setInteger(int64_t(reinterpret_cast<const uint32_t &>(a->blendInfo)), "blend");
		return v;
	}

	DataValue emitSubpass(const SubpassData *s) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(s->key, "key");
		v.setInteger(passes.ref(s->pass), "pass");
		v.setInteger(int64_t(s->index), "index");
		v.setValue(refArray(graphicPipelines, s->graphicPipelines), "graphicPipelines");
		v.setValue(refArray(computePipelines, s->computePipelines), "computePipelines");
		v.setValue(refArray(attachmentSubpasses, s->inputImages), "inputImages");
		v.setValue(refArray(attachmentSubpasses, s->outputImages), "outputImages");
		v.setValue(refArray(attachmentSubpasses, s->resolveImages), "resolveImages");
		v.setInteger(attachmentSubpasses.ref(s->depthStencil), "depthStencil");
		DataValue preserve(DataValue::Type::ARRAY);
		for (auto p : s->preserve) { preserve.addInteger(int64_t(p)); }
		v.setValue(sp::move(preserve), "preserve");
		return v;
	}

	DataValue emitPipelineLayout(const PipelineLayoutData *l) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(l->key, "key");
		v.setInteger(passes.ref(l->pass), "pass");
		v.setInteger(int64_t(l->index), "index");
		v.setInteger(textureSets.ref(l->textureSetLayout), "textureSetLayout");
		v.setValue(refArray(descriptorSets, l->sets), "sets");
		v.setValue(refArray(graphicPipelines, l->graphicPipelines), "graphicPipelines");
		v.setValue(refArray(computePipelines, l->computePipelines), "computePipelines");
		return v;
	}

	DataValue emitDescriptorSet(const DescriptorSetData *s) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(s->key, "key");
		v.setInteger(pipelineLayouts.ref(s->layout), "layout");
		v.setInteger(int64_t(s->index), "index");
		v.setValue(refArray(descriptors, s->descriptors), "descriptors");
		return v;
	}

	DataValue emitDescriptor(const PipelineDescriptor *d) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(d->key, "key");
		v.setInteger(descriptorSets.ref(d->set), "set");
		v.setInteger(attachmentPasses.ref(d->attachment), "attachment");
		v.setInteger(ei(d->type), "type");
		v.setInteger(ei(d->stages), "stages");
		v.setInteger(ei(d->layout), "layout");
		v.setInteger(int64_t(d->count), "count");
		v.setInteger(int64_t(d->index), "index");
		v.setInteger(ei(d->requestFlags), "requestFlags");
		v.setInteger(ei(d->deviceFlags), "deviceFlags");
		return v;
	}

	DataValue emitPass(const QueuePassData *p) {
		DataValue v(DataValue::Type::DICTIONARY);
		v.setString(p->key, "key");
		v.setInteger(ei(p->type), "type");
		v.setInteger(int64_t(p->ordering.get()), "ordering");
		v.setBool(p->hasUpdateAfterBind, "hasUpdateAfterBind");
		v.setInteger(int64_t(p->acquireTimestamps), "acquireTimestamps");
		v.setValue(refArray(attachmentPasses, p->attachments), "attachments");
		v.setValue(refArray(subpasses, p->subpasses), "subpasses");
		v.setValue(refArray(pipelineLayouts, p->pipelineLayouts), "pipelineLayouts");
		DataValue deps(DataValue::Type::ARRAY);
		for (auto &d : p->dependencies) {
			DataValue dv(DataValue::Type::DICTIONARY);
			dv.setInteger(int64_t(d.srcSubpass), "srcSubpass");
			dv.setInteger(ei(d.srcStage), "srcStage");
			dv.setInteger(ei(d.srcAccess), "srcAccess");
			dv.setInteger(int64_t(d.dstSubpass), "dstSubpass");
			dv.setInteger(ei(d.dstStage), "dstStage");
			dv.setInteger(ei(d.dstAccess), "dstAccess");
			dv.setBool(d.byRegion, "byRegion");
			deps.addValue(sp::move(dv));
		}
		v.setValue(sp::move(deps), "dependencies");
		v.setInteger(int64_t(reg.getId(p->impl.get())), "id");
		v.setInteger(int64_t(p->impl ? p->impl->getIndex() : 0), "implIndex");
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

Bytes QueueCodec::encodeQueue(const core::Queue &queue, ObjectRegistry &registry) {
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
static void derefArray(const DataValue &node, const char *key, const Vector<T *> &tbl, Out &out,
		Fn &&push) {
	for (auto &e : node.getValue(key).getArray()) {
		if (auto p = deref(tbl, e.getInteger())) {
			push(out, p);
		}
	}
}

} // namespace

Rc<core::Queue> QueueCodec::decodeQueue(BytesView bytes, ObjectFactory &factory) {
	auto root = data::read<memory::StandartInterface>(bytes);
	if (!root.isDictionary() || root.getInteger("v") != int64_t(kCodecVersion)) {
		return nullptr;
	}

	auto pool = memory::pool::create((memory::pool_t *)nullptr);
	auto data = new (pool) core::QueueData;
	Rc<core::Queue> queue;

	memory::perform([&] {
		data->pool = pool;
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
		Vector<TextureSetLayoutData *> textureSets;
		Vector<AttachmentData *> attachments;
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

		auto keyOf = [&](const DataValue &n) { return StringView(n.getString("key")).pdup(pool); };

		// 2) fill nodes
		auto &progArr = root.getValue("programs").getArray();
		for (size_t i = 0; i < programs.size(); ++i) {
			auto &n = progArr[i];
			auto p = programs[i];
			p->key = keyOf(n);
			p->stage = core::ProgramStage(n.getInteger("stage"));
			auto spv = readBytes(n, "data").pdup(pool);
			p->data = SpanView<uint32_t>(reinterpret_cast<const uint32_t *>(spv.data()),
					spv.size() / sizeof(uint32_t));
			p->program = factory.makeShader(uint64_t(n.getInteger("id")), p->stage);
		}

		auto &tslArr = root.getValue("textureSets").getArray();
		for (size_t i = 0; i < textureSets.size(); ++i) {
			auto &n = tslArr[i];
			auto t = textureSets[i];
			t->key = keyOf(n);
			t->queue = data;
			t->imageCount = uint32_t(n.getInteger("imageCount"));
			t->imageCountIndexed = uint32_t(n.getInteger("imageCountIndexed"));
			t->bufferCount = uint32_t(n.getInteger("bufferCount"));
			t->bufferCountIndexed = uint32_t(n.getInteger("bufferCountIndexed"));
			for (auto &sv : n.getValue("samplers").getArray()) {
				t->samplers.emplace_back(valueToSampler(sv));
			}
			t->layout = factory.makeTextureSetLayout(uint64_t(n.getInteger("id")), t->imageCount,
					uint32_t(t->samplers.size()));
		}

		auto &gpArr = root.getValue("graphicPipelines").getArray();
		for (size_t i = 0; i < graphicPipelines.size(); ++i) {
			auto &n = gpArr[i];
			auto p = graphicPipelines[i];
			p->key = keyOf(n);
			p->dynamicState = core::DynamicState(n.getInteger("dynamicState"));
			auto mat = readBytes(n, "material");
			if (mat.size() >= sizeof(PipelineMaterialInfo)) {
				PipelineMaterialInfo mi;
				memcpy(&mi, mat.data(), sizeof(PipelineMaterialInfo));
				p->material = mi;
			}
			p->subpass = deref(subpasses, n.getInteger("subpass", -1));
			p->layout = deref(pipelineLayouts, n.getInteger("layout", -1));
			for (auto &sv : n.getValue("shaders").getArray()) {
				if (auto prog = deref(programs, sv.getInteger())) {
					p->shaders.emplace_back(SpecializationInfo(prog));
				}
			}
			p->pipeline = factory.makeGraphicPipeline(uint64_t(n.getInteger("id")));
		}

		auto &cpArr = root.getValue("computePipelines").getArray();
		for (size_t i = 0; i < computePipelines.size(); ++i) {
			auto &n = cpArr[i];
			auto p = computePipelines[i];
			p->key = keyOf(n);
			p->subpass = deref(subpasses, n.getInteger("subpass", -1));
			p->layout = deref(pipelineLayouts, n.getInteger("layout", -1));
			if (auto prog = deref(programs, n.getInteger("shader", -1))) {
				p->shader = SpecializationInfo(prog);
			}
			p->pipeline = factory.makeComputePipeline(uint64_t(n.getInteger("id")),
					uint32_t(n.getInteger("lx")), uint32_t(n.getInteger("ly")),
					uint32_t(n.getInteger("lz")));
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
			a->id = uint64_t(n.getInteger("aid"));
			a->ops = core::AttachmentOps(n.getInteger("ops"));
			a->type = core::AttachmentType(n.getInteger("type"));
			a->usage = core::AttachmentUsage(n.getInteger("usage"));
			a->outputState = core::FrameRenderPassState(n.getInteger("outputState"));
			a->transient = n.getBool("transient");
			derefArray(n, "passes", attachmentPasses, a->passes,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// attachment passes
		for (size_t i = 0; i < attachmentPasses.size(); ++i) {
			auto &n = apArr[i];
			auto a = attachmentPasses[i];
			a->key = keyOf(n);
			a->attachment = deref(attachments, n.getInteger("attachment", -1));
			a->pass = deref(passes, n.getInteger("pass", -1));
			a->index = uint32_t(n.getInteger("index"));
			a->ops = core::AttachmentOps(n.getInteger("ops"));
			a->initialLayout = core::AttachmentLayout(n.getInteger("initialLayout"));
			a->finalLayout = core::AttachmentLayout(n.getInteger("finalLayout"));
			a->loadOp = core::AttachmentLoadOp(n.getInteger("loadOp"));
			a->storeOp = core::AttachmentStoreOp(n.getInteger("storeOp"));
			a->stencilLoadOp = core::AttachmentLoadOp(n.getInteger("stencilLoadOp"));
			a->stencilStoreOp = core::AttachmentStoreOp(n.getInteger("stencilStoreOp"));
			uint32_t cm = uint32_t(n.getInteger("colorMode"));
			a->colorMode = reinterpret_cast<core::ColorMode &>(cm);
			a->dependency = valueToDep(n.getValue("dependency"));
			derefArray(n, "descriptors", descriptors, a->descriptors,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, "subpasses", attachmentSubpasses, a->subpasses,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// attachment subpasses
		for (size_t i = 0; i < attachmentSubpasses.size(); ++i) {
			auto &n = asArr[i];
			auto a = attachmentSubpasses[i];
			a->key = keyOf(n);
			a->pass = deref(attachmentPasses, n.getInteger("pass", -1));
			a->subpass = deref(subpasses, n.getInteger("subpass", -1));
			a->layout = core::AttachmentLayout(n.getInteger("layout"));
			a->usage = core::AttachmentUsage(n.getInteger("usage"));
			a->ops = core::AttachmentOps(n.getInteger("ops"));
			a->dependency = valueToDep(n.getValue("dependency"));
			uint32_t bl = uint32_t(n.getInteger("blend"));
			a->blendInfo = reinterpret_cast<core::BlendInfo &>(bl);
		}

		// subpasses
		for (size_t i = 0; i < subpasses.size(); ++i) {
			auto &n = spArr[i];
			auto s = subpasses[i];
			s->key = keyOf(n);
			s->pass = deref(passes, n.getInteger("pass", -1));
			s->index = uint32_t(n.getInteger("index"));
			for (auto &e : n.getValue("graphicPipelines").getArray()) {
				if (auto p = deref(graphicPipelines, e.getInteger())) {
					s->graphicPipelines.emplace(p);
				}
			}
			for (auto &e : n.getValue("computePipelines").getArray()) {
				if (auto p = deref(computePipelines, e.getInteger())) {
					s->computePipelines.emplace(p);
				}
			}
			derefArray(n, "inputImages", attachmentSubpasses, s->inputImages,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, "outputImages", attachmentSubpasses, s->outputImages,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, "resolveImages", attachmentSubpasses, s->resolveImages,
					[](auto &out, auto p) { out.emplace_back(p); });
			s->depthStencil = deref(attachmentSubpasses, n.getInteger("depthStencil", -1));
			for (auto &e : n.getValue("preserve").getArray()) {
				s->preserve.emplace_back(uint32_t(e.getInteger()));
			}
		}

		// pipeline layouts
		for (size_t i = 0; i < pipelineLayouts.size(); ++i) {
			auto &n = plArr[i];
			auto l = pipelineLayouts[i];
			l->key = keyOf(n);
			l->pass = deref(passes, n.getInteger("pass", -1));
			l->index = uint32_t(n.getInteger("index"));
			l->textureSetLayout = deref(textureSets, n.getInteger("textureSetLayout", -1));
			derefArray(n, "sets", descriptorSets, l->sets,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, "graphicPipelines", graphicPipelines, l->graphicPipelines,
					[](auto &out, auto p) { out.emplace_back(p); });
			derefArray(n, "computePipelines", computePipelines, l->computePipelines,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// descriptor sets
		for (size_t i = 0; i < descriptorSets.size(); ++i) {
			auto &n = dsArr[i];
			auto s = descriptorSets[i];
			s->key = keyOf(n);
			s->layout = deref(pipelineLayouts, n.getInteger("layout", -1));
			s->index = uint32_t(n.getInteger("index"));
			derefArray(n, "descriptors", descriptors, s->descriptors,
					[](auto &out, auto p) { out.emplace_back(p); });
		}

		// descriptors
		for (size_t i = 0; i < descriptors.size(); ++i) {
			auto &n = dArr[i];
			auto d = descriptors[i];
			d->key = keyOf(n);
			d->set = deref(descriptorSets, n.getInteger("set", -1));
			d->attachment = deref(attachmentPasses, n.getInteger("attachment", -1));
			d->type = core::DescriptorType(n.getInteger("type"));
			d->stages = core::ProgramStage(n.getInteger("stages"));
			d->layout = core::AttachmentLayout(n.getInteger("layout"));
			d->count = uint32_t(n.getInteger("count"));
			d->index = uint32_t(n.getInteger("index"));
			d->requestFlags = core::DescriptorFlags(n.getInteger("requestFlags"));
			d->deviceFlags = core::DescriptorFlags(n.getInteger("deviceFlags"));
		}

		// passes
		for (size_t i = 0; i < passes.size(); ++i) {
			auto &n = passArr[i];
			auto p = passes[i];
			p->key = keyOf(n);
			p->queue = data;
			p->type = core::PassType(n.getInteger("type"));
			p->ordering = core::RenderOrdering(uint32_t(n.getInteger("ordering")));
			p->hasUpdateAfterBind = n.getBool("hasUpdateAfterBind");
			p->acquireTimestamps = uint32_t(n.getInteger("acquireTimestamps"));
			derefArray(n, "attachments", attachmentPasses, p->attachments,
					[](auto &out, auto x) { out.emplace_back(x); });
			derefArray(n, "subpasses", subpasses, p->subpasses,
					[](auto &out, auto x) { out.emplace_back(x); });
			derefArray(n, "pipelineLayouts", pipelineLayouts, p->pipelineLayouts,
					[](auto &out, auto x) { out.emplace_back(x); });
			for (auto &dv : n.getValue("dependencies").getArray()) {
				core::SubpassDependency dep;
				dep.srcSubpass = uint32_t(dv.getInteger("srcSubpass"));
				dep.srcStage = core::PipelineStage(dv.getInteger("srcStage"));
				dep.srcAccess = core::AccessType(dv.getInteger("srcAccess"));
				dep.dstSubpass = uint32_t(dv.getInteger("dstSubpass"));
				dep.dstStage = core::PipelineStage(dv.getInteger("dstStage"));
				dep.dstAccess = core::AccessType(dv.getInteger("dstAccess"));
				dep.byRegion = dv.getBool("byRegion");
				p->dependencies.emplace_back(dep);
			}
			p->impl = factory.makeRenderPass(uint64_t(n.getInteger("id")), p->type,
					uint64_t(n.getInteger("implIndex")));
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
	}, pool);

	// 4) resources (rebuilt as standalone core::Resource objects, then linked)
	if (root.getValue("internalResource").isDictionary()) {
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

	// 5) wrap in a Queue (friend access to adopt the prebuilt data; bypasses Queue::init)
	queue = Rc<core::Queue>::alloc();
	data->queue = queue.get();
	if (data->resource) {
		data->resource->setOwner(queue.get());
	}
	queue->_data = data;
	return queue;
}

} // namespace stappler::xenolith::remote
