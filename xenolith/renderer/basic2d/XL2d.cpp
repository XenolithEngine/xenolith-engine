/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XLCommon.h" // IWYU pragma: keep

#include "XL2dCommandList.cc"
#include "XL2dVertexArray.cc"
#include "XL2dFrameContext.cc"

#include "XL2dSprite.cc"
#include "XL2dLayer.cc"
#include "XL2dLabel.cc"
#include "XL2dScene.cc"
#include "XL2dSceneContent.cc"
#include "XL2dSceneLayout.cc"
#include "XL2dOverlayLayout.cc"
#include "XL2dSceneLight.cc"

#include "XL2dVectorCanvas.cc"
#include "XL2dVectorSprite.cc"

#include "XL2dActionAcceleratedMove.cc"
#include "XL2dImageLayer.cc"
#include "XL2dLayerRounded.cc"
#include "XL2dLinearProgress.cc"
#include "XL2dRoundedProgress.cc"

#include "icons/XL2dIconSprite.cc"

#include "particle/XL2dParticleSystem.cc"
#include "particle/XL2dParticleEmitter.cc"

#ifdef MODULE_XENOLITH_BACKEND_VK
#include "backend/vk/XL2dVkMaterial.cc"
#include "backend/vk/XL2dVkVertexPass.cc"
#include "backend/vk/XL2dVkShadow.cc"
#include "backend/vk/XL2dVkParticlePass.cc"
#include "backend/vk/XL2dVkShadowPass.cc"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

ImagePlacementResult ImagePlacementInfo::resolve(const Size2 &viewSize, const Size2 &imageSize) {
	ImagePlacementResult result;

	// find texture fragment size on image
	result.imageFragmentSize = Size2(imageSize.width * textureRect.size.width,
			imageSize.height * textureRect.size.height);

	// write defaults
	result.viewRect = Rect(Vec2::ZERO, viewSize);
	result.imageRect = Rect(0, 0, result.imageFragmentSize.width, result.imageFragmentSize.height);
	result.textureRect = textureRect;
	result.scale = 1.0f;

	// find scale from view to image
	switch (autofit) {
	case Autofit::None:
		result.scale = (result.imageFragmentSize.width / viewSize.width
							   + result.imageFragmentSize.height / viewSize.height)
				/ 2.0f;
		return result;
		break;
	case Autofit::Width: result.scale = result.imageFragmentSize.width / viewSize.width; break;
	case Autofit::Height: result.scale = result.imageFragmentSize.height / viewSize.height; break;
	case Autofit::Contain:
		result.scale = std::max(result.imageFragmentSize.width / viewSize.width,
				result.imageFragmentSize.height / viewSize.height);
		break;
	case Autofit::Cover:
		result.scale = std::min(result.imageFragmentSize.width / viewSize.width,
				result.imageFragmentSize.height / viewSize.height);
		break;
	}

	auto imageSizeInView = Size2(result.imageFragmentSize.width / result.scale,
			result.imageFragmentSize.height / result.scale);
	if (imageSizeInView.width < viewSize.width) {
		// reduce view size to fit image
		result.viewRect.size.width -= (viewSize.width - imageSizeInView.width);
		result.viewRect.origin.x = (viewSize.width - imageSizeInView.width) * autofitPos.x;
	} else if (imageSizeInView.width > viewSize.width) {
		// truncate image to fit view
		result.imageRect.origin.x =
				(result.imageFragmentSize.width - viewSize.width * result.scale) * autofitPos.x;
		result.imageRect.size.width = viewSize.width * result.scale;
	}

	if (imageSizeInView.height < viewSize.height) {
		// reduce view size to fit image
		result.viewRect.size.height -= (viewSize.height - imageSizeInView.height);
		result.viewRect.origin.y = (viewSize.height - imageSizeInView.height) * autofitPos.y;
	} else if (imageSizeInView.height > viewSize.height) {
		// truncate image to fit view
		result.imageRect.origin.y =
				(result.imageFragmentSize.height - viewSize.height * result.scale) * autofitPos.y;
		result.imageRect.size.height = viewSize.height * result.scale;
	}

	// adjust texture rect to image rect
	result.textureRect = Rect(textureRect.origin.x + result.imageRect.origin.x / imageSize.width,
			textureRect.origin.y + result.imageRect.origin.y / imageSize.height,
			result.imageRect.size.width / imageSize.width,
			result.imageRect.size.height / imageSize.height);

	return result;
}

} // namespace stappler::xenolith::basic2d
