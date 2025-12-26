/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#ifndef EXTRA_DOCUMENT_RICHTEXT_XLRICHTEXT_H_
#define EXTRA_DOCUMENT_RICHTEXT_XLRICHTEXT_H_

#include "XLAsset.h"
#include "XLAssetLibrary.h"
#include "SPFont.h"
#include "SPFontHyphenMap.h"
#include "SPDocLayoutResult.h"
#include "SPDocLayoutObject.h"
#include "SPDocAsset.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::richtext {

using MediaParameters = stappler::document::MediaParameters;
using Result = stappler::document::LayoutResult;
using PageData = stappler::document::LayoutPageData;
using Object = stappler::document::Object;
using Layout = stappler::document::LayoutBlock;
using Document = stappler::document::Document;
using DocumentAssetMeta = stappler::document::DocumentAssetMeta;

using Background = stappler::document::BackgroundParameters;
using Label = stappler::document::Label;
using Link = stappler::document::Link;

using AssetLibrary = storage::AssetLibrary;
using Asset = storage::Asset;
using AssetLock = storage::AssetLock;

class CommonSource;
class CommonView;
class ListenerView;
class View;
class ImageLayout;

namespace config {

constexpr auto getDocumentAssetTtl() {
	return TimeInterval::seconds(30 * 24 * 60 * 60); // 30 days
}

constexpr uint32_t ENGINE_VERSION = 4;

} // namespace config

} // namespace stappler::xenolith::richtext

#endif /* EXTRA_DOCUMENT_RICHTEXT_XLRICHTEXT_H_ */
