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

#include "XLFontComponent.h"
#include "XLCoreLoop.h"
#include "XLCoreQueue.h"
#include "XLCoreImageStorage.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"
#include "XLAppThread.h"
#include "XLVkFontQueue.h"
#include "SPEventLooper.h"
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H
#include FT_ADVANCES_H

#include "SPFontFace.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::font {

Rc<ContextComponent> FontComponent::createFontComponent(Context *a) {
	return Rc<FontComponent>::create(a);
}

Rc<FontController> FontComponent::createDefaultController(FontComponent *ext, event::Looper *looper,
		StringView name) {
	auto builder = FontComponent::makeDefaultControllerBuilder(name);
	return ext->acquireController(looper, move(builder));
}

static Bytes openResourceFont(FontLibrary::DefaultFontName name) {
	auto d = FontLibrary::getFont(name);
	return data::decompress<memory::StandartInterface>(d.data(), d.size());
}

static String getResourceFontName(FontLibrary::DefaultFontName name) {
	return toString("resource:", FontLibrary::getFontName(name));
}

static const FontController::FontSource *makeResourceFontQuery(FontController::Builder &builder,
		FontLibrary::DefaultFontName name, FontLayoutParameters params = FontLayoutParameters()) {
	return builder.addFontSource(getResourceFontName(name),
			[name] { return openResourceFont(name); }, params);
}

FontController::Builder FontComponent::makeDefaultControllerBuilder(StringView key) {
	FontController::Builder ret(key);

	auto res_RobotoFlex = makeResourceFontQuery(ret, DefaultFontName::RobotoFlex_VariableFont);
	auto res_RobotoMono_Variable =
			makeResourceFontQuery(ret, DefaultFontName::RobotoMono_VariableFont);
	auto res_RobotoMono_Italic_Variable = makeResourceFontQuery(ret,
			DefaultFontName::RobotoMono_Italic_VariableFont,
			FontLayoutParameters{FontStyle::Italic, FontWeight::Normal, FontStretch::Normal});
	auto res_DejaVuSans = ret.addFontSource(getResourceFontName(DefaultFontName::DejaVuSans),
			FontLibrary::getFont(DefaultFontName::DejaVuSans));

	ret.addFontFaceQuery("sans", res_RobotoFlex);
	ret.addFontFaceQuery("sans", res_DejaVuSans);
	ret.addFontFaceQuery("monospace", res_RobotoMono_Variable);
	ret.addFontFaceQuery("monospace", res_RobotoMono_Italic_Variable);

	ret.addAlias("default", "sans");
	ret.addAlias("system", "sans");

	return ret;
}

Rc<core::DynamicImage> FontComponent::makeInitialImage(StringView name) {
	return Rc<core::DynamicImage>::create(
			[name = name.str<Interface>()](core::DynamicImage::Builder &builder) {
		builder.setImage(toString(name, "_Image"),
				core::ImageInfo(Extent2(2, 2),
						core::ImageUsage::Sampled | core::ImageUsage::TransferSrc,
						core::PassType::Graphics, core::ImageFormat::R8_UNORM),
				[](uint8_t *ptr, uint64_t, const core::ImageData::DataCallback &cb) {
			if (ptr) {
				ptr[0] = 0;
				ptr[1] = 255;
				ptr[2] = 255;
				ptr[3] = 0;
			} else {
				Bytes bytes;
				bytes.reserve(4);
				bytes.emplace_back(0);
				bytes.emplace_back(255);
				bytes.emplace_back(255);
				bytes.emplace_back(0);
				cb(bytes);
			}
		}, nullptr);
		return true;
	});
}

bool FontComponent::init(Context *ctx) {
	_context = ctx;
	_library = Rc<FontLibrary>::alloc();
	return true;
}

void FontComponent::handleStart(Context *a) {

#if MODULE_XENOLITH_BACKEND_VK
	if (!_queue) {
		if (a->getGlLoop()->getInstance()->getApi() == core::InstanceApi::Vulkan) {
			_queue = Rc<vk::FontQueue>::create("FontQueue");
		}
	}
#endif

	if (!_queue) {
		log::source().error("FontComponent", "Fail to create FontQueue for GAPI");
	}

	if (_queue->isCompiled()) {
		handleActivated();
	} else {
		auto linkId = retain();
		a->getGlLoop()->compileQueue(_queue, [this, linkId](bool success) {
			if (success) {
				handleActivated();
			}
			release(linkId);
		});
	}
}

void FontComponent::handleStop(Context *a) {
	_library->invalidate();
	_queue = nullptr;
	_active = false;
}

void FontComponent::handleSystemNotification(Context *a, SystemNotification note) {
	switch (note) {
	case SystemNotification::LowMemory: update(); break;
	default: break;
	}
}

void FontComponent::update() { _library->update(); }

Rc<FontController> FontComponent::acquireController(event::Looper *looper,
		FontController::Builder &&b) {
	struct ControllerBuilder : Ref {
		FontController::Builder builder;
		Rc<FontController> controller;
		Rc<event::Looper> looper;

		bool invalid = false;
		std::atomic<size_t> pendingData = 0;
		FontComponent *ext = nullptr;

		ControllerBuilder(FontController::Builder &&b) : builder(move(b)) {
			if (auto c = builder.getTarget()) {
				controller = c;
			}
		}

		void invalidate() { controller = nullptr; }

		void loadData() {
			if (invalid) {
				invalidate();
				return;
			}

			looper->performOnThread([this]() {
				for (const Pair<const String, FontController::FamilyQuery> &it :
						builder.getFamilyQueries()) {
					Vector<Rc<FontFaceData>> d;
					d.reserve(it.second.sources.size());
					for (auto &iit : it.second.sources) { d.emplace_back(move(iit->data)); }
					controller->addFont(it.second.family, sp::move(d), it.second.addInFront);
				}

				if (builder.getTarget()) {
					for (auto &it : builder.getAliases()) {
						controller->addAlias(it.first, it.second);
					}
					controller->sendFontUpdatedEvent();
				} else {
					controller->setAliases(Map<String, String>(builder.getAliases()));
					controller->setLoaded(true);
				}
				controller = nullptr;
			}, this);
		}

		void onDataLoaded(bool success) {
			auto v = pendingData.fetch_sub(1);
			if (!success) {
				invalid = true;
				if (v == 1) {
					invalidate();
				}
			} else if (v == 1) {
				loadData();
			}
		}
	};

	bool hasController = (b.getTarget() != nullptr);
	auto builder = Rc<ControllerBuilder>::alloc(move(b));
	builder->looper = looper;
	builder->ext = this;
	if (!hasController) {
		builder->controller = Rc<FontController>::create(this, builder->builder.getName());
	}

	builder->pendingData = builder->builder.getDataQueries().size();

	for (auto &it : builder->builder.getDataQueries()) {
		looper->performAsync(
				[this, name = it.first, sourcePtr = &it.second, builder]() mutable -> bool {
			sourcePtr->data = _library->openFontData(name, sourcePtr->params,
					sourcePtr->preconfiguredParams, [&]() -> FontLibrary::FontData {
				if (sourcePtr->fontCallback) {
					return FontLibrary::FontData(sp::move(sourcePtr->fontCallback));
				} else if (!sourcePtr->fontExternalData.empty()) {
					return FontLibrary::FontData(sourcePtr->fontExternalData, true);
				} else if (!sourcePtr->fontMemoryData.empty()) {
					return FontLibrary::FontData(sp::move(sourcePtr->fontMemoryData));
				} else if (!sourcePtr->fontFilePath.empty()) {
					auto d = filesystem::readIntoMemory<Interface>(
							FileInfo{sourcePtr->fontFilePath});
					if (!d.empty()) {
						return FontLibrary::FontData(sp::move(d));
					}
				}
				return FontLibrary::FontData(BytesView(), false);
			});
			if (sourcePtr->data) {
				builder->onDataLoaded(true);
			} else {
				builder->onDataLoaded(false);
			}
			return true;
		});
	}

	return builder->controller;
}

void FontComponent::updateImage(event::Looper *looper, const Rc<core::DynamicImage> &image,
		Vector<font::FontUpdateRequest> &&data, Rc<core::DependencyEvent> &&dep,
		Function<void(bool)> &&complete) {
	if (!_active || !_queue) {
		_pendingImageQueries.emplace_back(ImageQuery{
			looper,
			image,
			sp::move(data),
			sp::move(dep),
			sp::move(complete),
		});
		return;
	}

	auto input = Rc<RenderFontInput>::alloc();
	input->queue = looper;
	input->image = image;
	input->ext = this;
	input->requests = sp::move(data);
	//input->output = [](const core::ImageInfoData &info, BytesView data) {
	//	Bitmap bmp(data, info.extent.width, info.extent.height, bitmap::PixelFormat::A8);
	//	bmp.save(bitmap::FileFormat::Png, FileInfo(toString(Time::now().toMicros(), ".png")));
	//};

	auto req = Rc<core::FrameRequest>::create(_queue);
	if (dep) {
		req->addSignalDependency(sp::move(dep));
	}

	for (auto &it : _queue->getInputAttachments()) {
		req->addInput(it, sp::move(input));
		break;
	}

	_context->getGlLoop()->runRenderQueue(sp::move(req), 0, sp::move(complete));
}

void FontComponent::handleActivated() {
	_active = true;

	for (auto &it : _pendingImageQueries) {
		updateImage(it.looper, it.image, sp::move(it.chars), sp::move(it.dependency),
				sp::move(it.complete));
	}

	_pendingImageQueries.clear();
}

} // namespace stappler::xenolith::font
