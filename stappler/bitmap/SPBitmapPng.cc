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
#include "png.h"

namespace STAPPLER_VERSIONIZED stappler::bitmap::png {

struct ReadState {
	const uint8_t *data = nullptr;
	size_t offset = 0;
};

static bool isPng(const uint8_t *data, size_t dataLen) {
	if (dataLen <= 8) {
		return false;
	}

	static const unsigned char PNG_SIGNATURE[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
	return memcmp(PNG_SIGNATURE, data, sizeof(PNG_SIGNATURE)) == 0;
}

SP_UNUSED static bool getPngImageSize(const io::Producer &file, StackBuffer<512> &data,
		uint32_t &width, uint32_t &height) {
	if (isPng(data.data(), data.size()) && data.size() >= 24) {
		auto reader = BytesViewNetwork(data.data() + 16, 8);

		width = reader.readUnsigned32();
		height = reader.readUnsigned32();

		return true;
	}
	return false;
}

static void readDynamicData(png_structp pngPtr, png_bytep data, png_size_t length) {
	auto state = (ReadState *)png_get_io_ptr(pngPtr);
	memcpy(data, state->data + state->offset, length);
	state->offset += length;
}

struct PngReadStruct {
	~PngReadStruct() {
		if (png_ptr || info_ptr) {
			png_destroy_read_struct(png_ptr ? &png_ptr : nullptr, info_ptr ? &info_ptr : nullptr,
					NULL);
			png_ptr = nullptr;
			info_ptr = nullptr;
		}
	}

	PngReadStruct() { }

	bool init(const uint8_t *inputData, size_t size) {
		png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png_ptr == NULL) {
			log::source().error("libpng", "fail to create read struct");
			return false;
		}

		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == NULL) {
			log::source().error("libpng", "fail to create info struct");
			png_destroy_read_struct(&png_ptr, NULL, NULL);
			return false;
		}

		if (setjmp(png_jmpbuf(png_ptr))) {
			log::source().error("libpng", "error in processing (setjmp return)");
			return false;
		}

		state.data = inputData;
		state.offset = 0;
		png_set_read_fn(png_ptr, (png_voidp)&state, readDynamicData);

#ifdef PNG_ARM_NEON_API_SUPPORTED
		png_set_option(png_ptr, PNG_ARM_NEON, PNG_OPTION_ON);
#endif
		png_read_info(png_ptr, info_ptr);
		return true;
	}

	bool info(ImageInfo &info) {
		if (setjmp(png_jmpbuf(png_ptr))) {
			log::source().error("libpng", "error in processing (setjmp return)");
			return false;
		}

		info.width = png_get_image_width(png_ptr, info_ptr);
		info.height = png_get_image_height(png_ptr, info_ptr);

		png_byte bitdepth = png_get_bit_depth(png_ptr, info_ptr);
		png_uint_32 color_type = png_get_color_type(png_ptr, info_ptr);

		if (color_type == PNG_COLOR_TYPE_PALETTE) {
			png_set_palette_to_rgb(png_ptr);
		}
		if (color_type == PNG_COLOR_TYPE_GRAY && bitdepth < 8) {
			bitdepth = 8;
			png_set_expand_gray_1_2_4_to_8(png_ptr);
		}
		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
			png_set_tRNS_to_alpha(png_ptr);
		}
		if (bitdepth == 16) {
			png_set_strip_16(png_ptr);
		}
		if (bitdepth < 8) {
			png_set_packing(png_ptr);
		}

		png_read_update_info(png_ptr, info_ptr);
		bitdepth = png_get_bit_depth(png_ptr, info_ptr);
		color_type = png_get_color_type(png_ptr, info_ptr);
		auto rowbytes = png_get_rowbytes(png_ptr, info_ptr);

		if (color_type == PNG_COLOR_TYPE_GRAY) {
			info.color = (info.color == PixelFormat::A8 ? PixelFormat::A8 : PixelFormat::I8);
		} else if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
			info.color = PixelFormat::IA88;
		} else if (color_type == PNG_COLOR_TYPE_RGB) {
			info.color = PixelFormat::RGB888;
		} else if (color_type == PNG_COLOR_TYPE_RGBA) {
			info.color = PixelFormat::RGBA8888;
		} else {
			info.width = 0;
			info.height = 0;
			info.stride = 0;
			log::format(log::Error, "libpng", SP_LOCATION, "unsupported color type: %u",
					(unsigned int)color_type);
			return false;
		}

		info.stride = (uint32_t)rowbytes;

		if (info.color == PixelFormat::I8 || info.color == PixelFormat::RGB888) {
			info.alpha = AlphaFormat::Opaque;
		} else {
			info.alpha = AlphaFormat::Unpremultiplied;
		}

		return true;
	}

	bool load(BitmapWriter &outputData) {
		if (!info(outputData)) {
			return false;
		}

		if (setjmp(png_jmpbuf(png_ptr))) {
			log::source().error("libpng", "error in processing (setjmp return)");
			return false;
		}

		if (outputData.getStride) {
			auto rowbytes = png_get_rowbytes(png_ptr, info_ptr);
			outputData.stride = max((uint32_t)outputData.getStride(outputData.target,
											outputData.color, outputData.width),
					(uint32_t)rowbytes);
		}

		png_bytep row_pointers[outputData.height];

		auto dataLen = outputData.stride * outputData.height;

		outputData.resize(outputData.target, dataLen);

		for (uint32_t i = 0; i < outputData.height; ++i) {
			row_pointers[i] = outputData.getData(outputData.target, i * outputData.stride);
		}

		png_read_image(png_ptr, row_pointers);
		png_read_end(png_ptr, nullptr);
		return true;
	}

	ReadState state;
	png_structp png_ptr = nullptr;
	png_infop info_ptr = nullptr;
};

struct PngWriteStruct {
	int bit_depth = 8;
	png_structp png_ptr = nullptr;
	png_infop info_ptr = nullptr;
	bool valid = false;

	FILE *fp = nullptr;
	BitmapWriter *out = nullptr;

	~PngWriteStruct() {
		if (png_ptr != nullptr) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
		}

		if (fp) {
			fclose(fp);
		}
	}

	PngWriteStruct() {
		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png_ptr == nullptr) {
			log::source().error("libpng", "fail to create write struct");
			return;
		}

		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == nullptr) {
			log::source().error("libpng", "fail to create info struct");
			return;
		}
	}

	PngWriteStruct(BitmapWriter *v) : PngWriteStruct() {
		out = v;
		valid = true;
	}

	PngWriteStruct(const FileInfo &filename) : PngWriteStruct() {
		filesystem::enumerateWritablePaths(filename, filesystem::Access::None,
				[&](StringView str, FileFlags) {
			fp = filesystem::native::fopen_fn(str, "wb");
			if (fp) {
				return false;
			}
			return true;
		});

		if (!fp) {
			log::source().error("Bitmap", "fail to open file ", filename, " to write png data");
			valid = false;
			return;
		}

		valid = true;
	}

	static void writePngFn(png_structp png_ptr, png_bytep data, png_size_t length) {
		auto out = (BitmapWriter *)png_get_io_ptr(png_ptr);
		out->push(out->target, data, uint32_t(length));
	}

	bool write(const uint8_t *data, BitmapWriter &state, bool invert = false) {
		if (!valid) {
			return false;
		}

		if (!fp && !out) {
			return false;
		}

		if (setjmp(png_jmpbuf(png_ptr))) {
			log::source().error("libpng", "error in processing (setjmp return)");
			return false;
		}

		if (state.stride == 0) {
			state.stride = getBytesPerPixel(state.color) * state.width;
		}

		int color_type = 0;
		switch (state.color) {
		case PixelFormat::A8:
		case PixelFormat::I8: color_type = PNG_COLOR_TYPE_GRAY; break;
		case PixelFormat::IA88: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
		case PixelFormat::RGB888: color_type = PNG_COLOR_TYPE_RGB; break;
		case PixelFormat::RGBA8888: color_type = PNG_COLOR_TYPE_RGBA; break;
		default: return false;
		}

		/* Set image attributes. */
		png_set_IHDR(png_ptr, info_ptr, state.width, state.height, bit_depth, color_type,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

		/* Initialize rows of PNG. */
		png_byte *row_pointers[state.height];
		for (size_t i = 0; i < state.height; ++i) {
			row_pointers[i] = (png_byte *)data
					+ (invert ? state.stride * (state.height - i - 1) : state.stride * i);
		}

		if (fp) {
			png_init_io(png_ptr, fp);
		} else {
			png_set_write_fn(png_ptr, out, &writePngFn, nullptr);
		}

		png_set_rows(png_ptr, info_ptr, row_pointers);
		png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
		return true;
	}
};

SP_UNUSED static bool infoPng(const uint8_t *inputData, size_t size, ImageInfo &outputData) {
	PngReadStruct pngStruct;
	return pngStruct.init(inputData, size) && pngStruct.info(outputData);
}

SP_UNUSED static bool loadPng(const uint8_t *inputData, size_t size, BitmapWriter &outputData) {
	PngReadStruct pngStruct;
	return pngStruct.init(inputData, size) && pngStruct.load(outputData);
}

SP_UNUSED static bool savePng(const FileInfo &filename, const uint8_t *data, BitmapWriter &state,
		bool invert) {
	PngWriteStruct s(filename);
	return s.write(data, state, invert);
}

SP_UNUSED static bool writePng(const uint8_t *data, BitmapWriter &state, bool invert) {
	PngWriteStruct s(&state);
	return s.write(data, state, invert);
}

} // namespace stappler::bitmap::png
