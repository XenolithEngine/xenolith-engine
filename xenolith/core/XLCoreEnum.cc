/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "XLCoreEnum.h"
#include "SPCore.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

PipelineStage getStagesForQueue(QueueFlags flags) {
	PipelineStage ret = PipelineStage::TopOfPipe | PipelineStage::BottomOfPipe | PipelineStage::Host
			| PipelineStage::AllCommands | PipelineStage::CommandPreprocess;
	if (hasFlag(flags, QueueFlags::Graphics)) {
		ret |= PipelineStage::DrawIndirect | PipelineStage::VertexInput
				| PipelineStage::VertexShader | PipelineStage::TesselationControl
				| PipelineStage::TesselationEvaluation | PipelineStage::GeometryShader
				| PipelineStage::FragmentShader | PipelineStage::EarlyFragmentTest
				| PipelineStage::LateFragmentTest | PipelineStage::ColorAttachmentOutput
				| PipelineStage::AllGraphics | PipelineStage::TransformFeedback
				| PipelineStage::ConditionalRendering | PipelineStage::AccelerationStructureBuild
				| PipelineStage::RayTracingShader | PipelineStage::ShadingRateImage
				| PipelineStage::TaskShader | PipelineStage::MeshShader
				| PipelineStage::FragmentDensityProcess;
	}
	if (hasFlag(flags, QueueFlags::Compute)) {
		ret |= PipelineStage::ComputeShader;
	}
	if (hasFlag(flags, QueueFlags::Transfer)) {
		ret |= PipelineStage::Transfer;
	}
	return ret;
}

StringView getDescriptorTypeName(DescriptorType type) {
	switch (type) {
	case DescriptorType::Sampler: return StringView("Sampler"); break;
	case DescriptorType::CombinedImageSampler: return StringView("CombinedImageSampler"); break;
	case DescriptorType::SampledImage: return StringView("SampledImage"); break;
	case DescriptorType::StorageImage: return StringView("StorageImage"); break;
	case DescriptorType::UniformTexelBuffer: return StringView("UniformTexelBuffer"); break;
	case DescriptorType::StorageTexelBuffer: return StringView("StorageTexelBuffer"); break;
	case DescriptorType::UniformBuffer: return StringView("UniformBuffer"); break;
	case DescriptorType::StorageBuffer: return StringView("StorageBuffer"); break;
	case DescriptorType::UniformBufferDynamic: return StringView("UniformBufferDynamic"); break;
	case DescriptorType::StorageBufferDynamic: return StringView("StorageBufferDynamic"); break;
	case DescriptorType::InputAttachment: return StringView("InputAttachment"); break;
	default: break;
	}
	return StringView("Unknown");
}

void getProgramStageDescription(const CallbackStream &stream, ProgramStage fmt) {
	if ((fmt & ProgramStage::Vertex) != ProgramStage::None) {
		stream << " Vertex";
	}
	if ((fmt & ProgramStage::TesselationControl) != ProgramStage::None) {
		stream << " TesselationControl";
	}
	if ((fmt & ProgramStage::TesselationEvaluation) != ProgramStage::None) {
		stream << " TesselationEvaluation";
	}
	if ((fmt & ProgramStage::Geometry) != ProgramStage::None) {
		stream << " Geometry";
	}
	if ((fmt & ProgramStage::Fragment) != ProgramStage::None) {
		stream << " Fragment";
	}
	if ((fmt & ProgramStage::Compute) != ProgramStage::None) {
		stream << " Compute";
	}
	if ((fmt & ProgramStage::RayGen) != ProgramStage::None) {
		stream << " RayGen";
	}
	if ((fmt & ProgramStage::AnyHit) != ProgramStage::None) {
		stream << " AnyHit";
	}
	if ((fmt & ProgramStage::ClosestHit) != ProgramStage::None) {
		stream << " ClosestHit";
	}
	if ((fmt & ProgramStage::MissHit) != ProgramStage::None) {
		stream << " MissHit";
	}
	if ((fmt & ProgramStage::AnyHit) != ProgramStage::None) {
		stream << " AnyHit";
	}
	if ((fmt & ProgramStage::Intersection) != ProgramStage::None) {
		stream << " Intersection";
	}
	if ((fmt & ProgramStage::Callable) != ProgramStage::None) {
		stream << " Callable";
	}
	if ((fmt & ProgramStage::Task) != ProgramStage::None) {
		stream << " Task";
	}
	if ((fmt & ProgramStage::Mesh) != ProgramStage::None) {
		stream << " Mesh";
	}
}

const CallbackStream &operator<<(const CallbackStream &out, ProgramStage stage) {
	for (auto value : flags(stage)) {
		switch (value) {
		case ProgramStage::None: break;
		case ProgramStage::Vertex: out << " Vertex"; break;
		case ProgramStage::TesselationControl: out << " TesselationControl"; break;
		case ProgramStage::TesselationEvaluation: out << " TesselationEvaluation"; break;
		case ProgramStage::Geometry: out << " Geometry"; break;
		case ProgramStage::Fragment: out << " Fragment"; break;
		case ProgramStage::Compute: out << " Compute"; break;
		case ProgramStage::RayGen: out << " RayGen"; break;
		case ProgramStage::AnyHit: out << " AnyHit"; break;
		case ProgramStage::ClosestHit: out << " ClosestHit"; break;
		case ProgramStage::MissHit: out << " MissHit"; break;
		case ProgramStage::Intersection: out << " Intersection"; break;
		case ProgramStage::Callable: out << " Callable"; break;
		case ProgramStage::Task: out << " Task"; break;
		case ProgramStage::Mesh: out << " Mesh"; break;
		}
	}
	return out;
}

const CallbackStream &operator<<(const CallbackStream &out, WindowState f) {
	for (auto value : flags(f)) {
		switch (value) {
		case WindowState::None: break;
		case WindowState::Modal: out << " Modal"; break;
		case WindowState::Sticky: out << " Sticky"; break;
		case WindowState::MaximizedVert: out << " MaximizedVert"; break;
		case WindowState::MaximizedHorz: out << " MaximizedHorz"; break;
		case WindowState::Maximized: break;
		case WindowState::Shaded: out << " Shaded"; break;
		case WindowState::SkipTaskbar: out << " SkipTaskbar"; break;
		case WindowState::SkipPager: out << " SkipPager"; break;
		case WindowState::Minimized: out << " Minimized"; break;
		case WindowState::Fullscreen: out << " Fullscreen"; break;
		case WindowState::Above: out << " Above"; break;
		case WindowState::Below: out << " Below"; break;
		case WindowState::DemandsAttention: out << " DemandsAttention"; break;
		case WindowState::Focused: out << " Focused"; break;
		case WindowState::Resizing: out << " Resizing"; break;
		case WindowState::Pointer: out << " Pointer"; break;
		case WindowState::CloseGuard: out << " CloseGuard"; break;
		case WindowState::CloseRequest: out << " CloseRequest"; break;
		case WindowState::Enabled: out << " Enabled"; break;
		case WindowState::DecorationStatusBarVisible: out << " DecorationStatusBarVisible"; break;
		case WindowState::DecorationNavigationVisible: out << " DecorationNavigationVisible"; break;
		case WindowState::DecorationStatusBarLight: out << " DecorationStatusBarLight"; break;
		case WindowState::DecorationNavigationLight: out << " DecorationNavigationLight"; break;
		case WindowState::DecorationShowBySwipe: out << " DecorationShowBySwipe"; break;
		case WindowState::DecorationState: break;
		case WindowState::AllowedWindowMenu: out << " AllowedWindowMenu"; break;
		case WindowState::AllowedMove: out << " MoveAllowed"; break;
		case WindowState::AllowedResize: out << " ResizeAllowed"; break;
		case WindowState::AllowedMinimize: out << " MinimizeAllowed"; break;
		case WindowState::AllowedShade: out << " ShadeAllowed"; break;
		case WindowState::AllowedStick: out << " StickAllowed"; break;
		case WindowState::AllowedMaximizeVert: out << " MaximizeVertAllowed"; break;
		case WindowState::AllowedMaximizeHorz: out << " MaximizeHorzAllowed"; break;
		case WindowState::AllowedClose: out << " CloseAllowed"; break;
		case WindowState::AllowedFullscreen: out << " FullscreenAllowed"; break;
		case WindowState::AllowedActionsMask: break;
		case WindowState::TiledLeft: out << " TiledLeft"; break;
		case WindowState::TiledRight: out << " TiledRight"; break;
		case WindowState::TiledTop: out << " TiledTop"; break;
		case WindowState::TiledBottom: out << " TiledBottom"; break;
		case WindowState::TiledTopLeft: break;
		case WindowState::TiledTopRight: break;
		case WindowState::TiledBottomLeft: break;
		case WindowState::TiledBottomRight: break;
		case WindowState::ConstrainedLeft: out << " ConstrainedLeft"; break;
		case WindowState::ConstrainedRight: out << " ConstrainedRight"; break;
		case WindowState::ConstrainedTop: out << " ConstrainedTop"; break;
		case WindowState::ConstrainedBottom: out << " ConstrainedBottom"; break;
		case WindowState::TilingMask: break;
		case WindowState::All: break;
		}
	}
	return out;
}

} // namespace stappler::xenolith::core
