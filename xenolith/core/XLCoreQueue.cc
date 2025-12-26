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

#include "XLCoreQueue.h"
#include "XLCoreAttachment.h"
#include "XLCoreQueueData.h"
#include "XLCoreResource.h"
#include "XLCoreDevice.h"
#include "XLCoreQueuePass.h"
#include "XLCoreTextureSet.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

template <typename T>
static T *Resource_conditionalInsert(HashTable<T *> &vec, StringView key, const Callback<T *()> &cb,
		memory::pool_t *pool);

static void Queue_buildLoadStore(QueueData *data) {
	for (auto &attachment : data->attachments) {
		if (attachment->type != AttachmentType::Image) {
			continue;
		}

		auto img = (ImageAttachment *)attachment->attachment.get();

		bool hasColor = false;
		bool hasStencil = false;
		switch (img->getImageInfo().format) {
		case ImageFormat::S8_UINT: hasStencil = true; break;
		case ImageFormat::D16_UNORM_S8_UINT:
		case ImageFormat::D24_UNORM_S8_UINT:
		case ImageFormat::D32_SFLOAT_S8_UINT:
			hasColor = true;
			hasStencil = true;
			break;
		default: hasColor = true; break;
		}

		for (auto &descriptor : attachment->passes) {
			if (descriptor->ops != AttachmentOps::Undefined) {
				// operations was hinted, no heuristics required
				continue;
			}

			AttachmentOps ops = AttachmentOps::Undefined;
			for (auto &it : descriptor->subpasses) {
				if (it->ops != AttachmentOps::Undefined) {
					ops |= it->ops;
					continue;
				}

				AttachmentOps refOps = AttachmentOps::Undefined;
				bool hasWriters = false;
				bool hasReaders = false;
				bool colorReadOnly = true;
				bool stencilReadOnly = true;

				if ((it->usage & AttachmentUsage::Output) != AttachmentUsage::None
						|| (it->usage & AttachmentUsage::Resolve) != AttachmentUsage::None
						|| (it->usage & AttachmentUsage::DepthStencil) != AttachmentUsage::None) {
					hasWriters = true;
				}
				if ((it->usage & AttachmentUsage::Input) != AttachmentUsage::None
						|| (it->usage & AttachmentUsage::DepthStencil) != AttachmentUsage::None) {
					hasReaders = true;
				}
				if ((it->usage & AttachmentUsage::DepthStencil) != AttachmentUsage::None) {
					switch (it->layout) {
					case AttachmentLayout::DepthStencilAttachmentOptimal:
					case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
					case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
					case AttachmentLayout::DepthAttachmentOptimal:
					case AttachmentLayout::StencilAttachmentOptimal:
					case AttachmentLayout::General: hasWriters = true; break;
					default: break;
					}
				}

				switch (it->layout) {
				case AttachmentLayout::General:
				case AttachmentLayout::DepthStencilAttachmentOptimal:
					stencilReadOnly = false;
					colorReadOnly = false;
					break;
				case AttachmentLayout::ColorAttachmentOptimal:
				case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
				case AttachmentLayout::DepthAttachmentOptimal: colorReadOnly = false; break;
				case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
				case AttachmentLayout::StencilAttachmentOptimal: stencilReadOnly = false; break;
				default: break;
				}

				if (hasWriters) {
					if (hasColor && !colorReadOnly) {
						refOps |= AttachmentOps::WritesColor;
					}
					if (hasStencil && !stencilReadOnly) {
						refOps |= AttachmentOps::WritesStencil;
					}
				}

				if (hasReaders) {
					if (hasColor) {
						refOps |= AttachmentOps::ReadColor;
					}
					if (hasStencil) {
						refOps |= AttachmentOps::ReadStencil;
					}
				}

				it->ops = refOps;
				ops |= refOps;
			}
			descriptor->ops = ops;
		}
	};

	auto dataWasWritten = [](AttachmentData *data, uint32_t idx) -> Pair<bool, bool> {
		if ((data->usage & AttachmentUsage::Input) != AttachmentUsage::None) {
			if ((data->ops & (AttachmentOps::WritesColor | AttachmentOps::WritesStencil))
					!= AttachmentOps::Undefined) {
				return pair(true, true);
			}
		}

		bool colorWasWritten = (data->ops & AttachmentOps::WritesColor) != AttachmentOps::Undefined;
		bool stencilWasWritten =
				(data->ops & AttachmentOps::WritesStencil) != AttachmentOps::Undefined;

		auto &descriptors = data->passes;
		for (size_t i = 0; i < idx; ++i) {
			auto desc = descriptors[i];
			if ((desc->ops & AttachmentOps::WritesColor) != AttachmentOps::Undefined) {
				colorWasWritten = true;
			}
			if ((desc->ops & AttachmentOps::WritesStencil) != AttachmentOps::Undefined) {
				stencilWasWritten = true;
			}
		}

		return pair(colorWasWritten, stencilWasWritten);
	};

	auto dataWillBeRead = [](AttachmentData *data, uint32_t idx) -> Pair<bool, bool> {
		if ((data->usage & AttachmentUsage::Output) != AttachmentUsage::None) {
			if ((data->ops & (AttachmentOps::ReadColor | AttachmentOps::ReadStencil))
					!= AttachmentOps::Undefined) {
				return pair(true, true);
			}
		}

		bool colorWillBeRead = (data->ops & AttachmentOps::ReadColor) != AttachmentOps::Undefined;
		bool stencilWillBeRead =
				(data->ops & AttachmentOps::ReadStencil) != AttachmentOps::Undefined;

		auto &descriptors = data->passes;
		for (size_t i = idx + 1; i < descriptors.size(); ++i) {
			auto desc = descriptors[i];
			if ((desc->ops & AttachmentOps::ReadColor) != AttachmentOps::Undefined) {
				colorWillBeRead = true;
			}
			if ((desc->ops & AttachmentOps::ReadStencil) != AttachmentOps::Undefined) {
				stencilWillBeRead = true;
			}
		}

		return pair(colorWillBeRead, stencilWillBeRead);
	};

	// fill layout chain
	for (auto &attachment : data->attachments) {
		if (attachment->passes.empty()) {
			continue;
		}

		if (attachment->passes.size() == 1 && attachment->usage == AttachmentUsage::None) {
			attachment->transient = true;

			if (attachment->type != AttachmentType::Image) {
				continue;
			}

			auto img = (ImageAttachment *)attachment->attachment.get();
			for (auto &desc : attachment->passes) {
				auto fmt = getImagePixelFormat(img->getImageInfo().format);
				switch (fmt) {
				case PixelFormat::DS:
				case PixelFormat::S:
					desc->loadOp = img->shouldClearOnLoad() ? AttachmentLoadOp::Clear
															: AttachmentLoadOp::DontCare;
					desc->stencilLoadOp = img->shouldClearOnLoad() ? AttachmentLoadOp::Clear
																   : AttachmentLoadOp::DontCare;
					desc->storeOp = AttachmentStoreOp::DontCare;
					desc->stencilStoreOp = AttachmentStoreOp::DontCare;
					break;
				default:
					desc->loadOp = img->shouldClearOnLoad() ? AttachmentLoadOp::Clear
															: AttachmentLoadOp::DontCare;
					desc->stencilLoadOp = img->shouldClearOnLoad() ? AttachmentLoadOp::Clear
																   : AttachmentLoadOp::DontCare;
					desc->storeOp = AttachmentStoreOp::DontCare;
					desc->stencilStoreOp = AttachmentStoreOp::DontCare;
					break;
				}
			}
		} else {
			if (attachment->type != AttachmentType::Image) {
				continue;
			}

			uint32_t descIndex = 0;
			for (auto &desc : attachment->passes) {
				auto wasWritten = dataWasWritten(attachment, descIndex); // Color, Stencil
				auto willBeRead = dataWillBeRead(attachment, descIndex);

				if (wasWritten.first) {
					if ((desc->ops & AttachmentOps::ReadColor) != AttachmentOps::Undefined) {
						desc->loadOp = AttachmentLoadOp::Load;
					} else {
						desc->loadOp = AttachmentLoadOp::DontCare;
					}
				} else {
					bool isRead =
							((desc->ops & AttachmentOps::ReadColor) != AttachmentOps::Undefined);
					bool isWrite =
							((desc->ops & AttachmentOps::WritesColor) != AttachmentOps::Undefined);
					if (isRead && !isWrite) {
						log::source().error("Gl-Error", "Attachment's color component '",
								attachment->key, "' is read in renderpass ", desc->pass->key,
								" before written");
					}

					auto img = (ImageAttachment *)attachment->attachment.get();
					desc->loadOp = img->shouldClearOnLoad() ? AttachmentLoadOp::Clear
															: AttachmentLoadOp::DontCare;
				}

				if (wasWritten.second) {
					if ((desc->ops & AttachmentOps::ReadStencil) != AttachmentOps::Undefined) {
						desc->stencilLoadOp = AttachmentLoadOp::Load;
					} else {
						desc->stencilLoadOp = AttachmentLoadOp::DontCare;
					}
				} else {
					bool isRead =
							((desc->ops & AttachmentOps::ReadStencil) != AttachmentOps::Undefined);
					bool isWrite = ((desc->ops & AttachmentOps::WritesStencil)
							!= AttachmentOps::Undefined);
					if (isRead && !isWrite) {
						log::source().error("Gl-Error", "Attachment's stencil component '",
								attachment->key, "' is read in renderpass ", desc->pass->key,
								" before written");
					}
					auto img = (ImageAttachment *)attachment->attachment.get();
					desc->stencilLoadOp = img->shouldClearOnLoad() ? AttachmentLoadOp::Clear
																   : AttachmentLoadOp::DontCare;
				}

				if (willBeRead.first) {
					if ((desc->ops & AttachmentOps::WritesColor) != AttachmentOps::Undefined) {
						desc->storeOp = AttachmentStoreOp::Store;
					} else {
						desc->storeOp = AttachmentStoreOp::DontCare;
					}
				} else {
					bool isRead =
							((desc->ops & AttachmentOps::ReadColor) != AttachmentOps::Undefined);
					bool isWrite =
							((desc->ops & AttachmentOps::WritesColor) != AttachmentOps::Undefined);
					if (!isRead && isWrite) {
						log::source().error("Gl-Error", "Attachment's color component '",
								attachment->key, "' is written in renderpass ", desc->pass->key,
								" but never read");
					}
					desc->storeOp = AttachmentStoreOp::DontCare;
				}

				if (willBeRead.second) {
					if ((desc->ops & AttachmentOps::WritesStencil) != AttachmentOps::Undefined) {
						desc->stencilStoreOp = AttachmentStoreOp::Store;
					} else {
						desc->stencilStoreOp = AttachmentStoreOp::DontCare;
					}
				} else {
					bool isRead =
							((desc->ops & AttachmentOps::ReadStencil) != AttachmentOps::Undefined);
					bool isWrite = ((desc->ops & AttachmentOps::WritesStencil)
							!= AttachmentOps::Undefined);
					if (!isRead && isWrite) {
						log::source().error("Gl-Error", "Attachment's stencil component '",
								attachment->key, "' is writen in renderpass ", desc->pass->key,
								" but never read");
					}
					desc->stencilStoreOp = AttachmentStoreOp::DontCare;
				}
			}
			++descIndex;
		}

		if (attachment->type != AttachmentType::Image) {
			continue;
		}

		auto img = (ImageAttachment *)attachment->attachment.get();
		AttachmentLayout layout = img->getInitialLayout();
		for (auto &desc : attachment->passes) {
			if (desc->initialLayout == AttachmentLayout::Ignored) {
				if (layout == AttachmentLayout::Ignored && !desc->subpasses.empty()) {
					desc->initialLayout = desc->subpasses.front()->layout;
				} else {
					desc->initialLayout = layout;
				}
			}
			if (desc->finalLayout == AttachmentLayout::Ignored) {
				if (!desc->subpasses.empty()) {
					layout = desc->subpasses.back()->layout;
					desc->finalLayout = layout;
				} else {
					desc->finalLayout = desc->initialLayout;
				}
			}
		}
		if (img->getFinalLayout() != AttachmentLayout::Ignored) {
			attachment->passes.back()->finalLayout = img->getFinalLayout();
		}
	}
}

static void Queue_buildDescriptors(QueueData *data, Device &dev) {
	for (auto &pass : data->passes) {
		if (pass->pass->getType() == PassType::Graphics) {
			for (auto &subpass : pass->subpasses) {
				for (auto a : subpass->outputImages) {
					if (a->pass->attachment->type == AttachmentType::Image) {
						auto desc = (ImageAttachment *)a->pass->attachment->attachment.get();
						desc->addImageUsage(ImageUsage::ColorAttachment);
					}
				}
				for (auto a : subpass->resolveImages) {
					if (a->pass->attachment->type == AttachmentType::Image) {
						auto desc = (ImageAttachment *)a->pass->attachment->attachment.get();
						desc->addImageUsage(ImageUsage::ColorAttachment);
					}
				}
				for (auto a : subpass->inputImages) {
					if (a->pass->attachment->type == AttachmentType::Image) {
						auto desc = (ImageAttachment *)a->pass->attachment->attachment.get();
						desc->addImageUsage(ImageUsage::InputAttachment);
					}
				}
				if (subpass->depthStencil) {
					if (subpass->depthStencil->pass->attachment->type == AttachmentType::Image) {
						auto desc =
								(ImageAttachment *)
										subpass->depthStencil->pass->attachment->attachment.get();
						desc->addImageUsage(ImageUsage::DepthStencilAttachment);
					}
				}
			}
		}

		for (auto &attachment : pass->attachments) {
			if (attachment->attachment->type == AttachmentType::Image) {
				auto desc = (ImageAttachment *)attachment->attachment->attachment.get();
				switch (attachment->finalLayout) {
				case AttachmentLayout::Undefined:
				case AttachmentLayout::General:
				case AttachmentLayout::ShaderReadOnlyOptimal:
				case AttachmentLayout::Preinitialized:
				case AttachmentLayout::Ignored: break;
				case AttachmentLayout::PresentSrc:
					// in alternative mode, images can be presented via transfer
					desc->addImageUsage(ImageUsage::TransferSrc);
					break;
				case AttachmentLayout::ColorAttachmentOptimal:
					desc->addImageUsage(ImageUsage::ColorAttachment);
					break;
				case AttachmentLayout::TransferSrcOptimal:
					desc->addImageUsage(ImageUsage::TransferSrc);
					break;
				case AttachmentLayout::TransferDstOptimal:
					desc->addImageUsage(ImageUsage::TransferDst);
					break;
				case AttachmentLayout::DepthStencilAttachmentOptimal:
				case AttachmentLayout::DepthStencilReadOnlyOptimal:
				case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
				case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
				case AttachmentLayout::DepthAttachmentOptimal:
				case AttachmentLayout::DepthReadOnlyOptimal:
				case AttachmentLayout::StencilAttachmentOptimal:
				case AttachmentLayout::StencilReadOnlyOptimal:
					desc->addImageUsage(ImageUsage::DepthStencilAttachment);
					break;
				}
			}
		}

		for (auto &layout : pass->pipelineLayouts) {
			for (auto &set : layout->sets) {
				for (auto &desc : set->descriptors) {
					if (desc->type != DescriptorType::Unknown) {
						auto flags = dev.getSupportedDescriptorFlags(desc->type);

						if (hasFlag(desc->requestFlags, DescriptorFlags::UpdateAfterBind)
								&& hasFlag(flags, DescriptorFlags::UpdateAfterBind)) {
							desc->deviceFlags |= core::DescriptorFlags::UpdateAfterBind;
							pass->hasUpdateAfterBind = true;
						}
						if (hasFlag(desc->requestFlags, DescriptorFlags::PartiallyBound)
								&& hasFlag(flags, DescriptorFlags::PartiallyBound)) {
							desc->deviceFlags |= core::DescriptorFlags::PartiallyBound;
						}
					}
				}
			}
		}
	}
}

static void Queue_addRequiredPass(QueuePassData &pass, const QueuePassData &required,
		const AttachmentData &attachment, const AttachmentPassData &desc,
		FrameRenderPassState defaultSync) {
	auto requiredState = (desc.dependency.requiredRenderPassState == FrameRenderPassState::Initial)
			? defaultSync
			: desc.dependency.requiredRenderPassState;
	auto lockedState = desc.dependency.lockedRenderPassState;
	if (requiredState == FrameRenderPassState::Initial) {
		return;
	}

	auto lb = std::lower_bound(pass.required.begin(), pass.required.end(), &required,
			[&](const QueuePassRequirements &l, const QueuePassData *r) { return l.data < r; });
	if (lb == pass.required.end()) {
		pass.required.emplace_back(QueuePassRequirements(required, requiredState, lockedState));
	} else if (lb->data != &required) {
		pass.required.emplace(lb, QueuePassRequirements(required, requiredState, lockedState));
	} else {
		lb->requiredState =
				FrameRenderPassState(std::max(toInt(lb->requiredState), toInt(requiredState)));
		lb->lockedState =
				FrameRenderPassState(std::min(toInt(lb->lockedState), toInt(lockedState)));
	}
}

static void Queue_addDirectDependency(QueueData *data, const AttachmentPassData &source,
		const AttachmentPassData &target) {
	if (target.dependency.initialUsageStage == PipelineStage::None) {
		// no pipeline stage specified for synchronization
		return;
	}

	for (auto &it : data->passDependencies) {
		if (it.source == source.pass && it.target == target.pass) {
			it.attachments.emplace_back(source.attachment);
			if (target.dependency.initialUsageStage != PipelineStage::None) {
				it.stageFlags |= target.dependency.initialUsageStage;
			}
			return;
		}
	}

	auto &it = data->passDependencies.emplace_back(QueuePassDependency{source.pass, target.pass,
		memory::vector<const AttachmentData *>{source.attachment},
		target.dependency.initialUsageStage});

	const_cast<QueuePassData *>(it.source)->sourceQueueDependencies.emplace_back(&it);
	const_cast<QueuePassData *>(it.target)->targetQueueDependencies.emplace_back(&it);
}

static void Queue_buildRequirements(QueueData *data, Device &dev) {
	for (auto &passIt : data->passes) {
		for (auto &a : passIt->attachments) {
			auto &desc = a->attachment->passes;
			auto it = desc.begin();
			while (it != desc.end() && (*it)->pass != passIt) {
				Queue_addRequiredPass(*passIt, *(*it)->pass, *a->attachment, **it,
						data->defaultSyncPassState);

				if ((*(it + 1))->pass == passIt) {
					// direct dependency
					Queue_addDirectDependency(data, *(*it), **(it + 1));
				}

				++it;
			}
		}
	}
}

static void Queue_updateLayout(AttachmentSubpassData *attachemnt, Device &dev) {
	if (attachemnt->pass->attachment->type != AttachmentType::Image) {
		return;
	}

	auto a = (ImageAttachment *)attachemnt->pass->attachment->attachment.get();

	auto fmt = a->getImageInfo().format;

	bool separateDepthStencil = false;
	bool hasColor = false;
	bool hasDepth = false;
	bool hasStencil = false;

	switch (fmt) {
	case ImageFormat::D16_UNORM:
	case ImageFormat::X8_D24_UNORM_PACK32:
	case ImageFormat::D32_SFLOAT: hasDepth = true; break;
	case ImageFormat::S8_UINT: hasStencil = true; break;
	case ImageFormat::D16_UNORM_S8_UINT:
	case ImageFormat::D24_UNORM_S8_UINT:
	case ImageFormat::D32_SFLOAT_S8_UINT:
		hasDepth = true;
		hasStencil = true;
		break;
	default: hasColor = true; break;
	}

	switch (attachemnt->usage) {
	case AttachmentUsage::Input:
		switch (attachemnt->layout) {
		case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
		case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
		case AttachmentLayout::DepthReadOnlyOptimal:
		case AttachmentLayout::StencilReadOnlyOptimal:
		case AttachmentLayout::DepthStencilReadOnlyOptimal:
		case AttachmentLayout::ShaderReadOnlyOptimal:
		case AttachmentLayout::General: break;
		case AttachmentLayout::Ignored:
			if (hasColor) {
				attachemnt->layout = AttachmentLayout::ShaderReadOnlyOptimal;
			} else if ((!separateDepthStencil && (hasDepth || hasStencil))
					|| (hasDepth && hasStencil)) {
				attachemnt->layout = AttachmentLayout::DepthStencilReadOnlyOptimal;
			} else if (hasDepth) {
				attachemnt->layout = AttachmentLayout::DepthReadOnlyOptimal;
			} else if (hasStencil) {
				attachemnt->layout = AttachmentLayout::StencilReadOnlyOptimal;
			} else {
				attachemnt->layout = AttachmentLayout::General;
			}
			break;
		default:
			log::source().error("Gl-Error", "Invalid layout for attachment '", attachemnt->key,
					"' in renderpass ", attachemnt->pass->pass->key, ":",
					attachemnt->subpass->index);
			break;
		}
		break;
	case AttachmentUsage::Output:
	case AttachmentUsage::Resolve:
		switch (attachemnt->layout) {
		case AttachmentLayout::ColorAttachmentOptimal:
		case AttachmentLayout::General: break;
		case AttachmentLayout::Ignored:
			attachemnt->layout = AttachmentLayout::ColorAttachmentOptimal;
			break;
		default:
			log::source().error("Gl-Error", "Invalid layout for attachment '", attachemnt->key,
					"' in renderpass ", attachemnt->pass->pass->key, ":",
					attachemnt->subpass->index);
			break;
		}
		break;
	case AttachmentUsage::InputOutput:
		switch (attachemnt->layout) {
		case AttachmentLayout::General: break;
		case AttachmentLayout::Ignored: attachemnt->layout = AttachmentLayout::General; break;
		default:
			log::source().error("Gl-Error", "Invalid layout for attachment '", attachemnt->key,
					"' in renderpass ", attachemnt->pass->pass->key, ":",
					attachemnt->subpass->index);
			break;
		}
		break;
	case AttachmentUsage::DepthStencil:
		switch (attachemnt->layout) {
		case AttachmentLayout::DepthStencilAttachmentOptimal:
		case AttachmentLayout::DepthAttachmentOptimal:
		case AttachmentLayout::DepthReadOnlyOptimal:
		case AttachmentLayout::StencilAttachmentOptimal:
		case AttachmentLayout::StencilReadOnlyOptimal:
		case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
		case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
		case AttachmentLayout::DepthStencilReadOnlyOptimal:
		case AttachmentLayout::General: break;
		case AttachmentLayout::Ignored:
			if ((!separateDepthStencil && (hasDepth || hasStencil)) || (hasDepth && hasStencil)) {
				attachemnt->layout = AttachmentLayout::DepthStencilAttachmentOptimal;
			} else if (hasDepth) {
				attachemnt->layout = AttachmentLayout::DepthAttachmentOptimal;
			} else if (hasStencil) {
				attachemnt->layout = AttachmentLayout::StencilAttachmentOptimal;
			} else {
				attachemnt->layout = AttachmentLayout::General;
			}
			break;
		default:
			log::source().error("Gl-Error", "Invalid layout for attachment '", attachemnt->key,
					"' in renderpass ", attachemnt->pass->pass->key, ":",
					attachemnt->subpass->index);
			break;
		}
		break;
	case AttachmentUsage::Input | AttachmentUsage::DepthStencil:
		switch (attachemnt->layout) {
		case AttachmentLayout::DepthReadOnlyStencilAttachmentOptimal:
		case AttachmentLayout::DepthAttachmentStencilReadOnlyOptimal:
		case AttachmentLayout::DepthStencilReadOnlyOptimal:
		case AttachmentLayout::General: break;
		case AttachmentLayout::Ignored: attachemnt->layout = AttachmentLayout::General; break;
		default:
			log::source().error("Gl-Error", "Invalid layout for attachment '", attachemnt->key,
					"' in renderpass ", attachemnt->pass->pass->key, ":",
					attachemnt->subpass->index);
			break;
		}
		break;
	default:
		log::source().error("Gl-Error", "Invalid usage for attachment '", attachemnt->key,
				"' in renderpass ", attachemnt->pass->pass->key, ":", attachemnt->subpass->index);
		break;
	}
}

static void Queue_sortRefs(AttachmentPassData *attachemnt, Device &dev) {
	std::sort(attachemnt->subpasses.begin(), attachemnt->subpasses.end(),
			[&](const AttachmentSubpassData *l, const AttachmentSubpassData *r) {
		return l->subpass->index < r->subpass->index;
	});

	for (auto &it : attachemnt->subpasses) {
		Queue_updateLayout(it, dev);

		attachemnt->dependency.requiredRenderPassState =
				FrameRenderPassState(std::max(toInt(attachemnt->dependency.requiredRenderPassState),
						toInt(it->dependency.requiredRenderPassState)));
	}

	if (!attachemnt->subpasses.empty()) {
		attachemnt->dependency.initialUsageStage =
				attachemnt->subpasses.front()->dependency.initialUsageStage;
		attachemnt->dependency.initialAccessMask =
				attachemnt->subpasses.front()->dependency.initialAccessMask;
		attachemnt->dependency.finalUsageStage =
				attachemnt->subpasses.back()->dependency.finalUsageStage;
		attachemnt->dependency.finalAccessMask =
				attachemnt->subpasses.back()->dependency.finalAccessMask;
	}
}

static void Queue_sortDescriptors(AttachmentData *attachemnt, Device &dev) {
	Set<uint32_t> priorities;

	for (auto &it : attachemnt->passes) {
		auto pass = it->pass;
		auto iit = priorities.find(pass->ordering.get());
		if (iit == priorities.end()) {
			priorities.emplace(pass->ordering.get());
		} else {
			log::source().error("Gl-Error", "Duplicate render pass priority '",
					pass->ordering.get(), "' for attachment '", attachemnt->key,
					"', render ordering can be invalid");
		}
	}

	std::sort(attachemnt->passes.begin(), attachemnt->passes.end(),
			[&](const AttachmentPassData *l, const AttachmentPassData *r) {
		return l->pass->ordering < r->pass->ordering;
	});

	for (auto &it : attachemnt->passes) { Queue_sortRefs(it, dev); }
}

static void Queue_validateShaderPipelineLayout(StringView pipelineName,
		const PipelineLayoutData *layout, const ProgramInfo *info) {
	bool hasTexturesArray = false;
	bool hasSamplersArray = false;
	bool hasAtlasArray = false;
	for (auto &binding : info->bindings) {
		auto set = binding.set;
		auto desc = binding.descriptor;

		if (set < layout->sets.size()) {
			auto s = layout->sets[set];
			if (desc < s->descriptors.size()) {
				auto d = s->descriptors[desc];

				if (d->type == DescriptorType::Unknown) {
					d->type = binding.type;
				} else if (d->type != binding.type) {
					log::source().warn("renderqueue::Queue", "[", layout->key, ":", pipelineName,
							":", set, ":", desc, "] descriptor type conflict: (code)",
							getDescriptorTypeName(d->type), " vs. (shader)",
							getDescriptorTypeName(binding.type));
				}
				d->stages |= info->stage;
				if (!hasFlag(d->requestFlags, DescriptorFlags::PredefinedCount)) {
					if (binding.count < maxOf<uint32_t>()) {
						d->count = std::max(binding.count, d->count);
					}
				} else if (binding.count < maxOf<uint32_t>() && binding.count > d->count) {
					log::source().warn("renderqueue::Queue", "[", layout->key, ":", pipelineName,
							":", set, ":", desc, "] descriptor requires ", binding.count,
							" objects, but only ", d->count, "defined with addDescriptorArray");
				}
			} else {
				log::source().warn("renderqueue::Queue", "[", layout->key, ":", pipelineName, ":",
						set, ":", desc, "] descriptor target not found");
			}
		} else {
			if (desc == 0 && binding.type == DescriptorType::Sampler) {
				hasTexturesArray = true;
			} else if (desc == 1 && binding.type == DescriptorType::SampledImage) {
				hasSamplersArray = true;
			} else if (desc == 2 && binding.type == DescriptorType::StorageBuffer) {
				hasAtlasArray = true;
			} else {
				log::source().warn("renderqueue::Queue", "[", layout->key, ":", pipelineName, ":",
						set, ":", desc, "] descriptor set not found");
			}
		}
	}

	if (hasTexturesArray || hasSamplersArray || hasAtlasArray) {
		if (layout->textureSetLayout == nullptr) {
			log::source().warn("renderqueue::Queue", "[", layout->key, ":", pipelineName,
					"] uses TextureSetLayout, that was not defined");
		}
	}
}

Queue::Queue() { }

Queue::~Queue() {
	if (_data) {
		_data->clear();
		auto p = _data->pool;
		_data->~QueueData();
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

bool Queue::init(Builder &&buf) {
	Rc<Resource> res;
	if (!buf._internalResource.empty()) {
		res = Rc<Resource>::create(move(buf._internalResource));
	}
	if (buf._data) {
		_data = buf._data;
		_data->queue = this;
		buf._data = nullptr;

		for (auto &it : _data->passes) { it->pass->_data = it; }

		if (res) {
			_data->resource = res;
			_data->resource->setOwner(this);
		}

		return true;
	}
	return false;
}

bool Queue::isCompiled() const { return _data->compiled; }

void Queue::setCompiled(Device &dev, Function<void()> &&cb) {
	_data->compiled = true;

	for (auto &attachment : _data->attachments) { attachment->attachment->setCompiled(dev); }

	_data->releaseCallback = sp::move(cb);
}

bool Queue::isCompatible(const ImageInfo &info) const {
	if (_data && _data->output.size() == 1) {
		auto out = _data->output.front();
		if (out->type == AttachmentType::Image) {
			return out->attachment->isCompatible(info);
		}
	}
	return false;
}

StringView Queue::getName() const { return _data->key; }

FrameRenderPassState Queue::getDefaultSyncPassState() const { return _data->defaultSyncPassState; }

const HashTable<ProgramData *> &Queue::getPrograms() const { return _data->programs; }

const HashTable<QueuePassData *> &Queue::getPasses() const { return _data->passes; }

const HashTable<GraphicPipelineData *> &Queue::getGraphicPipelines() const {
	return _data->graphicPipelines;
}

const HashTable<ComputePipelineData *> &Queue::getComputePipelines() const {
	return _data->computePipelines;
}

const HashTable<AttachmentData *> &Queue::getAttachments() const { return _data->attachments; }

const HashTable<TextureSetLayoutData *> &Queue::getTextureSetLayouts() const {
	return _data->textureSets;
}

const HashTable<Rc<Resource>> &Queue::getLinkedResources() const { return _data->linked; }

Rc<Resource> Queue::getInternalResource() const { return _data->resource; }

const memory::vector<AttachmentData *> &Queue::getInputAttachments() const { return _data->input; }

const memory::vector<AttachmentData *> &Queue::getOutputAttachments() const {
	return _data->output;
}

const Attachment *Queue::getInputAttachment(std::type_index name) const {
	auto it = _data->typedInput.find(name);
	if (it != _data->typedInput.end()) {
		return it->second;
	}
	return nullptr;
}

const Attachment *Queue::getOutputAttachment(std::type_index name) const {
	auto it = _data->typedOutput.find(name);
	if (it != _data->typedOutput.end()) {
		return it->second;
	}
	return nullptr;
}

const QueuePassData *Queue::getPass(StringView key) const { return _data->passes.get(key); }

const ProgramData *Queue::getProgram(StringView key) const { return _data->programs.get(key); }

const GraphicPipelineData *Queue::getGraphicPipeline(StringView key) const {
	return _data->graphicPipelines.get(key);
}

const ComputePipelineData *Queue::getComputePipeline(StringView key) const {
	return _data->computePipelines.get(key);
}

const AttachmentData *Queue::getAttachment(StringView key) const {
	return _data->attachments.get(key);
}

Vector<AttachmentData *> Queue::getOutput() const {
	Vector<AttachmentData *> ret;
	ret.reserve(_data->output.size());
	for (auto &it : _data->output) { ret.emplace_back(it); }
	return ret;
}

Vector<AttachmentData *> Queue::getOutput(AttachmentType t) const {
	Vector<AttachmentData *> ret;
	for (auto &it : _data->output) {
		if (it->type == t) {
			ret.emplace_back(it);
		}
	}
	return ret;
}

AttachmentData *Queue::getPresentImageOutput() const {
	for (auto &it : _data->output) {
		if (it->type == AttachmentType::Image) {
			auto img = (ImageAttachment *)it->attachment.get();
			if (img->getFinalLayout() == AttachmentLayout::PresentSrc) {
				return it;
			}
		}
	}
	return nullptr;
}

AttachmentData *Queue::getTransferImageOutput() const {
	for (auto &it : _data->output) {
		if (it->type == AttachmentType::Image) {
			auto img = (ImageAttachment *)it->attachment.get();
			if (img->getFinalLayout() == AttachmentLayout::TransferSrcOptimal) {
				return it;
			}
		}
	}
	return nullptr;
}

uint64_t Queue::incrementOrder() {
	auto ret = _data->order;
	++_data->order;
	return ret;
}

bool Queue::prepare(Device &dev) {
	memory::context ctx(_data->pool);

	for (auto &it : _data->programs) {
		if (!it->data.empty()) {
			it->inspect(it->data);
		} else if (it->callback) {
			it->callback(dev, [&](SpanView<uint32_t> data) { it->inspect(data); });
		}
	}

	for (auto &pass : _data->passes) {
		for (auto &subpass : pass->subpasses) {
			for (auto &pipeline : subpass->graphicPipelines) {
				for (auto &shaderSpec : pipeline->shaders) {
					Queue_validateShaderPipelineLayout(pipeline->key, pipeline->layout,
							shaderSpec.data);
				}
			}

			for (auto &pipeline : subpass->computePipelines) {
				Queue_validateShaderPipelineLayout(pipeline->key, pipeline->layout,
						pipeline->shader.data);
			}
		}
	}

	for (auto &it : _data->input) {
		auto &r = *it->attachment.get();
		_data->typedInput.emplace(std::type_index(typeid(r)), it->attachment.get());
	}

	for (auto &it : _data->output) {
		auto &r = *it->attachment.get();
		_data->typedOutput.emplace(std::type_index(typeid(r)), it->attachment.get());
	}

	// fill attachment descriptors
	for (auto &attachment : _data->attachments) { Queue_sortDescriptors(attachment, dev); }

	Queue_buildLoadStore(_data);
	Queue_buildDescriptors(_data, dev);

	for (auto &it : _data->passes) { it->pass->prepare(dev); }

	Queue_buildRequirements(_data, dev);

	return true;
}

void Queue::beginFrame(FrameRequest &frame) {
	if (_data->beginCallback) {
		_data->beginCallback(frame);
	}
}

void Queue::endFrame(FrameRequest &frame) {
	if (_data->endCallback) {
		_data->endCallback(frame);
	}
}

void Queue::attachFrame(FrameHandle *frame) {
	if (_data->attachCallback) {
		_data->attachCallback(frame);
	}
}

void Queue::detachFrame(FrameHandle *frame) {
	if (_data->detachCallback) {
		_data->detachCallback(frame);
	}
}

void Queue::describe(const Callback<void(StringView)> &out) {
	out << "Queue '" << getName() << "':\nInput attachments:\n";
	for (auto &it : _data->input) {
		auto aPtr = it->attachment.get();
		out << "\t" << it->key << " (" << typeid(*aPtr) << ")\n";
	}
	out << "Output attachments:\n";
	for (auto &it : _data->output) {
		auto aPtr = it->attachment.get();
		out << "\t" << it->key << " (" << typeid(*aPtr) << ")\n";
	}

	out << "Attachments:\n";
	for (auto &it : _data->attachments) {
		auto aPtr = it->attachment.get();
		out << "\t" << it->key << " (" << typeid(*aPtr) << "):\n";
		for (auto &iit : it->passes) {
			out << "\t\t" << iit->key << " for pass '" << iit->pass->key << "':\n";
			for (auto &sub : iit->subpasses) {
				out << "\t\t\t" << sub->key << " for subpass '" << sub->subpass->key << "' ["
					<< sub->subpass->index << "]\n";
			}
		}
	}
	out << "Passes:\n";
	for (auto &it : _data->passes) {
		auto passPtr = it->pass.get();
		out << "\t" << it->key << " (" << typeid(*passPtr) << "):\n";
		for (auto &sub : it->subpasses) {
			out << "\t\t" << it->key << " [" << sub->index << "]\n";
			if (!sub->graphicPipelines.empty()) {
				out << "\t\t\tGraphicPipelines:\n";
				for (auto &p : sub->graphicPipelines) {
					out << "\t\t\t\t" << p->key << " with layout: " << p->layout->key << "\n";
				}
			}
			if (!sub->computePipelines.empty()) {
				out << "\t\t\tComputePipelines:\n";
				for (auto &p : sub->computePipelines) {
					out << "\t\t\t\t" << p->key << " with layout: " << p->layout->key << "\n";
				}
			}
			if (!sub->inputImages.empty()) {
				out << "\t\t\tInputImages:\n";
				for (auto &p : sub->inputImages) { out << "\t\t\t\t" << p->key << "\n"; }
			}
			if (!sub->outputImages.empty()) {
				out << "\t\t\tOutputImages:\n";
				for (auto &p : sub->outputImages) { out << "\t\t\t\t" << p->key << "\n"; }
			}
			if (!sub->resolveImages.empty()) {
				out << "\t\t\tResolveImages:\n";
				for (auto &p : sub->resolveImages) { out << "\t\t\t\t" << p->key << "\n"; }
			}
			if (sub->depthStencil) {
				out << "\t\t\tDepthStencil:\n";
				out << "\t\t\t\t" << sub->depthStencil->key << "\n";
			}
		}
		if (!it->pipelineLayouts.empty()) {
			out << "\tPipelineLayout:\n";
			for (auto &l : it->pipelineLayouts) {
				out << "\t\t" << l->key << ":\n";
				if (l->textureSetLayout) {
					out << "\t\t\t[tex]: samplers: " << l->textureSetLayout->samplers.size()
						<< " images: " << l->textureSetLayout->imageCount << "/"
						<< l->textureSetLayout->imageCountIndexed
						<< " buffers: " << l->textureSetLayout->bufferCount << "/"
						<< l->textureSetLayout->bufferCountIndexed << "\n";
				}
				for (auto &s : l->sets) {
					out << "\t\t\t[" << s->index << "]:\n";
					for (auto &d : s->descriptors) {
						out << "\t\t\t\t[" << d->index << "] " << d->type << "[" << d->count
							<< "]\n";
					}
				}
			}
		}
	}
}

void AttachmentBuilder::setType(AttachmentType type) { _data->type = type; }

void AttachmentBuilder::defineAsInput(AttachmentOps ops) {
	_data->usage |= AttachmentUsage::Input;
	_data->ops |= ops;

	((QueueData *)_data->queue)->input.emplace_back(_data);
}

void AttachmentBuilder::defineAsOutput(AttachmentOps ops, FrameRenderPassState pass) {
	_data->usage |= AttachmentUsage::Output;
	_data->ops |= ops;
	_data->outputState = pass;

	((QueueData *)_data->queue)->output.emplace_back(_data);
}

void AttachmentBuilder::defineAsOutput(FrameRenderPassState pass) {
	defineAsOutput(AttachmentOps::ReadColor | AttachmentOps::ReadStencil, pass);
}


void AttachmentBuilder::setInputAcquisitionCallback(AttachmentData::InputAcquisitionCallback &&cb) {
	_data->inputAcquisitionCallback = sp::move(cb);
}

void AttachmentBuilder::setInputSubmissionCallback(AttachmentData::InputSubmissionCallback &&cb) {
	_data->inputSubmissionCallback = sp::move(cb);
}

void AttachmentBuilder::setInputValidationCallback(AttachmentData::InputValidationCallback &&cb) {
	_data->inputValidationCallback = sp::move(cb);
}

AttachmentBuilder::AttachmentBuilder(AttachmentData *data) : _data(data) { }

void AttachmentPassBuilder::setAttachmentOps(AttachmentOps ops) { _data->ops = ops; }

void AttachmentPassBuilder::setInitialLayout(AttachmentLayout l) { _data->initialLayout = l; }

void AttachmentPassBuilder::setFinalLayout(AttachmentLayout l) { _data->finalLayout = l; }

void AttachmentPassBuilder::setLoadOp(AttachmentLoadOp op) { _data->loadOp = op; }

void AttachmentPassBuilder::setStoreOp(AttachmentStoreOp op) { _data->storeOp = op; }

void AttachmentPassBuilder::setStencilLoadOp(AttachmentLoadOp op) { _data->stencilLoadOp = op; }

void AttachmentPassBuilder::setStencilStoreOp(AttachmentStoreOp op) { _data->stencilStoreOp = op; }

void AttachmentPassBuilder::setColorMode(const ColorMode &mode) { _data->colorMode = mode; }

void AttachmentPassBuilder::setDependency(const AttachmentDependencyInfo &dep) {
	_data->dependency = dep;
}

AttachmentPassBuilder::AttachmentPassBuilder(AttachmentPassData *data) : _data(data) { }

bool DescriptorSetBuilder::addDescriptor(const AttachmentPassData *attachment, DescriptorType type,
		AttachmentLayout layout) {
	auto pool = _data->layout->pass->queue->pool;
	memory::context ctx(pool);

	auto p = new (pool) PipelineDescriptor;
	p->key = attachment->key;
	p->set = _data;
	p->attachment = attachment;
	p->type = type;
	p->layout = layout;
	p->count = 1;
	p->index = uint32_t(_data->descriptors.size());

	_data->descriptors.emplace_back(p);
	((AttachmentPassData *)attachment)->descriptors.emplace_back(p);

	return true;
}

bool DescriptorSetBuilder::addDescriptor(const AttachmentPassData *attachment,
		DescriptorFlags flags, DescriptorType type, AttachmentLayout layout) {
	auto pool = _data->layout->pass->queue->pool;
	memory::context ctx(pool);

	auto p = new (pool) PipelineDescriptor;
	p->key = attachment->key;
	p->set = _data;
	p->attachment = attachment;
	p->type = type;
	p->layout = layout;
	p->count = 1;
	p->requestFlags = flags;
	p->index = uint32_t(_data->descriptors.size());

	_data->descriptors.emplace_back(p);
	((AttachmentPassData *)attachment)->descriptors.emplace_back(p);

	return true;
}

bool DescriptorSetBuilder::addDescriptorArray(const AttachmentPassData *attachment, uint32_t count,
		DescriptorFlags flags, DescriptorType type, AttachmentLayout layout) {
	auto pool = _data->layout->pass->queue->pool;
	memory::context ctx(pool);

	auto p = new (pool) PipelineDescriptor;
	p->key = attachment->key;
	p->set = _data;
	p->attachment = attachment;
	p->type = type;
	p->layout = layout;
	p->count = count;
	p->index = uint32_t(_data->descriptors.size());
	p->requestFlags = flags | DescriptorFlags::PredefinedCount;

	_data->descriptors.emplace_back(p);
	((AttachmentPassData *)attachment)->descriptors.emplace_back(p);

	return true;
}

bool DescriptorSetBuilder::addDescriptorArray(const AttachmentPassData *attachment, uint32_t count,
		DescriptorType type, AttachmentLayout layout) {
	auto pool = _data->layout->pass->queue->pool;
	memory::context ctx(pool);

	auto p = new (pool) PipelineDescriptor;
	p->key = attachment->key;
	p->set = _data;
	p->attachment = attachment;
	p->type = type;
	p->layout = layout;
	p->count = count;
	p->index = uint32_t(_data->descriptors.size());
	p->requestFlags |= DescriptorFlags::PredefinedCount;

	_data->descriptors.emplace_back(p);
	((AttachmentPassData *)attachment)->descriptors.emplace_back(p);

	return true;
}

DescriptorSetBuilder::DescriptorSetBuilder(DescriptorSetData *data) : _data(data) { }

bool PipelineLayoutBuilder::addSet(const Callback<void(DescriptorSetBuilder &)> &cb) {
	auto pool = _data->pass->queue->pool;
	memory::context ctx(pool);

	auto s = new (pool) DescriptorSetData;
	s->key = _data->key;
	s->layout = _data;
	s->index = uint32_t(_data->sets.size());

	DescriptorSetBuilder builder(s);
	cb(builder);

	_data->sets.emplace_back(s);

	return true;
}

void PipelineLayoutBuilder::setTextureSetLayout(const TextureSetLayoutData *d) {
	_data->textureSetLayout = d;
	const_cast<TextureSetLayoutData *>(_data->textureSetLayout)->bindingLayouts.emplace_back(_data);
}

const PipelineFamilyData *PipelineLayoutBuilder::addPipelineFamily(StringView key) {
	auto pool = _data->pass->queue->pool;
	auto family = new (pool) PipelineFamilyData;
	family->key = key.pdup(pool);
	family->layout = _data;
	_data->families.emplace_back(family);
	return family;
}

PipelineLayoutBuilder::PipelineLayoutBuilder(PipelineLayoutData *data) : _data(data) { }

bool SubpassBuilder::addColor(const AttachmentPassData *attachment,
		AttachmentDependencyInfo dependency, AttachmentLayout layout, AttachmentOps ops,
		BlendInfo blendInfo) {
	auto pool = _data->pass->queue->pool;
	memory::context ctx(pool);

	auto a = new (pool) AttachmentSubpassData;
	a->key = attachment->key;
	a->pass = attachment;
	a->subpass = _data;
	a->layout = layout;
	a->dependency = dependency;
	a->usage = AttachmentUsage::Output;
	a->ops = ops;
	a->blendInfo = blendInfo;

	_data->outputImages.emplace_back(a);
	((AttachmentPassData *)attachment)->subpasses.emplace_back(a);

	return true;
}

bool SubpassBuilder::addColor(const AttachmentPassData *attachment,
		AttachmentDependencyInfo dependency, BlendInfo blendInfo) {
	auto pool = _data->pass->queue->pool;
	memory::context ctx(pool);

	auto a = new (pool) AttachmentSubpassData;
	a->key = attachment->key;
	a->pass = attachment;
	a->subpass = _data;
	a->dependency = dependency;
	a->usage = AttachmentUsage::Output;
	a->blendInfo = blendInfo;

	_data->outputImages.emplace_back(a);
	((AttachmentPassData *)attachment)->subpasses.emplace_back(a);

	return true;
}

bool SubpassBuilder::addInput(const AttachmentPassData *attachment,
		AttachmentDependencyInfo dependency, AttachmentLayout layout, AttachmentOps ops) {
	auto pool = _data->pass->queue->pool;
	memory::context ctx(pool);

	auto a = new (pool) AttachmentSubpassData;
	a->key = attachment->key;
	a->pass = attachment;
	a->subpass = _data;
	a->layout = layout;
	a->dependency = dependency;
	a->usage = AttachmentUsage::Input;
	a->ops = ops;

	_data->inputImages.emplace_back(a);
	((AttachmentPassData *)attachment)->subpasses.emplace_back(a);

	return true;
}

bool SubpassBuilder::addResolve(const AttachmentPassData *color, const AttachmentPassData *resolve,
		AttachmentDependencyInfo colorDep, AttachmentDependencyInfo resolveDep) {
	auto pool = _data->pass->queue->pool;
	memory::context ctx(pool);

	auto a = new (pool) AttachmentSubpassData;
	a->key = color->key;
	a->pass = color;
	a->subpass = _data;
	a->dependency = colorDep;
	a->usage = AttachmentUsage::Output;

	_data->outputImages.emplace_back(a);
	_data->resolveImages.resize(_data->outputImages.size() - 1, nullptr);
	((AttachmentPassData *)color)->subpasses.emplace_back(a);

	auto res = new (pool) AttachmentSubpassData;
	res->key = resolve->key;
	res->pass = resolve;
	res->subpass = _data;
	res->dependency = resolveDep;
	res->usage = AttachmentUsage::Resolve;

	_data->resolveImages.emplace_back(res);
	((AttachmentPassData *)resolve)->subpasses.emplace_back(res);

	return true;
}

bool SubpassBuilder::setDepthStencil(const AttachmentPassData *attachment,
		AttachmentDependencyInfo dependency, AttachmentLayout layout, AttachmentOps ops) {
	auto pool = _data->pass->queue->pool;
	memory::context ctx(pool);

	auto a = new (pool) AttachmentSubpassData;
	a->key = attachment->key;
	a->pass = attachment;
	a->subpass = _data;
	a->layout = layout;
	a->dependency = dependency;
	a->usage = AttachmentUsage::DepthStencil;
	a->ops = ops;

	_data->depthStencil = a;
	((AttachmentPassData *)attachment)->subpasses.emplace_back(a);

	return true;
}

const ComputePipelineData *SubpassBuilder::addComputePipeline(StringView key,
		const PipelineFamilyData *family, SpecializationInfo &&spec) {
	auto it = _data->computePipelines.find(key);
	if (it != _data->computePipelines.end()) {
		log::source().error("Resource", _data->key, ": Pipeline '", key, "' already added");
		return nullptr;
	}

	auto pool = _data->pass->queue->pool;
	memory::context ctx(pool);

	auto pipeline = new (pool) ComputePipelineData;
	pipeline->key = key.pdup(pool);
	pipeline->shader = move(spec);
	pipeline->layout = family->layout;
	pipeline->subpass = _data;

	const_cast<PipelineFamilyData *>(family)->computePipelines.emplace_back(pipeline);
	pipeline->family = family;

	_data->computePipelines.emplace(pipeline);
	const_cast<PipelineLayoutData *>(pipeline->layout)->computePipelines.emplace_back(pipeline);

	return pipeline;
}

void SubpassBuilder::setPrepareCallback(
		memory::function<void(FrameQueue &, const SubpassData &)> &&cb) {
	_data->prepareCallback = move(cb);
}

void SubpassBuilder::setCommandsCallback(
		memory::function<void(FrameQueue &, const SubpassData &, CommandBuffer &)> &&cb) {
	_data->commandsCallback = move(cb);
}

GraphicPipelineData *SubpassBuilder::emplacePipeline(StringView key,
		const PipelineFamilyData *family) {
	auto it = _data->graphicPipelines.find(key);
	if (it != _data->graphicPipelines.end()) {
		log::source().error("Resource", _data->key, ": Pipeline '", key, "' already added");
		return nullptr;
	}

	auto pool = _data->pass->queue->pool;
	memory::context ctx(pool);

	auto pipeline = new (pool) GraphicPipelineData;
	pipeline->key = key.pdup(pool);
	pipeline->subpass = _data;
	pipeline->layout = family->layout;
	pipeline->family = family;

	return pipeline;
}

void SubpassBuilder::finalizePipeline(GraphicPipelineData *data) {
	// validate shaders descriptors

	_data->graphicPipelines.emplace(data);

	const_cast<PipelineFamilyData *>(static_cast<const PipelineFamilyData *>(data->family))
			->graphicPipelines.emplace_back(data);

	const_cast<PipelineLayoutData *>(data->layout)->graphicPipelines.emplace_back(data);
}

void SubpassBuilder::erasePipeline(GraphicPipelineData *data) {
	_data->graphicPipelines.erase(data->key);
}

bool SubpassBuilder::setPipelineOption(GraphicPipelineData &f, DynamicState state) {
	f.dynamicState = state;
	return true;
}

bool SubpassBuilder::setPipelineOption(GraphicPipelineData &f,
		const Vector<SpecializationInfo> &programs) {
	for (auto &it : programs) {
		auto p = _data->pass->queue->programs.get(it.data->key);
		if (!p) {
			log::source().error("PipelineRequest", _data->key,
					": Shader not found in request: ", it.data->key);
			return false;
		}
	}

	f.shaders.reserve(programs.size());
	for (auto &it : programs) { f.shaders.emplace_back(move(it)); }
	return true;
}

bool SubpassBuilder::setPipelineOption(GraphicPipelineData &f, const PipelineMaterialInfo &info) {
	f.material = info;
	return true;
}

SubpassBuilder::SubpassBuilder(SubpassData *data) : _data(data) { }

const PipelineLayoutData *QueuePassBuilder::addDescriptorLayout(StringView str,
		const Callback<void(PipelineLayoutBuilder &)> &cb) {
	auto pool = _data->queue->pool;
	memory::context ctx(pool);

	auto layout = new (pool) PipelineLayoutData;
	layout->key = str.pdup(_data->queue->pool);
	layout->pass = _data;
	layout->index = uint32_t(_data->pipelineLayouts.size());

	auto defaultFamily = new (pool) PipelineFamilyData;
	defaultFamily->key = layout->key;
	defaultFamily->layout = layout;

	layout->defaultFamily = defaultFamily;
	layout->families.emplace_back(defaultFamily);

	PipelineLayoutBuilder builder(layout);
	cb(builder);

	_data->pipelineLayouts.emplace_back(layout);
	return layout;
}

const PipelineLayoutData *QueuePassBuilder::addDescriptorLayout(
		const Callback<void(PipelineLayoutBuilder &)> &cb) {
	auto pool = _data->queue->pool;
	memory::context ctx(pool);

	auto layout = new (pool) PipelineLayoutData;
	layout->key = _data->key;
	layout->pass = _data;
	layout->index = uint32_t(_data->pipelineLayouts.size());

	auto defaultFamily = new (pool) PipelineFamilyData;
	defaultFamily->key = layout->key;
	defaultFamily->layout = layout;

	layout->defaultFamily = defaultFamily;
	layout->families.emplace_back(defaultFamily);

	PipelineLayoutBuilder builder(layout);
	cb(builder);

	_data->pipelineLayouts.emplace_back(layout);
	return layout;
}

const SubpassData *QueuePassBuilder::addSubpass(const Callback<void(SubpassBuilder &)> &cb) {
	auto pool = _data->queue->pool;
	memory::context ctx(pool);

	auto subpass = new (pool) SubpassData;
	subpass->key = _data->key;
	subpass->pass = _data;
	subpass->index = uint32_t(_data->subpasses.size());

	SubpassBuilder builder(subpass);
	cb(builder);

	_data->subpasses.emplace_back(subpass);
	return subpass;
}

bool QueuePassBuilder::addSubpassDependency(const SubpassData *src, PipelineStage srcStage,
		AccessType srcAccess, const SubpassData *dst, PipelineStage dstStage, AccessType dstAccess,
		bool byRegion) {
	_data->dependencies.emplace_back(SubpassDependency{src->index, srcStage, srcAccess, dst->index,
		dstStage, dstAccess, byRegion});
	return true;
}

const AttachmentPassData *QueuePassBuilder::addAttachment(const AttachmentData *data) {
	return addAttachment(data, [](AttachmentPassBuilder &builder) { });
}

const AttachmentPassData *QueuePassBuilder::addAttachment(const AttachmentData *data,
		const AttachmentDependencyInfo &deps) {
	return addAttachment(data,
			[&](AttachmentPassBuilder &builder) { builder.setDependency(deps); });
}

const AttachmentPassData *QueuePassBuilder::addAttachment(const AttachmentData *data,
		const Callback<void(AttachmentPassBuilder &)> &cb) {
	auto pool = _data->queue->pool;
	memory::context ctx(pool);

	for (auto &it : _data->attachments) {
		if (it->attachment == data) {
			return it;
		}
	}

	auto a = new (pool) AttachmentPassData;
	a->key = data->key;
	a->attachment = data;
	a->pass = _data;
	a->index = uint32_t(_data->attachments.size());

	AttachmentPassBuilder builder(a);
	cb(builder);

	_data->attachments.emplace_back(a);
	((AttachmentData *)data)->passes.emplace_back(a);

	return a;
}

void QueuePassBuilder::setAvailabilityChecker(
		memory::function<bool(const FrameQueue &, const QueuePassData &)> &&cb) {
	_data->checkAvailable = move(cb);
}

void QueuePassBuilder::addSubmittedCallback(
		memory::function<void(FrameQueue &, const QueuePassData &, bool success)> &&cb) {
	_data->submittedCallbacks.emplace_back(move(cb));
}

void QueuePassBuilder::addCompleteCallback(
		memory::function<void(FrameQueue &, const QueuePassData &, bool success)> &&cb) {
	_data->submittedCallbacks.emplace_back(move(cb));
}

void QueuePassBuilder::setAcquireTimestamps(uint32_t t) { _data->acquireTimestamps = t; }

StringView QueuePassBuilder::getName() const { return _data->key; }

QueuePassBuilder::QueuePassBuilder(QueuePassData *data) : _data(data) { }

Queue::Builder::Builder(StringView name)
: _internalResource(memory::pool::create((memory::pool_t *)nullptr), toString(name, "_resource")) {
	auto p = _internalResource.getPool();
	memory::perform([&] {
		_data = new (p) QueueData;
		_data->pool = p;
		_data->key = name.pdup(p);

		uint8_t empty = 0, solid = 255;
		uint64_t emptyBuffer = maxOf<uint64_t>();

		_data->emptyImage = _internalResource.getImage(EmptyTextureName);
		if (!_data->emptyImage) {
			_data->emptyImage = _internalResource.addBitmapImage(EmptyTextureName,
					ImageInfo(Extent2(1, 1), core::ImageUsage::Sampled,
							core::ImageFormat::R8_UNORM),
					BytesView(&empty, 1).pdup(_internalResource.getPool()));
			_internalResource.addImageView(_data->emptyImage, ImageViewInfo());
		}

		_data->solidImage = _internalResource.getImage(SolidTextureName);
		if (!_data->solidImage) {
			_data->solidImage = _internalResource.addBitmapImage(SolidTextureName,
					ImageInfo(Extent2(1, 1), core::ImageUsage::Sampled, core::ImageFormat::R8_UNORM,
							core::ImageHints::Opaque),
					BytesView(&solid, 1).pdup(_internalResource.getPool()));
			_internalResource.addImageView(_data->solidImage, ImageViewInfo());
		}

		_data->emptyBuffer = _internalResource.getBuffer(EmptyBufferName);
		if (!_data->emptyBuffer) {
			_data->emptyBuffer = _internalResource.addBuffer(EmptyBufferName,
					BufferInfo(uint64_t(8), core::BufferUsage::StorageBuffer),
					BytesView(reinterpret_cast<const uint8_t *>(&emptyBuffer), sizeof(uint64_t))
							.pdup(_internalResource.getPool()));
		}
	}, p);
}

Queue::Builder::~Builder() {
	if (_data) {
		auto p = _data->pool;
		memory::pool::destroy(p);
		_data = nullptr;
	}
}

static std::atomic<uint64_t> s_AttachmentCurrentIndex = 1;

void Queue::Builder::setDefaultSyncPassState(FrameRenderPassState val) {
	_data->defaultSyncPassState = val;
}

const AttachmentData *Queue::Builder::addAttachemnt(StringView name,
		const Callback<Rc<Attachment>(AttachmentBuilder &)> &cb) {
	auto it = _data->attachments.find(name);
	if (it == _data->attachments.end()) {
		AttachmentData *ret = nullptr;
		memory::perform([&] {
			ret = new (_data->pool) AttachmentData();
			ret->key = name.pdup(_data->pool);
			ret->id = s_AttachmentCurrentIndex.fetch_add(1);
			ret->queue = _data;

			AttachmentBuilder builder(ret);
			auto p = cb(builder);

			ret->attachment = p;
			_data->attachments.emplace(ret);
		}, _data->pool);
		return ret;
	} else {
		log::source().error("Queue::Builder", "Attachment for name already defined: ", name);
	}
	return nullptr;
}

const QueuePassData *Queue::Builder::addPass(StringView name, PassType type,
		RenderOrdering ordering, const Callback<Rc<QueuePass>(QueuePassBuilder &)> &cb) {
	auto it = _data->passes.find(name);
	if (it == _data->passes.end()) {
		QueuePassData *ret = nullptr;
		memory::perform([&] {
			ret = new (_data->pool) QueuePassData();
			ret->key = name.pdup(_data->pool);
			ret->queue = _data;
			ret->ordering = ordering;
			ret->type = type;

			QueuePassBuilder builder(ret);
			auto p = cb(builder);

			ret->pass = p;
			_data->passes.emplace(ret);
		}, _data->pool);
		return ret;
	} else {
		log::source().error("Queue::Builder", "RenderPass for name already defined: ", name);
	}
	return nullptr;
}

const ProgramData *Queue::Builder::addProgram(StringView key, SpanView<uint32_t> data,
		const ProgramInfo *info) {
	if (!_data) {
		log::source().error("Resource", "Fail to add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key,
				[&, this]() -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->data = data.pdup(_data->pool);
		if (info) {
			program->stage = info->stage;
			program->bindings = info->bindings;
			program->constants = info->constants;
		}
		return program;
	}, _data->pool)) {
		return r;
	}

	log::source().error("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

const ProgramData *Queue::Builder::addProgramByRef(StringView key, SpanView<uint32_t> data,
		const ProgramInfo *info) {
	if (!_data) {
		log::source().error("Resource", "Fail tom add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key,
				[&, this]() -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->data = data;
		if (info) {
			program->stage = info->stage;
			program->bindings = info->bindings;
			program->constants = info->constants;
		}
		return program;
	}, _data->pool)) {
		return r;
	}

	log::source().error("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

const ProgramData *Queue::Builder::addProgram(StringView key,
		const memory::function<void(Device &, const ProgramData::DataCallback &)> &cb,
		const ProgramInfo *info) {
	if (!_data) {
		log::source().error("Resource", "Fail to add shader: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<ProgramData>(_data->programs, key,
				[&, this]() -> ProgramData * {
		auto program = new (_data->pool) ProgramData;
		program->key = key.pdup(_data->pool);
		program->callback = cb;
		if (info) {
			program->stage = info->stage;
			program->bindings = info->bindings;
			program->constants = info->constants;
		}
		return program;
	}, _data->pool)) {
		return r;
	}

	log::source().error("Resource", _data->key, ": Shader already added: ", key);
	return nullptr;
}

const TextureSetLayoutData *Queue::Builder::addTextureSetLayout(StringView key,
		SpanView<SamplerInfo> samplers, uint32_t images, uint32_t buffers, uint32_t imagesIndexed,
		uint32_t buffersIndexed) {
	if (!_data) {
		log::source().error("Resource", "Fail to add TextureSetLayout: ", key, ", not initialized");
		return nullptr;
	}

	if (auto r = Resource_conditionalInsert<TextureSetLayoutData>(_data->textureSets, key,
				[&, this]() -> TextureSetLayoutData * {
		auto layout = new (_data->pool) TextureSetLayoutData;
		layout->queue = _data;
		layout->key = key.pdup(_data->pool);
		layout->imageCount = images;
		layout->imageCountIndexed = imagesIndexed;
		layout->bufferCount = buffers;
		layout->bufferCountIndexed = buffersIndexed;
		layout->samplers = samplers.vec<memory::PoolInterface>();
		layout->compiledSamplers.resize(layout->samplers.size());
		return layout;
	}, _data->pool)) {
		return r;
	}

	log::source().error("Resource", _data->key, ": TextureSetLayout already added: ", key);
	return nullptr;
}

void Queue::Builder::addLinkedResource(const Rc<Resource> &res) {
	if (!_data) {
		log::source().error("Resource", "Fail to add linked resource: ", res->getName(),
				", not initialized");
		return;
	}
	if (res->getOwner() != nullptr) {
		log::source().error("Resource", "Fail to add linked resource: ", res->getName(),
				", it's owned by ", res->getOwner()->getName());
		return;
	}
	if (!res->isCompiled()) {
		log::source().error("Resource", "Fail to add linked resource: ", res->getName(),
				", resource is not compiled");
		return;
	}
	_data->linked.emplace(res);
}

void Queue::Builder::setBeginCallback(Function<void(FrameRequest &)> &&cb) {
	_data->beginCallback = sp::move(cb);
}

void Queue::Builder::setEndCallback(Function<void(FrameRequest &)> &&cb) {
	_data->endCallback = sp::move(cb);
}

void Queue::Builder::setAttachCallback(Function<void(const FrameHandle *)> &&cb) {
	_data->attachCallback = sp::move(cb);
}

void Queue::Builder::setDetachCallback(Function<void(const FrameHandle *)> &&cb) {
	_data->detachCallback = sp::move(cb);
}

const BufferData *Queue::Builder::addBufferByRef(StringView key, BufferInfo &&info, BytesView data,
		Rc<DataAtlas> &&atlas, AccessType access) {
	return _internalResource.addBufferByRef(key, move(info), data, move(atlas), access);
}

const BufferData *Queue::Builder::addBuffer(StringView key, BufferInfo &&info, const FileInfo &data,
		Rc<DataAtlas> &&atlas, AccessType access) {
	return _internalResource.addBuffer(key, move(info), data, move(atlas), access);
}

const BufferData *Queue::Builder::addBuffer(StringView key, BufferInfo &&info, BytesView data,
		Rc<DataAtlas> &&atlas, AccessType access) {
	return _internalResource.addBuffer(key, move(info), data, move(atlas), access);
}

const BufferData *Queue::Builder::addBuffer(StringView key, BufferInfo &&info,
		const memory::function<void(uint8_t *, uint64_t, const BufferData::DataCallback &)> &cb,
		Rc<DataAtlas> &&atlas, AccessType access) {
	return _internalResource.addBuffer(key, move(info), cb, move(atlas), access);
}

const ImageData *Queue::Builder::addBitmapImageByRef(StringView key, ImageInfo &&info,
		BytesView data, AttachmentLayout layout, AccessType access) {
	return _internalResource.addBitmapImageByRef(key, move(info), data, layout, access);
}
const ImageData *Queue::Builder::addBitmapImage(StringView key, ImageInfo &&info, BytesView data,
		AttachmentLayout layout, AccessType access) {
	return _internalResource.addBitmapImage(key, move(info), data, layout, access);
}
const ImageData *Queue::Builder::addEncodedImageByRef(StringView key, ImageInfo &&info,
		BytesView data, AttachmentLayout layout, AccessType access) {
	return _internalResource.addEncodedImageByRef(key, move(info), data, layout, access);
}
const ImageData *Queue::Builder::addEncodedImage(StringView key, ImageInfo &&info, BytesView data,
		AttachmentLayout layout, AccessType access) {
	return _internalResource.addEncodedImage(key, move(info), data, layout, access);
}
const ImageData *Queue::Builder::addImage(StringView key, ImageInfo &&info, const FileInfo &data,
		AttachmentLayout layout, AccessType access) {
	return _internalResource.addImage(key, move(info), data, layout, access);
}
const ImageData *Queue::Builder::addImage(StringView key, ImageInfo &&info,
		const memory::function<void(uint8_t *, uint64_t, const ImageData::DataCallback &)> &cb,
		AttachmentLayout layout, AccessType access) {
	return _internalResource.addImage(key, move(info), cb, layout, access);
}

const ImageViewData *Queue::Builder::addImageView(const ImageData *data, ImageViewInfo &&view) {
	return _internalResource.addImageView(data, move(view));
}

} // namespace stappler::xenolith::core
