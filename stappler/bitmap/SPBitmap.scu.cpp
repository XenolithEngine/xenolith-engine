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

#include "SPMemory.h"
#include "SPBitmapCustom.cc"
#include "SPBitmapGif.cc"
#include "SPBitmapJpeg.cc"
#include "SPBitmapPng.cc"
#include "SPBitmapWebp.cc"
#include "SPBitmapShared.cc"

#include <sprt/cxx/mutex>

namespace STAPPLER_VERSIONIZED stappler::bitmap {

static BitmapFormat s_defaultFormats[toInt(FileFormat::Custom)] = {
	BitmapFormat(FileFormat::Png, &png::isPng, &png::getPngImageSize, &png::infoPng, &png::loadPng,
			&png::writePng, &png::savePng),
	BitmapFormat(FileFormat::Jpeg, &jpeg::isJpg, &jpeg::getJpegImageSize, &jpeg::infoJpg,
			&jpeg::loadJpg, &jpeg::writeJpeg, &jpeg::saveJpeg),
	BitmapFormat(FileFormat::WebpLossless, &webp::isWebpLossless, &webp::getWebpLosslessImageSize,
			&webp::infoWebp, &webp::loadWebp, &webp::writeWebpLossless, &webp::saveWebpLossless),
	BitmapFormat(FileFormat::WebpLossy, &webp::isWebp, &webp::getWebpImageSize, &webp::infoWebp,
			&webp::loadWebp, &webp::writeWebpLossy, &webp::saveWebpLossy),
	BitmapFormat(FileFormat::Svg, &custom::isSvg, &custom::getSvgImageSize),
	BitmapFormat(FileFormat::Gif, &gif::isGif, &gif::getGifImageSize, &gif::infoGif, &gif::loadGif),
	BitmapFormat(FileFormat::Tiff, &custom::isTiff, &custom::getTiffImageSize),
};

static sprt::qmutex _formatListMutex;
static mem_std::Vector<BitmapFormat *> _formatList;

const BitmapFormat *getDefaultFormat(FileFormat i) {
	if (i < FileFormat::Custom) {
		return &s_defaultFormats[toInt(i)];
	}
	return nullptr;
}

SPUNUSED static sprt::unique_lock<sprt::mutex> lockFormatList() {
	return sprt::unique_lock<sprt::mutex>(_formatListMutex);
}

SPUNUSED static void addCustomFormat(BitmapFormat &&fmt) {
	auto lock = lockFormatList();
	_formatList.emplace_back(new (sprt::nothrow) BitmapFormat(move(fmt)));
}

SPUNUSED static const mem_std::Vector<BitmapFormat *> &getCustomFormats() { return _formatList; }

const BitmapFormat *getCustomFormat(StringView name) {
	auto lock = lockFormatList();

	for (auto &it : getCustomFormats()) {
		if (it->getName() == name && it->isWritable()) {
			return it;
		}
	}
	return nullptr;
}

} // namespace stappler::bitmap

#include "SPBitmapFormat.cc"
#include "SPBitmap.cc"
#include "SPBitmapResample.cc"
