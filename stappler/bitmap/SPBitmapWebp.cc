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
#include "SPFilepath.h"
#include "SPLog.h"
#include "SPFilesystem.h"
#include "webp/decode.h"
#include "webp/encode.h"

namespace STAPPLER_VERSIONIZED stappler::bitmap::webp {

static bool isWebpLossless(const uint8_t *data, size_t dataLen) {
	if (dataLen <= 12) {
		return false;
	}

	static const char *WEBP_RIFF = "RIFF";
	static const char *WEBP_WEBP = "WEBPVP8L";

	return memcmp(data, WEBP_RIFF, 4) == 0
			&& memcmp(static_cast<const unsigned char *>(data) + 8, WEBP_WEBP, 8) == 0;
}

static bool getWebpLosslessImageSize(const io::Producer &file, StackBuffer<512> &data,
		uint32_t &width, uint32_t &height) {
	if (isWebpLossless(data.data(), data.size())) {
		auto reader = BytesViewTemplate<Endian::Big>(data.data() + 21, 4);

		auto b0 = reader.readUnsigned();
		auto b1 = reader.readUnsigned();
		auto b2 = reader.readUnsigned();
		auto b3 = reader.readUnsigned();

		// first 14 bits - width, last 14 bits - height

		width = (b0 | ((b1 & 0x3F) << 8)) + 1;
		height = (((b3 & 0xF) << 10) | (b2 << 2) | ((b1 & 0xC0) >> 6)) + 1;

		return true;
	}
	return false;
}

static bool isWebp(const uint8_t *data, size_t dataLen) {
	if (dataLen <= 12) {
		return false;
	}

	static const char *WEBP_RIFF = "RIFF";
	static const char *WEBP_WEBP = "WEBP";

	return memcmp(data, WEBP_RIFF, 4) == 0
			&& memcmp(static_cast<const unsigned char *>(data) + 8, WEBP_WEBP, 4) == 0;
}

static bool getWebpImageSize(const io::Producer &file, StackBuffer<512> &data, uint32_t &width,
		uint32_t &height) {
	if (isWebp(data.data(), data.size())) {
		auto reader = BytesViewTemplate<Endian::Little>(data.data() + 24, 6);

		auto b0 = reader.readUnsigned();
		auto b1 = reader.readUnsigned();
		auto b2 = reader.readUnsigned();
		auto b3 = reader.readUnsigned();
		auto b4 = reader.readUnsigned();
		auto b5 = reader.readUnsigned();

		// first 14 bits - width, last 14 bits - height

		width = (b0 | (b1 << 8) | (b2 << 8)) + 1;
		height = (b3 | (b4 << 8) | (b5 << 8)) + 1;

		return true;
	}
	return false;
}

static bool infoWebp(WebPDecoderConfig *config, const uint8_t *inputData, size_t size,
		ImageInfo &outputData) {
	if (WebPInitDecoderConfig(config) == 0) {
		return false;
	}
	if (WebPGetFeatures(inputData, size, &config->input) != VP8_STATUS_OK) {
		return false;
	}
	if (config->input.width == 0 || config->input.height == 0) {
		return false;
	}

	outputData.color = config->input.has_alpha ? PixelFormat::RGBA8888 : PixelFormat::RGB888;
	outputData.width = config->input.width;
	outputData.height = config->input.height;

	outputData.alpha =
			(config->input.has_alpha != 0) ? AlphaFormat::Unpremultiplied : AlphaFormat::Opaque;
	outputData.stride = (uint32_t)outputData.width * getBytesPerPixel(outputData.color);
	return true;
}

static bool infoWebp(const uint8_t *inputData, size_t size, ImageInfo &outputData) {
	WebPDecoderConfig config;
	return infoWebp(&config, inputData, size, outputData);
}

static bool loadWebp(const uint8_t *inputData, size_t size, BitmapWriter &outputData) {
	WebPDecoderConfig config;
	if (!infoWebp(&config, inputData, size, outputData)) {
		return false;
	}

	if (outputData.getStride) {
		outputData.stride = max((uint32_t)outputData.getStride(outputData.target, outputData.color,
										outputData.width),
				(uint32_t)outputData.width * getBytesPerPixel(outputData.color));
	}

	outputData.resize(outputData.target, outputData.stride * outputData.height);

	config.output.colorspace = config.input.has_alpha ? MODE_RGBA : MODE_RGB;
	config.output.u.RGBA.rgba = outputData.getData(outputData.target, 0);
	config.output.u.RGBA.stride = outputData.stride;
	config.output.u.RGBA.size = outputData.stride * outputData.height;
	config.output.is_external_memory = 1;

	if (WebPDecode(inputData, size, &config) != VP8_STATUS_OK) {
		outputData.clear(outputData.target);
		return false;
	}

	return true;
}

struct WebpStruct {
	static bool isWebpSupported(PixelFormat format) {
		switch (format) {
		case PixelFormat::A8:
		case PixelFormat::I8:
		case PixelFormat::IA88:
		case PixelFormat::Auto:
			log::source().error("Bitmap", "Webp supports only RGB888 and RGBA8888");
			return false;
		default: break;
		}
		return true;
	}

	static int FileWriter(const uint8_t *data, size_t data_size, const WebPPicture *const pic) {
		FILE *const out = (FILE *)pic->custom_ptr;
		return data_size ? (fwrite(data, data_size, 1, out) == 1) : 1;
	}

	FILE *fp = nullptr;
	BitmapWriter *out = nullptr;

	WebPConfig config;
	WebPPicture pic;
	WebPMemoryWriter writer;

	bool pictureInit = false;
	bool memoryInit = false;
	bool valid = true;

	~WebpStruct() {
		if (pictureInit) {
			WebPPictureFree(&pic);
			pictureInit = false;
		}

		if (memoryInit) {
			WebPMemoryWriterClear(&writer);
			memoryInit = false;
		}

		if (fp) {
			fclose(fp);
		}
	}

	WebpStruct(bool lossless) {
		if (!WebPPictureInit(&pic)) {
			valid = false;
		}

		if (lossless) {
			if (!WebPConfigPreset(&config, WEBP_PRESET_ICON, 100.0f)) {
				valid = false;
			}

			config.lossless = 1;
			config.method = 6;
		} else {
			if (!WebPConfigPreset(&config, WEBP_PRESET_PICTURE, 90.0f)) {
				valid = false;
			}

			config.lossless = 0;
			config.method = 6;
		}

		if (!WebPValidateConfig(&config)) {
			valid = false;
		}
	}

	WebpStruct(BitmapWriter *v, bool lossless) : WebpStruct(lossless) { out = v; }

	WebpStruct(const FileInfo &filename, bool lossless) : WebpStruct(lossless) {
		filesystem::enumerateWritablePaths(filename, filesystem::Access::None,
				[&](StringView str, FileFlags) {
			fp = filesystem::native::fopen_fn(str, "wb");
			if (fp) {
				return false;
			}
			return true;
		});

		if (!fp) {
			log::source().error("Bitmap", "fail to open file ", filename, " to write webp data");
			valid = false;
			return;
		}
	}

	explicit operator bool() const { return valid; }

	bool write(const uint8_t *data, BitmapWriter &state) {
		if (!valid) {
			return false;
		}

		if (!fp && !out) {
			return false;
		}

		WebPPicture pic;
		WebPPictureInit(&pic);

		pic.use_argb = 1;
		pic.width = state.width;
		pic.height = state.height;

		if (state.stride == 0) {
			state.stride = getBytesPerPixel(state.color) * state.width;
		}

		switch (state.color) {
		case PixelFormat::A8:
		case PixelFormat::I8:
		case PixelFormat::IA88:
		case PixelFormat::Auto: return false;
		case PixelFormat::RGB888: WebPPictureImportRGB(&pic, data, state.stride); break;
		case PixelFormat::RGBA8888: WebPPictureImportRGBA(&pic, data, state.stride); break;
		}
		pictureInit = true;

		if (fp) {
			pic.writer = FileWriter;
			pic.custom_ptr = fp;
		} else {
			WebPMemoryWriterInit(&writer);
			pic.writer = WebPMemoryWrite;
			pic.custom_ptr = &writer;
			memoryInit = true;
		}

		if (!WebPEncode(&config, &pic)) {
			return false;
		}

		if (out) {
			out->resize(out->target, uint32_t(writer.size));
			memcpy(out->getData(out->target, 0), writer.mem, writer.size);
		}

		return true;
	}
};

static bool saveWebpLossless(const FileInfo &filename, const uint8_t *data, BitmapWriter &state,
		bool invert) {
	if (!WebpStruct::isWebpSupported(state.color)) {
		return false;
	}
	if (invert) {
		log::source().error("Bitmap", "Inverted output is not supported for webp");
		return false;
	}

	WebpStruct coder(filename, true);
	return coder.write(data, state);
}

static bool writeWebpLossless(const uint8_t *data, BitmapWriter &state, bool invert) {
	if (!WebpStruct::isWebpSupported(state.color) || invert) {
		if (invert) {
			log::source().error("Bitmap", "Inverted output is not supported for webp");
		}
		return false;
	}

	WebpStruct coder(&state, true);
	return coder.write(data, state);
}

static bool saveWebpLossy(const FileInfo &filename, const uint8_t *data, BitmapWriter &state,
		bool invert) {
	if (!WebpStruct::isWebpSupported(state.color) || invert) {
		if (invert) {
			log::source().error("Bitmap", "Inverted output is not supported for webp");
		}
		return false;
	}

	WebpStruct coder(filename, false);
	return coder.write(data, state);
}

static bool writeWebpLossy(const uint8_t *data, BitmapWriter &state, bool invert) {
	if (!WebpStruct::isWebpSupported(state.color) || invert) {
		if (invert) {
			log::source().error("Bitmap", "Inverted output is not supported for webp");
		}
		return false;
	}

	WebpStruct coder(&state, false);
	return coder.write(data, state);
}

} // namespace stappler::bitmap::webp
