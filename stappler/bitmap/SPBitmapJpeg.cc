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
#include "jpeglib.h"
#include <setjmp.h>

namespace STAPPLER_VERSIONIZED stappler::bitmap::jpeg {

struct JpegError {
	struct jpeg_error_mgr pub; /* "public" fields */
	jmp_buf setjmp_buffer; /* for return to caller */

	static void ErrorExit(j_common_ptr cinfo) {
		JpegError *myerr = (JpegError *)cinfo->err;
		char buffer[JMSG_LENGTH_MAX];
		(*cinfo->err->format_message)(cinfo, buffer);
		log::source().error("JPEG", "jpeg error: %s", buffer);
		longjmp(myerr->setjmp_buffer, 1);
	}
};

static bool isJpg(const uint8_t *data, size_t dataLen) {
	if (dataLen <= 4) {
		return false;
	}

	static const unsigned char JPG_SOI[] = {0xFF, 0xD8};
	return memcmp(data, JPG_SOI, 2) == 0;
}

static bool getJpegImageSize(const io::Producer &file, StackBuffer<512> &data, uint32_t &width,
		uint32_t &height) {
	if (isJpg(data.data(), data.size())) {
		size_t offset = 2;
		uint16_t len = 0;
		uint8_t marker = 0;

		auto reader = BytesViewNetwork(data.data() + 2, data.size() - 2);
		while (reader.is((uint8_t)0xFF)) {
			++reader;
			++offset;
		}

		marker = reader.readUnsigned();
		len = reader.readUnsigned16();

		while (marker < 0xC0 || marker > 0xCF || marker == 0xC4) {
			offset += 1 + len;
			data.clear();

			if (file.seekAndRead(offset, data, 12) != 12) {
				return false;
			}

			if (data.size() >= 12) {
				reader = data.get<BytesViewNetwork>();

				while (reader.is((uint8_t)0xFF)) {
					++reader;
					++offset;
				}

				marker = reader.readUnsigned();
				len = reader.readUnsigned16();
			} else {
				reader.clear();
				break;
			}
		}

		if (reader >= 5 && marker >= 0xC0 && marker <= 0xCF && marker != 0xC4) {
			++reader;
			height = reader.readUnsigned16();
			width = reader.readUnsigned16();
			return true;
		}

		return false;
	}
	return false;
}

struct JpegReadStruct {
	~JpegReadStruct() {
		if (initialized) {
			jpeg_destroy_decompress(&cinfo);
			initialized = false;
		}
	}

	JpegReadStruct() {
		cinfo.err = jpeg_std_error(&jerr.pub);
		jerr.pub.error_exit = &JpegError::ErrorExit;
	}

	bool init(const uint8_t *inputData, size_t size) {
		if (setjmp(jerr.setjmp_buffer)) {
			return false;
		}

		jpeg_create_decompress(&cinfo);
		initialized = true;
		jpeg_mem_src(&cinfo, const_cast<unsigned char *>(inputData), size);

		/* reading the image header which contains image information */
		jpeg_read_header(&cinfo, boolean(TRUE));

		return true;
	}

	bool info(ImageInfo &info) {
		if (setjmp(jerr.setjmp_buffer)) {
			return false;
		}

		// we only support RGB or grayscale
		if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
			info.color = (info.color == PixelFormat::A8 ? PixelFormat::A8 : PixelFormat::I8);
		} else if (cinfo.jpeg_color_space == JCS_YCCK || cinfo.jpeg_color_space == JCS_CMYK) {
			cinfo.out_color_space = JCS_CMYK;
			info.color = PixelFormat::RGB888;
		} else {
			cinfo.out_color_space = JCS_RGB;
			info.color = PixelFormat::RGB888;
		}

		if (info.color == PixelFormat::I8 || info.color == PixelFormat::RGB888) {
			info.alpha = AlphaFormat::Opaque;
		} else {
			info.alpha = AlphaFormat::Unpremultiplied;
		}

		jpeg_calc_output_dimensions(&cinfo);

		info.width = cinfo.output_width;
		info.height = cinfo.output_height;
		info.stride = cinfo.output_width * getBytesPerPixel(info.color);

		return true;
	}

	bool load(BitmapWriter &outputData) {
		if (!info(outputData)) {
			return false;
		}

		if (setjmp(jerr.setjmp_buffer)) {
			return false;
		}

		if (outputData.getStride) {
			outputData.stride =
					max(outputData.getStride(outputData.target, outputData.color, outputData.width),
							uint32_t(cinfo.output_width * getBytesPerPixel(outputData.color)));
		}

		/* Start decompression jpeg here */
		jpeg_start_decompress(&cinfo);


		auto dataLen = outputData.height * outputData.stride;
		outputData.resize(outputData.target, dataLen);

		JSAMPROW row_pointer[1] = {0};
		uint32_t location = 0;

		if (cinfo.out_color_space == JCS_CMYK || cinfo.out_color_space == JCS_YCCK) {
			memory::PoolInterface::BytesType buf;
			buf.resize(cinfo.output_width * cinfo.output_components);
			while (cinfo.output_scanline < cinfo.output_height) {
				row_pointer[0] = buf.data();
				jpeg_read_scanlines(&cinfo, row_pointer, 1);

				auto loc = outputData.getData(outputData.target, location);
				for (size_t i = 0; i < cinfo.output_width; ++i) {
					*loc++ = (buf[i * 4]) * (buf[i * 4 + 3]) / 255;
					*loc++ = (buf[i * 4 + 1]) * (buf[i * 4 + 3]) / 255;
					*loc++ = (buf[i * 4 + 2]) * (buf[i * 4 + 3]) / 255;
				}
				location += outputData.stride;
			}
		} else {
			/* now actually read the jpeg into the raw buffer */
			/* read one scan line at a time */
			while (cinfo.output_scanline < cinfo.output_height) {
				row_pointer[0] = outputData.getData(outputData.target, location);
				location += outputData.stride;
				jpeg_read_scanlines(&cinfo, row_pointer, 1);
			}
		}

		return true;
	}

	bool initialized = false;
	struct jpeg_decompress_struct cinfo;
	struct JpegError jerr;
};

struct JpegWriteStruct {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	bool valid = false;

	FILE *fp = nullptr;
	BitmapWriter *vec = nullptr;

	unsigned char *mem = nullptr;
	unsigned long memSize = 0;

	~JpegWriteStruct() {
		jpeg_destroy_compress(&cinfo);

		if (fp) {
			fclose(fp);
		}
		if (mem) {
			free(mem);
		}
	}

	JpegWriteStruct() {
		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_compress(&cinfo);
	}

	JpegWriteStruct(BitmapWriter *v) : JpegWriteStruct() {
		vec = v;
		jpeg_mem_dest(&cinfo, &mem, &memSize);
		valid = true;
	}

	JpegWriteStruct(const FileInfo &filename) : JpegWriteStruct() {
		filesystem::enumerateWritablePaths(filename, filesystem::Access::None,
				[&](StringView str, FileFlags) {
			fp = filesystem::native::fopen_fn(str, "wb");
			if (fp) {
				return false;
			}
			return true;
		});

		if (!fp) {
			log::source().error("Bitmap", "fail to open file ", filename, " to write jpeg data");
			valid = false;
			return;
		}

		jpeg_stdio_dest(&cinfo, fp);
		valid = true;
	}

	bool write(const uint8_t *data, BitmapWriter &state, bool invert = false) {
		if (!valid) {
			return false;
		}

		/* this is a pointer to one row of image data */
		JSAMPROW row_pointer[1];

		/* Setting the parameters of the output file here */
		cinfo.image_width = state.width;
		cinfo.image_height = state.height;
		cinfo.input_components = getBytesPerPixel(state.color);

		switch (state.color) {
		case PixelFormat::A8:
		case PixelFormat::I8:
			cinfo.input_components = 1;
			cinfo.in_color_space = JCS_GRAYSCALE;
			break;
		case PixelFormat::RGB888:
			cinfo.input_components = 3;
			cinfo.in_color_space = JCS_RGB;
			break;
		default:
			log::source().error("JPEG", "Color format is not supported by JPEG!");
			return false;
			break;
		}

		/* default compression parameters, we shouldn't be worried about these */
		jpeg_set_defaults(&cinfo);
		jpeg_set_quality(&cinfo, 90, boolean(TRUE));
		/* Now do the compression .. */
		jpeg_start_compress(&cinfo, boolean(TRUE));
		/* like reading a file, this time write one row at a time */
		while (cinfo.next_scanline < cinfo.image_height) {
			row_pointer[0] = (JSAMPROW)&data[(invert ? (state.height - 1 - cinfo.next_scanline)
													 : cinfo.next_scanline)
					* state.stride];
			jpeg_write_scanlines(&cinfo, row_pointer, 1);
		}
		/* similar to read file, clean up after we're done compressing */
		jpeg_finish_compress(&cinfo);

		if (vec) {
			if (memSize) {
				vec->assign(vec->target, mem, uint32_t(memSize));
			} else {
				return false;
			}
		}

		return true;
	}
};

static bool infoJpg(const uint8_t *inputData, size_t size, ImageInfo &outputData) {
	JpegReadStruct jpegStruct;
	return jpegStruct.init(inputData, size) && jpegStruct.info(outputData);
}

static bool loadJpg(const uint8_t *inputData, size_t size, BitmapWriter &outputData) {
	JpegReadStruct jpegStruct;
	return jpegStruct.init(inputData, size) && jpegStruct.load(outputData);
}

static bool saveJpeg(const FileInfo &filename, const uint8_t *data, BitmapWriter &state,
		bool invert) {
	JpegWriteStruct s(filename);
	return s.write(data, state, invert);
}

static bool writeJpeg(const uint8_t *data, BitmapWriter &state, bool invert) {
	JpegWriteStruct s(&state);
	return s.write(data, state, invert);
}

} // namespace stappler::bitmap::jpeg
