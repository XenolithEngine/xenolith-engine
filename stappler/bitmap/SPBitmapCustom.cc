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

#include "SPBitmapFormat.h"
#include "SPHtmlParser.h"
#include "SPLog.h"

namespace STAPPLER_VERSIONIZED stappler::bitmap::custom {

static uint32_t detectSvgSize(StringView value) {
	StringView str(value);
	auto fRes = str.readFloat();
	if (!fRes.valid()) {
		return 0;
	}

	auto fvalue = fRes.get();
	if (fvalue == 0.0f) {
		return 0;
	}

	str.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	if (str == "px" || str.empty()) {
		// do nothing
	} else if (str == "pt") {
		fvalue = fvalue * 4.0f / 3.0f;
	} else if (str == "pc") {
		fvalue = fvalue * 15.0f;
	} else if (str == "mm") {
		fvalue = fvalue * 3.543307f;
	} else if (str == "cm") {
		fvalue = fvalue * 35.43307f;
	} else {
		log::source().error("Bitmap", "Invalid size metric in svg: %s", str.data());
		return 0;
	}

	return uint32_t(ceilf(fvalue));
}

static bool detectSvg(StringView str, uint32_t &w, uint32_t &h) {
	str.skipUntilString("<svg", true);
	if (!str.starts_with("<svg")) {
		return false;
	}
	str += "<svg"_len;

	if (!str.empty() && str.is<StringView::CharGroup<CharGroupId::WhiteSpace>>()) {
		bool found = false;
		bool isSvg = false;
		uint32_t width = 0;
		uint32_t height = 0;
		while (!found && !str.empty()) {
			str.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
			auto key = html::Tag_readAttrName(str);
			auto value = html::Tag_readAttrValue(str);
			if (!key.empty() && !value.empty()) {
				if (key == "xmlns") {
					if (value.is("http://www.w3.org/2000/svg")) {
						isSvg = true;
					}
				} else if (key == "width") {
					width = detectSvgSize(value);
				} else if (key == "height") {
					height = detectSvgSize(value);
				}
				if (isSvg && width && height) {
					found = true;
				}
			}
		}
		if (isSvg) {
			w = width;
			h = height;
		}
		return isSvg;
	}

	return false;
}

static bool detectSvg(const StringView &buf) {
	uint32_t w = 0, h = 0;
	return detectSvg(buf, w, h);
}

static bool isSvg(const uint8_t *data, size_t dataLen) {
	if (dataLen <= 127) {
		return false;
	}

	return detectSvg(StringView((const char *)data, dataLen));
}

static bool getSvgImageSize(const io::Producer &file, StackBuffer<512> &data, uint32_t &width,
		uint32_t &height) {
	if (detectSvg(StringView((const char *)data.data(), data.size()), width, height)) {
		return true;
	}

	return false;
}

static bool isTiff(const uint8_t *data, size_t dataLen) {
	if (dataLen <= 4) {
		return false;
	}

	static const char *TIFF_II = "II";
	static const char *TIFF_MM = "MM";

	return (memcmp(data, TIFF_II, 2) == 0 && *(static_cast<const unsigned char *>(data) + 2) == 42
				   && *(static_cast<const unsigned char *>(data) + 3) == 0)
			|| (memcmp(data, TIFF_MM, 2) == 0
					&& *(static_cast<const unsigned char *>(data) + 2) == 0
					&& *(static_cast<const unsigned char *>(data) + 3) == 42);
}

template <typename Reader>
static bool getTiffImageSizeImpl(const io::Producer &file, StackBuffer<512> &data, uint32_t &width,
		uint32_t &height) {
	auto reader = Reader(data.data() + 4, 4);
	auto offset = reader.readUnsigned32();

	data.clear();
	if (file.seekAndRead(offset, data, 2) != 2) {
		return false;
	}
	auto size = Reader(data.data(), 2).readUnsigned16();
	auto dictSize = size * 12;
	offset += 2;
	while (dictSize > 0) {
		data.clear();
		size_t blockSize = min(12 * 21, dictSize);
		if (file.read(data, blockSize) != blockSize) {
			return false;
		}

		auto blocks = blockSize / 12;
		reader = Reader(data.data(), blockSize);

		for (uint16_t i = 0; i < blocks; ++i) {
			auto tagid = reader.readUnsigned16();
			auto type = reader.readUnsigned16();
			auto count = reader.readUnsigned32();
			if (tagid == 256 && count == 1) {
				if (type == 3) {
					width = reader.readUnsigned16();
					reader.offset(2);
				} else if (type == 4) {
					width = reader.readUnsigned32();
				} else {
					reader.offset(4);
				}
			} else if (tagid == 257 && count == 1) {
				if (type == 3) {
					height = reader.readUnsigned16();
					reader.offset(2);
				} else if (type == 4) {
					height = reader.readUnsigned32();
				} else {
					reader.offset(4);
				}
				return true;
			} else {
				if (tagid > 257) {
					return false;
				}
				reader.offset(4);
			}
		}
	}
	return false;
}

static bool getTiffImageSize(const io::Producer &file, StackBuffer<512> &data, uint32_t &width,
		uint32_t &height) {
	if (isTiff(data.data(), data.size())) {
		if (memcmp(data.data(), "II", 2) == 0) {
			if (getTiffImageSizeImpl<BytesViewTemplate<sprt::endian::little>>(file, data, width,
						height)) {
				return true;
			}
		} else {
			if (getTiffImageSizeImpl<BytesViewTemplate<sprt::endian::big>>(file, data, width,
						height)) {
				return true;
			}
		}
	}
	return false;
}

} // namespace stappler::bitmap::custom
