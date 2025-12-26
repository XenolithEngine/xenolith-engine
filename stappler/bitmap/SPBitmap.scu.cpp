/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "SPCommon.h"
#include "SPBitmapCustom.cc"
#include "SPBitmapGif.cc"
#include "SPBitmapJpeg.cc"
#include "SPBitmapPng.cc"
#include "SPBitmapWebp.cc"
#include "SPBitmapShared.cc"

namespace STAPPLER_VERSIONIZED stappler::bitmap {

static BitmapFormat s_defaultFormats[toInt(FileFormat::Custom)] = {
	BitmapFormat(FileFormat::Png, &png::isPng, &png::getPngImageSize
			, &png::infoPng, &png::loadPng, &png::writePng, &png::savePng
	),
	BitmapFormat(FileFormat::Jpeg, &jpeg::isJpg, &jpeg::getJpegImageSize
			, &jpeg::infoJpg, &jpeg::loadJpg, &jpeg::writeJpeg, &jpeg::saveJpeg
	),
	BitmapFormat(FileFormat::WebpLossless, &webp::isWebpLossless, &webp::getWebpLosslessImageSize
			, &webp::infoWebp, &webp::loadWebp, &webp::writeWebpLossless, &webp::saveWebpLossless
	),
	BitmapFormat(FileFormat::WebpLossy, &webp::isWebp, &webp::getWebpImageSize
			, &webp::infoWebp, &webp::loadWebp, &webp::writeWebpLossy, &webp::saveWebpLossy
	),
	BitmapFormat(FileFormat::Svg, &custom::isSvg, &custom::getSvgImageSize),
	BitmapFormat(FileFormat::Gif, &gif::isGif, &gif::getGifImageSize
			, &gif::infoGif, &gif::loadGif
	),
	BitmapFormat(FileFormat::Tiff, &custom::isTiff, &custom::getTiffImageSize),
};

static std::mutex _formatListMutex;
static std::vector<BitmapFormat *> _formatList;

SPUNUSED static const BitmapFormat &getDefaultFormat(uint32_t i) {
	return s_defaultFormats[i];
}

SPUNUSED static std::unique_lock<std::mutex> lockFormatList() {
	return std::unique_lock<std::mutex>(_formatListMutex);
}

SPUNUSED static void addCustomFormat(BitmapFormat &&fmt) {
	auto lock = lockFormatList();
	_formatList.emplace_back(new BitmapFormat(move(fmt)));
}

SPUNUSED static const std::vector<BitmapFormat *> &getCustomFormats() {
	return _formatList;
}

}

#include "SPBitmapFormat.cc"
#include "SPBitmap.cc"
#include "SPBitmapResample.cc"
