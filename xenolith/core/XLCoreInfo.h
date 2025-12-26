/**
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

#ifndef XENOLITH_CORE_XLCOREINFO_H_
#define XENOLITH_CORE_XLCOREINFO_H_

#include "XLCorePipelineInfo.h"
#include "SPBitmap.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class BufferObject;
class ImageObject;
class ImageView;
class Sampler;
class DataAtlas;
class Resource;
class TextureSetLayout;

struct PipelineDescriptor;
struct PipelineLayoutData;
struct QueueData;

#if (XL_USE_64_BIT_PTR_DEFINES == 1)
using ObjectHandle = ValueWrapper<void *, class ObjectHandleFlag>;
#else
using ObjectHandle = ValueWrapper<uint64_t, class ObjectHandleFlag>;
#endif

using MipLevels = ValueWrapper<uint32_t, class MipLevelFlag>;
using ArrayLayers = ValueWrapper<uint32_t, class ArrayLayersFlag>;
using Extent1 = ValueWrapper<uint32_t, class Extent1Flag>;
using BaseArrayLayer = ValueWrapper<uint32_t, class BaseArrayLayerFlag>;

// Опции работы движка презентации кадров
// Позволяют балансировать презентацию кадров между нагрузкой на GPU, низкой задержкой ввода
// и стабильным кадровым интервалом.
// Настройки по умолчанию выполнены сбалансированно в пользу снижения задержки ввода
//
// Ряд опций зависит от реализации оконного менеджера. Для работы движок презантации получает
// предпочитаемый набор опций от окна файловой системы.
//
// При работе нужно помнить, что Xenolith требует стабильной асинхронной работы.
// Использовать опции, которые могут заблокировать презентацию или получение кадрового буфера
// категорически не следует, это может нарушить нормальную работу движка.
// Например, способ презентации в системе в режиме earlyPresent может блокировать презентацию
// по вертикальной синхронизации (оснвоной поток будет заблокирован до сигнала). В таком случае,
// этот режим в системе нужно отключить. Функции получения кадра и презентации всегда должны
// завершаться без блокировки.
//
// Особой осторожности требуют старые или экспериментальные системы, где асинхронный режим
// работы графичского стека не реализован на уровне системы. Для таких систем необходимо
// использовать собственную реализацию таймеров (режим usePresentWindow) вместо ожидания
// синхронизации от самой системы
struct PresentationOptions {
	// Запускать следующий кадр только по запросу либо наличию действий в процессе.
	// В таком случае, PresentationInterval будет считаться на основе реальной презентации
	// Показательными для производительности остаются интервал подготовки кадра и
	// таймер использования GPU
	bool renderOnDemand = true;

	// Использовать внешний сигнал вертикальной синхронизации (система должна поддерживать)
	// В этом режиме готовые к презентации кадры ожидают сигнала, прежде, чем отправиться
	// Также, по сигналу система запрашиваает новый буфер для отрисовки следующего кадра
	// Если система не успела подготовить новый кадр - обновление пропускается
	bool followDisplayLink = false;

	// Начинать новый кадр только по внешнему сигналу вертикальной синхронизации
	// В этом режиме система нчинает новый кадр по сигналу, и отправляет его на презентацию
	// по готовности без ожидания. Если новый сигнал синхронизации поступил до презентации
	// предыдущего кадра - новый кадр будет начат сразу же
	// Обеспечивает более низкую задержку ввода, чем базовый followDisplayLink, но требует
	// большей поддержки от ОС и менеджера окон.
	// Стандартный режим для Linux+Wayland и Android+Choreographer
	// Несовместим с usePresentWindow
	//
	// P.S. Интерфейс к Wayland запрашивает новый сигнал синхронизации только при отправке кадра
	// на презентацию. То есть, если кадр не укладывается в релизное окно, без поддержки от композитора
	// можно ожидать жёсткой потери кадров. В таком случае рекомендуется работать в режиме usePresentWindow
	bool followDisplayLinkBarrier = false;

	// Использовать внеэкранный рендеринг для подготовки изображений. В этом режиме презентация нового изображения выполняется
	// строго синхронно (см. presentImmediate)
	// TODO -пока не реализовано
	bool renderImageOffscreen = false;

	// Начинать новый кадр как только предыдущий был отправлен на исполнение (то есть, до его завершения и презентации)
	// Позволяет иметь несколько кадров в одновременной обработке для большей загрузке графического процессора
	// Не стоит применять без выделенного или интегрированного GPU (то есть, для рендеринга на CPU)
	bool preStartFrame = true;

	// Использовать временное окно для презентации кадра
	// Вместо презентации по готовности, система будет стараться удерживать целевую частоту кадров за счёт откладывания
	// презентации до следующего окна времени
	// Не работает в режиме followDisplayLink и followDisplayLinkBarrier
	bool usePresentWindow = true;

	// Отправлять кадр на презентацию сразу после его отправки в обработку. Может снизить видимую задержку ввода.
	// Использовать с осторожностью в режиме FIFO с двойной буферизацией (предпочитать тройную)
	bool earlyPresent = true;

	// Сихронизировать обновление параметров и закрытие окна с оконным менеджером
	// Необходимо для немедленного повторного использования поверхности менеджером окон (Android)
	//
	// В этом режиме вызов AppWindow::close может создавать существенную задержку
	bool syncConstraintsUpdate = false;
};

struct SP_PUBLIC SamplerIndex : ValueWrapper<uint32_t, class SamplerIndexFlag> {
	// Predefined samplers
	static SamplerIndex DefaultFilterNearest;
	static SamplerIndex DefaultFilterLinear;
	static SamplerIndex DefaultFilterLinearClamped;

	using ValueWrapper::ValueWrapper;
};

struct SP_PUBLIC SamplerInfo {
	Filter magFilter = Filter::Nearest;
	Filter minFilter = Filter::Nearest;
	SamplerMipmapMode mipmapMode = SamplerMipmapMode::Nearest;
	SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
	SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
	SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;
	float mipLodBias = 0.0f;
	bool anisotropyEnable = false;
	float maxAnisotropy = 0.0f;
	bool compareEnable = false;
	CompareOp compareOp = CompareOp::Never;
	float minLod = 0.0;
	float maxLod = 0.0;

	constexpr auto operator<=>(const SamplerInfo &) const = default;
};

using ForceBufferFlags = ValueWrapper<BufferFlags, class ForceBufferFlagsFlag>;
using ForceBufferUsage = ValueWrapper<BufferUsage, class ForceBufferUsageFlag>;
using BufferPersistent = ValueWrapper<bool, class BufferPersistentFlag>;

struct SP_PUBLIC BufferInfo : NamedMem {
	BufferFlags flags = BufferFlags::None;
	BufferUsage usage = BufferUsage::TransferDst;

	// on which type of RenderPass this buffer will be used (there is no universal usage, so, think carefully)
	PassType type = PassType::Graphics;
	uint64_t size = 0;
	bool persistent = true;

	BufferInfo() = default;

	template <typename... Args>
	BufferInfo(Args &&...args) {
		define(std::forward<Args>(args)...);
	}

	void setup(const BufferInfo &value) { *this = value; }
	void setup(BufferFlags value) { flags |= value; }
	void setup(ForceBufferFlags value) { flags = value.get(); }
	void setup(BufferUsage value) { usage |= value; }
	void setup(ForceBufferUsage value) { usage = value.get(); }
	void setup(uint64_t value) { size = value; }
	void setup(BufferPersistent value) { persistent = value.get(); }
	void setup(PassType value) { type = value; }
	void setup(StringView n) { key = n; }

	template <typename T>
	void define(T &&t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename... Args>
	void define(T &&t, Args &&...args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	String description() const;
};

struct SP_PUBLIC BufferData : BufferInfo {
	using DataCallback = memory::callback<void(BytesView)>;

	BytesView data;
	memory::function<void(uint8_t *, uint64_t, const DataCallback &)> memCallback = nullptr;
	std::function<void(uint8_t *, uint64_t, const DataCallback &)> stdCallback = nullptr;
	Rc<BufferObject> buffer; // GL implementation-dependent object
	Rc<DataAtlas> atlas;
	const Resource *resource = nullptr; // owning resource;
	core::AccessType targetAccess = core::AccessType::ShaderRead;

	size_t writeData(uint8_t *mem, size_t expected) const;
};


using ForceImageFlags = ValueWrapper<ImageFlags, class ForceImageFlagsFlag>;
using ForceImageUsage = ValueWrapper<ImageUsage, class ForceImageUsageFlag>;

struct ImageViewInfo;

struct SP_PUBLIC ImageInfoData {
	ImageFormat format = ImageFormat::Undefined;
	ImageFlags flags = ImageFlags::None;
	ImageType imageType = ImageType::Image2D;
	Extent3 extent = Extent3(1, 1, 1);
	MipLevels mipLevels = MipLevels(1);
	ArrayLayers arrayLayers = ArrayLayers(1);
	SampleCount samples = SampleCount::X1;
	ImageTiling tiling = ImageTiling::Optimal;
	ImageUsage usage = ImageUsage::TransferDst;

	// on which type of RenderPass this image will be used (there is no universal usage, so, think carefully)
	PassType type = PassType::Graphics;
	ImageHints hints = ImageHints::None;

	ImageViewInfo getViewInfo(const ImageViewInfo &info) const;

	auto operator<=>(const ImageInfoData &) const = default;

	constexpr bool operator==(const ImageInfoData &other) const {
		return format == other.format && flags == other.flags && imageType == other.imageType
				&& extent == other.extent && mipLevels == other.mipLevels
				&& arrayLayers == other.arrayLayers && samples == other.samples
				&& tiling == other.tiling && usage == other.usage && type == other.type
				&& hints == other.hints;
	}
	constexpr bool operator!=(const ImageInfoData &other) const {
		return format != other.format || flags != other.flags || imageType != other.imageType
				|| extent != other.extent || mipLevels != other.mipLevels
				|| arrayLayers != other.arrayLayers || samples != other.samples
				|| tiling != other.tiling || usage != other.usage || type != other.type
				|| hints != other.hints;
	}
	constexpr bool operator<(const ImageInfoData &other) const {
		if (format < other.format) {
			return true;
		} else if (format > other.format) {
			return false;
		}
		if (flags < other.flags) {
			return true;
		} else if (flags > other.flags) {
			return false;
		}
		if (imageType < other.imageType) {
			return true;
		} else if (imageType > other.imageType) {
			return false;
		}
		if (extent < other.extent) {
			return true;
		} else if (extent > other.extent) {
			return false;
		}
		if (mipLevels < other.mipLevels) {
			return true;
		} else if (mipLevels > other.mipLevels) {
			return false;
		}
		if (arrayLayers < other.arrayLayers) {
			return true;
		} else if (arrayLayers > other.arrayLayers) {
			return false;
		}
		if (samples < other.samples) {
			return true;
		} else if (samples > other.samples) {
			return false;
		}
		if (tiling < other.tiling) {
			return true;
		} else if (tiling > other.tiling) {
			return false;
		}
		if (usage < other.usage) {
			return true;
		} else if (usage > other.usage) {
			return false;
		}
		if (type < other.type) {
			return true;
		} else if (type > other.type) {
			return false;
		}
		if (hints < other.hints) {
			return true;
		} else if (hints > other.hints) {
			return false;
		}
		return false;
	}
};

struct SP_PUBLIC ImageInfo : NamedMem, ImageInfoData {
	ImageInfo() = default;

	template <typename... Args>
	ImageInfo(Args &&...args) {
		define(std::forward<Args>(args)...);
	}

	void setup(Extent1 value) { extent = Extent3(value.get(), 1, 1); }
	void setup(Extent2 value) { extent = Extent3(value.width, value.height, 1); }
	void setup(Extent3 value) {
		extent = value;
		if (extent.depth > 1 && imageType != ImageType::Image3D) {
			imageType = ImageType::Image3D;
		}
	}
	void setup(ImageFlags value) { flags |= value; }
	void setup(ForceImageFlags value) { flags = value.get(); }
	void setup(ImageType value) { imageType = value; }
	void setup(MipLevels value) { mipLevels = value; }
	void setup(ArrayLayers value) { arrayLayers = value; }
	void setup(SampleCount value) { samples = value; }
	void setup(ImageTiling value) { tiling = value; }
	void setup(ImageUsage value) { usage |= value; }
	void setup(ForceImageUsage value) { usage = value.get(); }
	void setup(ImageFormat value) { format = value; }
	void setup(PassType value) { type = value; }
	void setup(ImageHints value) { hints |= value; }
	void setup(StringView value) { key = value; }

	template <typename T>
	void define(T &&t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename... Args>
	void define(T &&t, Args &&...args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	bool isCompatible(const ImageInfo &) const;

	String description() const;
};

using ComponentMappingR = ValueWrapper<ComponentMapping, class ComponentMappingRFlag>;
using ComponentMappingG = ValueWrapper<ComponentMapping, class ComponentMappingGFlag>;
using ComponentMappingB = ValueWrapper<ComponentMapping, class ComponentMappingBFlag>;
using ComponentMappingA = ValueWrapper<ComponentMapping, class ComponentMappingAFlag>;

struct SP_PUBLIC ImageViewInfo {
	ImageFormat format = ImageFormat::Undefined; // inherited from Image if undefined
	ImageViewType type = ImageViewType::ImageView2D;
	ComponentMapping r = ComponentMapping::Identity;
	ComponentMapping g = ComponentMapping::Identity;
	ComponentMapping b = ComponentMapping::Identity;
	ComponentMapping a = ComponentMapping::Identity;
	BaseArrayLayer baseArrayLayer = BaseArrayLayer(0);
	ArrayLayers layerCount = ArrayLayers::max();

	ImageViewInfo() = default;
	ImageViewInfo(const ImageViewInfo &) = default;
	ImageViewInfo(ImageViewInfo &&) = default;
	ImageViewInfo &operator=(const ImageViewInfo &) = default;
	ImageViewInfo &operator=(ImageViewInfo &&) = default;

	template <typename... Args>
	ImageViewInfo(Args &&...args) {
		define(std::forward<Args>(args)...);
	}

	void setup(const ImageViewInfo &);
	void setup(const ImageInfoData &);
	void setup(ImageViewType value) { type = value; }
	void setup(ImageFormat value) { format = value; }
	void setup(ArrayLayers value) { layerCount = value; }
	void setup(BaseArrayLayer value) { baseArrayLayer = value; }
	void setup(ComponentMappingR value) { r = value.get(); }
	void setup(ComponentMappingG value) { g = value.get(); }
	void setup(ComponentMappingB value) { b = value.get(); }
	void setup(ComponentMappingA value) { a = value.get(); }
	void setup(ColorMode value, bool allowSwizzle = true);
	void setup(ImageType, ArrayLayers);

	ColorMode getColorMode() const;

	template <typename T>
	void define(T &&t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename... Args>
	void define(T &&t, Args &&...args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	bool isCompatible(const ImageInfo &) const;
	String description() const;

	auto operator<=>(const ImageViewInfo &) const = default;

	constexpr bool operator==(const ImageViewInfo &other) const {
		return format == other.format && type == other.type && r == other.r && g == other.g
				&& b == other.b && a == other.a && baseArrayLayer == other.baseArrayLayer
				&& layerCount == other.layerCount;
	}
	constexpr bool operator!=(const ImageViewInfo &other) const {
		return format != other.format || type != other.type || r != other.r || g != other.g
				|| b != other.b || a != other.a || baseArrayLayer != other.baseArrayLayer
				|| layerCount != other.layerCount;
	}
	constexpr bool operator<(const ImageViewInfo &other) const {
		if (format < other.format) {
			return true;
		} else if (format > other.format) {
			return false;
		}
		if (type < other.type) {
			return true;
		} else if (type > other.type) {
			return false;
		}
		if (r < other.r) {
			return true;
		} else if (r > other.r) {
			return false;
		}
		if (g < other.g) {
			return true;
		} else if (g > other.g) {
			return false;
		}
		if (b < other.b) {
			return true;
		} else if (b > other.b) {
			return false;
		}
		if (a < other.a) {
			return true;
		} else if (a > other.a) {
			return false;
		}
		if (baseArrayLayer < other.baseArrayLayer) {
			return true;
		} else if (baseArrayLayer > other.baseArrayLayer) {
			return false;
		}
		if (layerCount < other.layerCount) {
			return true;
		} else if (layerCount > other.layerCount) {
			return false;
		}
		return false;
	}
};

struct SP_PUBLIC ImageViewData : ImageViewInfo, memory::AllocPool {
	Rc<ImageView> view;
	const Resource *resource = nullptr; // owning resource;
};

struct SP_PUBLIC ImageData : ImageInfo {
	using DataCallback = memory::callback<void(BytesView)>;

	BytesView data;
	memory::function<void(uint8_t *, uint64_t, const DataCallback &)> memCallback = nullptr;
	std::function<void(uint8_t *, uint64_t, const DataCallback &)> stdCallback = nullptr;
	Rc<ImageObject> image; // GL implementation-dependent object
	Rc<DataAtlas> atlas;
	const Resource *resource = nullptr; // owning resource;
	core::AccessType targetAccess = core::AccessType::ShaderRead;
	core::AttachmentLayout targetLayout = core::AttachmentLayout::ShaderReadOnlyOptimal;

	memory::vector<ImageViewData *> views;

	size_t writeData(uint8_t *mem, size_t expected) const;
};

// OS Windows Manager constraints for a frame presentation
struct SP_PUBLIC FrameConstraints {
	Extent3 extent;
	Padding contentPadding;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;

	float density = 1.0f;
	float surfaceDensity = 1.0f;

	// WM frame interval (it's not an engine limit, but OS limit)
	uint64_t frameInterval = 1'000'000 / 60;

	Size2 getScreenSize() const {
		if (hasFlag(transform, core::SurfaceTransformFlags::PreRotated)) {
			switch (core::getPureTransform(transform)) {
			case SurfaceTransformFlags::Rotate90:
			case SurfaceTransformFlags::Rotate270:
			case SurfaceTransformFlags::MirrorRotate90:
			case SurfaceTransformFlags::MirrorRotate270:
				return Size2(extent.height, extent.width);
				break;
			default: break;
			}
		}
		return Size2(extent.width, extent.height);
	}

	Padding getRotatedPadding() const {
		Padding out = contentPadding;
		switch (transform) {
		case SurfaceTransformFlags::Rotate90:
			out.left = contentPadding.top;
			out.top = contentPadding.right;
			out.right = contentPadding.bottom;
			out.bottom = contentPadding.left;
			break;
		case SurfaceTransformFlags::Rotate180:
			out.left = contentPadding.right;
			out.top = contentPadding.bottom;
			out.right = contentPadding.left;
			out.bottom = contentPadding.top;
			break;
		case SurfaceTransformFlags::Rotate270:
			out.left = contentPadding.bottom;
			out.top = contentPadding.left;
			out.right = contentPadding.top;
			out.bottom = contentPadding.right;
			break;
		case SurfaceTransformFlags::Mirror:
			out.left = contentPadding.right;
			out.right = contentPadding.left;
			break;
		case SurfaceTransformFlags::MirrorRotate90: break;
		case SurfaceTransformFlags::MirrorRotate180:
			out.top = contentPadding.bottom;
			out.bottom = contentPadding.top;
			break;
		case SurfaceTransformFlags::MirrorRotate270: break;
		default: break;
		}
		return out;
	}

	constexpr bool operator==(const FrameConstraints &) const = default;
	constexpr bool operator!=(const FrameConstraints &) const = default;
};

struct SP_PUBLIC SwapchainConfig {
	PresentMode presentMode = PresentMode::Mailbox;
	PresentMode presentModeFast = PresentMode::Unsupported;
	ImageFormat imageFormat = ImageFormat::R8G8B8A8_UNORM;
	ColorSpace colorSpace = ColorSpace::SRGB_NONLINEAR_KHR;
	CompositeAlphaFlags alpha = CompositeAlphaFlags::Opaque;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;
	uint32_t imageCount = 3;
	Extent2 extent;
	bool clipped = false;
	bool transfer = true;
	bool liveResize = false;

	// Used when gAPI support for the fullscreen mode required
	FullScreenExclusiveMode fullscreenMode = FullScreenExclusiveMode::Default;
	void *fullscreenHandle = nullptr;

	String description() const;

	constexpr bool operator==(const SwapchainConfig &) const = default;
	constexpr bool operator!=(const SwapchainConfig &) const = default;
};

struct SP_PUBLIC SurfaceInfo {
	uint32_t minImageCount;
	uint32_t maxImageCount;
	Extent2 currentExtent;
	Extent2 minImageExtent;
	Extent2 maxImageExtent;
	uint32_t maxImageArrayLayers;
	CompositeAlphaFlags supportedCompositeAlpha;
	SurfaceTransformFlags supportedTransforms;
	SurfaceTransformFlags currentTransform;
	ImageUsage supportedUsageFlags;
	Vector<Pair<ImageFormat, ColorSpace>> formats;
	Vector<PresentMode> presentModes;

	// Used when gAPI support for the fullscreen mode required
	FullScreenExclusiveMode fullscreenMode = FullScreenExclusiveMode::Default;
	void *fullscreenHandle = nullptr;

	bool isSupported(const SwapchainConfig &) const;

	String description() const;
};

struct SP_PUBLIC TextureSetLayoutInfo {
	uint32_t imageCount = config::MaxTextureSetImages;
	uint32_t imageCountIndexed = config::MaxTextureSetImagesIndexed;
	uint32_t bufferCount = config::MaxBufferArrayObjects;
	uint32_t bufferCountIndexed = config::MaxBufferArrayObjectsIndexed;
	memory::vector<SamplerInfo> samplers;
};

struct SP_PUBLIC TextureSetLayoutData : NamedMem, TextureSetLayoutInfo {
	QueueData *queue = nullptr;
	Rc<TextureSetLayout> layout;
	memory::vector<Rc<Sampler>> compiledSamplers;
	memory::vector<PipelineLayoutData *> bindingLayouts;
};

struct SP_PUBLIC SubresourceRangeInfo {
	ObjectType type;
	union {
		struct {
			ImageAspects aspectMask;
			uint32_t baseMipLevel;
			uint32_t levelCount;
			uint32_t baseArrayLayer;
			uint32_t layerCount;
		} image;
		struct {
			uint64_t offset;
			uint64_t size;
		} buffer;
	};

	// assume buffer data
	SubresourceRangeInfo(ObjectType);
	SubresourceRangeInfo(ObjectType, uint64_t, uint64_t);

	// assume image data
	SubresourceRangeInfo(ObjectType, ImageAspects);
	SubresourceRangeInfo(ObjectType, ImageAspects, uint32_t, uint32_t, uint32_t, uint32_t);
};

SP_PUBLIC String getBufferFlagsDescription(BufferFlags fmt);
SP_PUBLIC String getBufferUsageDescription(BufferUsage fmt);
SP_PUBLIC String getImageFlagsDescription(ImageFlags fmt);
SP_PUBLIC String getSampleCountDescription(SampleCount fmt);
SP_PUBLIC StringView getImageTypeName(ImageType type);
SP_PUBLIC StringView getImageViewTypeName(ImageViewType type);
SP_PUBLIC StringView getImageFormatName(ImageFormat fmt);
SP_PUBLIC StringView getImageTilingName(ImageTiling type);
SP_PUBLIC StringView getComponentMappingName(ComponentMapping);
SP_PUBLIC StringView getPresentModeName(PresentMode);
SP_PUBLIC StringView getColorSpaceName(ColorSpace);
SP_PUBLIC String getCompositeAlphaFlagsDescription(CompositeAlphaFlags);
SP_PUBLIC String getSurfaceTransformFlagsDescription(SurfaceTransformFlags);
SP_PUBLIC String getImageUsageDescription(ImageUsage fmt);
SP_PUBLIC size_t getFormatBlockSize(ImageFormat format);
SP_PUBLIC PixelFormat getImagePixelFormat(ImageFormat format);
SP_PUBLIC bool isStencilFormat(ImageFormat format);
SP_PUBLIC bool isDepthFormat(ImageFormat format);

SP_PUBLIC ImageViewType getImageViewType(ImageType, ArrayLayers);

SP_PUBLIC bool hasReadAccess(AccessType);
SP_PUBLIC bool hasWriteAccess(AccessType);

SP_PUBLIC String getQueueFlagsDesc(QueueFlags);

SP_PUBLIC Bitmap getBitmap(const ImageInfoData &, BytesView);
SP_PUBLIC bool saveImage(const FileInfo &, const ImageInfoData &, BytesView);

SP_PUBLIC std::ostream &operator<<(std::ostream &stream, const ImageInfoData &value);

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREINFO_H_ */
