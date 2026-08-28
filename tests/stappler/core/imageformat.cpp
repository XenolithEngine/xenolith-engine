/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

/* The block arithmetic of sprt::window::ImageFormat.

WHAT WENT WRONG, and why this file exists. Every measurement of an image in this tree was written as
`getFormatBlockSize(format) * width * height * depth` - nine of them, across four backends - and
getFormatBlockSize returns BYTES PER BLOCK. For BC7 that is sixteen times the truth, for BC1
thirty-two times, and the size check in ImageData::writeData rejected a perfectly correct buffer. A
second, quieter mistake sat beside it: ASTC_*_SFLOAT_BLOCK_EXT claimed 8 bytes per block where every
ASTC block is 16, which UNDER-counts - and an under-count is a read past the end of the buffer.

THE WALK BELOW CROSS-CHECKS TWO INDEPENDENTLY WRITTEN TABLES. getImageFormatName and
getFormatBlockSize are separate switches over the same enumeration, so the name is a source of truth
about the format that owes nothing to the size table - "ASTC_8x6_SRGB_BLOCK" says its block is 8x6
and says it in a place nobody edits when they touch block sizes. A check written against a
hand-copied list of expectations would agree with a hand-copied mistake; this one cannot. */

#include "SPCommon.h"

#include <sprt/runtime/window/mode.h>

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using sprt::window::Extent2;
using sprt::window::Extent3;
using sprt::window::ImageFormat;
using sprt::window::getFormatBlockExtent;
using sprt::window::getFormatBlockSize;
using sprt::window::getFormatImageSize;
using sprt::window::getFormatRowCount;
using sprt::window::getFormatRowSize;
using sprt::window::getImageFormatName;

// The whole enumeration, by id. The core formats are 1..184 and the extensions live in five blocks
// of their own; anything in these ranges that the name table does not know is skipped, so a gap
// costs nothing and a new entry is walked the day it is added.
struct IdRange {
	uint32_t first;
	uint32_t last;
};

static constexpr IdRange s_ranges[] = {
	{1, 184}, // core
	{1'000'054'000, 1'000'054'007}, // PVRTC
	{1'000'066'000, 1'000'066'013}, // ASTC SFLOAT
	{1'000'156'000, 1'000'156'033}, // 422 and planar
	{1'000'330'000, 1'000'330'003},
	{1'000'340'000, 1'000'340'001},
};

// Every format the name table knows, in id order.
static memory::StandardInterface::VectorType<ImageFormat> enumerateFormats() {
	memory::StandardInterface::VectorType<ImageFormat> ret;
	for (auto &range : s_ranges) {
		for (uint32_t id = range.first; id <= range.last; ++id) {
			auto fmt = ImageFormat(id);
			if (getImageFormatName(fmt) != StringView("Unknown")) {
				ret.emplace_back(fmt);
			}
		}
	}
	return ret;
}

// The block extent the NAME claims, which is what the size table has to agree with.
//
// ASTC is the interesting one: its tile is spelled out in the name, so the expectation is PARSED
// rather than listed - a fourteenth ASTC size added to the enum is checked without this file being
// touched at all.
static Extent2 expectedExtentFromName(StringView name) {
	if (name.starts_with("ASTC_")) {
		StringView dims = name.sub(5); // "8x6_SRGB_BLOCK"
		uint32_t w =
				uint32_t(StringView(dims.readChars<StringView::Numbers>()).readInteger(10).get(0));
		if (dims.is('x')) {
			++dims;
		}
		uint32_t h =
				uint32_t(StringView(dims.readChars<StringView::Numbers>()).readInteger(10).get(0));
		return Extent2(w, h);
	}
	if (name.starts_with("BC") || name.starts_with("ETC2_") || name.starts_with("EAC_")) {
		return Extent2(4, 4);
	}
	if (name.starts_with("PVRTC")) {
		// Same eight bytes either way; twice the pixels at 2bpp. Exactly the distinction the block
		// extent exists to carry.
		return name.find("_2BPP_") != maxOf<size_t>() ? Extent2(8, 4) : Extent2(4, 4);
	}
	/* A 422 name means "two pixels in one block" only when the format is INTERLEAVED. The planar
	ones carry 422 in their names too - G8_B8_R8_3PLANE_422_UNORM - and mean something else by it:
	their chroma planes are subsampled and stored separately, so there is no single block covering
	two pixels to speak of. They are the formats the header names as out of scope, and this is
	where that decision is spelled out as an expectation rather than assumed. */
	if (name.find("_422_") != maxOf<size_t>() && name.find("PLANE") == maxOf<size_t>()) {
		return Extent2(2, 1);
	}
	return Extent2(1, 1);
}

static memory::StandardInterface::StringType describe(StringView name, StringView what) {
	memory::StandardInterface::StringType ret;
	ret.append("image-format: ").append(what.data(), what.size());
	ret.append(" (").append(name.data(), name.size()).append(")");
	return ret;
}

} // namespace

void performImageFormatTests() {
	sprt::cout << "\n== stappler core image format tests ==\n";

	auto formats = enumerateFormats();
	check(formats.size() >= 240, "image-format: the walk reaches the whole enumeration");

	{
		// One aggregate check per claim rather than one per format: 246 lines of `ok` say nothing,
		// and a failure names the offender anyway.
		StringView badExtent, badAstc, badBlockBytes, badParity;
		size_t compressed = 0;

		for (auto fmt : formats) {
			auto name = getImageFormatName(fmt);
			auto extent = getFormatBlockExtent(fmt);
			auto expect = expectedExtentFromName(name);
			auto blockSize = getFormatBlockSize(fmt);

			if (extent != expect && badExtent.empty()) {
				badExtent = name;
			}

			// Every ASTC block is 16 bytes, at every tile size and in every encoding. This is the
			// assertion that catches the SFLOAT variants claiming 8.
			if (name.starts_with("ASTC_") && blockSize != 16 && badAstc.empty()) {
				badAstc = name;
			}

			if (extent.width * extent.height > 2) {
				++compressed;
				// Every block-compressed format in existence here is 8 or 16 bytes per tile. A
				// format whose extent table was forgotten reads as 1x1 and drops out of this set,
				// which the extent check above has already caught.
				if (blockSize != 8 && blockSize != 16 && badBlockBytes.empty()) {
					badBlockBytes = name;
				}
			}

			// THE REGRESSION GUARD: for a one-pixel block the new arithmetic must be byte for byte
			// the pixel arithmetic it replaced, or this change moved something it had no business
			// moving.
			if (extent == Extent2(1, 1)) {
				const uint64_t before = uint64_t(blockSize) * 7 * 5 * 3 * 2;
				if (getFormatImageSize(fmt, Extent3(7, 5, 3), 2) != before && badParity.empty()) {
					badParity = name;
				}
			}
		}

		check(badExtent.empty(),
				badExtent.empty()
						? StringView(
								  "image-format: every block extent agrees with the format's name")
						: StringView(describe(badExtent, "block extent disagrees with the name")));
		check(badAstc.empty(),
				badAstc.empty() ? StringView("image-format: every ASTC block is 16 bytes")
								: StringView(describe(badAstc, "ASTC block is not 16 bytes")));
		check(badBlockBytes.empty(),
				badBlockBytes.empty()
						? StringView("image-format: every compressed block is 8 or 16 bytes")
						: StringView(
								  describe(badBlockBytes, "compressed block is neither 8 nor 16")));
		check(badParity.empty(),
				badParity.empty() ? StringView("image-format: the uncompressed path did not move")
								  : StringView(describe(badParity, "uncompressed size changed")));
		check(compressed >= 60, "image-format: the walk actually saw the compressed formats");
	}

	// Reference sizes, spelled out. The walk above proves the tables agree with each other; these
	// prove they agree with the format specifications.
	check(getFormatImageSize(ImageFormat::BC1_RGB_UNORM_BLOCK, Extent3(8, 8, 1)) == 32,
			"image-format: BC1 8x8 is 32 bytes (2x2 blocks of 8)");
	check(getFormatImageSize(ImageFormat::BC7_UNORM_BLOCK, Extent3(8, 8, 1)) == 64,
			"image-format: BC7 8x8 is 64 bytes (2x2 blocks of 16)");
	check(getFormatImageSize(ImageFormat::BC7_UNORM_BLOCK, Extent3(1, 1, 1)) == 16,
			"image-format: BC7 1x1 is one whole block");
	check(getFormatImageSize(ImageFormat::ASTC_8x8_UNORM_BLOCK, Extent3(8, 8, 1)) == 16,
			"image-format: ASTC 8x8 over 8x8 pixels is one block");
	check(getFormatImageSize(ImageFormat::ASTC_4x4_UNORM_BLOCK, Extent3(8, 8, 1)) == 64,
			"image-format: ASTC 4x4 over 8x8 pixels is four blocks");
	check(getFormatImageSize(ImageFormat::ASTC_4x4_SFLOAT_BLOCK_EXT, Extent3(8, 8, 1)) == 64,
			"image-format: ASTC SFLOAT measures the same as ASTC UNORM");
	check(getFormatImageSize(ImageFormat::ASTC_12x12_UNORM_BLOCK, Extent3(12, 12, 1)) == 16,
			"image-format: ASTC 12x12 over 12x12 pixels is one block");

	/* THE ETC2 FAMILY, WHERE ALPHA DOUBLES THE BLOCK AND ONLY SOMETIMES.

	The walk above compares the tables with EACH OTHER, so a block size that is wrong in the same
	way in both of them passes it. These compare them with the specification, and they are here
	because one of them did not: ETC2_R8G8B8A8 was recorded as eight bytes a block, which halved
	the size of every ETC2 RGBA image - and a texture upload measured at half its length is a read
	off the end of its own data.

	The three ETC2 colour formats are three different answers and the difference is exactly where
	the alpha goes. Punch-through (A1) spends one bit of the colour block and stays at 64 bits;
	full alpha prepends a whole EAC alpha block and becomes 128. EAC itself is the same story one
	channel at a time. */
	check(getFormatImageSize(ImageFormat::ETC2_R8G8B8_UNORM_BLOCK, Extent3(4, 4, 1)) == 8,
			"image-format: ETC2 RGB is 8 bytes a block");
	check(getFormatImageSize(ImageFormat::ETC2_R8G8B8A1_UNORM_BLOCK, Extent3(4, 4, 1)) == 8,
			"image-format: ETC2 RGB with punch-through alpha still fits in 8");
	check(getFormatImageSize(ImageFormat::ETC2_R8G8B8A8_UNORM_BLOCK, Extent3(4, 4, 1)) == 16,
			"image-format: ETC2 RGBA is 16 - an EAC alpha block AND a colour block");
	check(getFormatImageSize(ImageFormat::ETC2_R8G8B8A8_SRGB_BLOCK, Extent3(8, 8, 1)) == 64,
			"image-format: ... and its sRGB twin measures the same, over four blocks");
	check(getFormatImageSize(ImageFormat::EAC_R11_UNORM_BLOCK, Extent3(4, 4, 1)) == 8,
			"image-format: EAC R11 is one 8-byte block per 4x4");
	check(getFormatImageSize(ImageFormat::EAC_R11G11_UNORM_BLOCK, Extent3(4, 4, 1)) == 16,
			"image-format: EAC R11G11 is two of them");

	// Rounding UP, which is the half of this that a pixel count cannot express at all.
	check(getFormatImageSize(ImageFormat::BC7_UNORM_BLOCK, Extent3(5, 5, 1)) == 64,
			"image-format: BC7 5x5 rounds up to 2x2 blocks");
	check(getFormatImageSize(ImageFormat::ASTC_12x12_UNORM_BLOCK, Extent3(13, 13, 1)) == 64,
			"image-format: ASTC 12x12 over 13x13 pixels rounds up to 2x2 blocks");
	check(getFormatRowCount(ImageFormat::BC7_UNORM_BLOCK, 5) == 2,
			"image-format: five pixel rows of BC7 are two block rows");
	check(getFormatRowSize(ImageFormat::BC7_UNORM_BLOCK, 8) == 32,
			"image-format: a BC7 row of 8 pixels is 32 bytes");

	// The uncompressed path, named rather than only walked.
	check(getFormatImageSize(ImageFormat::R8G8B8A8_UNORM, Extent3(8, 8, 1)) == 256,
			"image-format: RGBA8 8x8 is 256 bytes");
	check(getFormatImageSize(ImageFormat::R8_UNORM, Extent3(1, 1, 1)) == 1,
			"image-format: R8 1x1 is one byte");
	check(getFormatRowSize(ImageFormat::R8G8B8A8_UNORM, 8) == 32,
			"image-format: an RGBA8 row of 8 pixels is 32 bytes");
	check(getFormatRowCount(ImageFormat::R8G8B8A8_UNORM, 8) == 8,
			"image-format: an uncompressed image has one block row per pixel row");

	// 422: two pixels in one block, so a row is half as many blocks as it is pixels - and an odd
	// width still pays for the whole trailing block.
	check(getFormatRowSize(ImageFormat::G8B8G8R8_422_UNORM, 8) == 16,
			"image-format: a 422 row of 8 pixels is 16 bytes, not 32");
	check(getFormatRowSize(ImageFormat::G8B8G8R8_422_UNORM, 7) == 16,
			"image-format: a 422 row of 7 pixels still pays for four blocks");

	// Depth and layers multiply the whole image; neither replaces the other.
	check(getFormatImageSize(ImageFormat::R8_UNORM, Extent3(4, 4, 4)) == 64,
			"image-format: depth multiplies");
	check(getFormatImageSize(ImageFormat::R8G8B8A8_UNORM, Extent3(8, 8, 1), 6) == 1'536,
			"image-format: array layers multiply");
	check(getFormatImageSize(ImageFormat::BC7_UNORM_BLOCK, Extent3(8, 8, 1), 6) == 384,
			"image-format: array layers multiply a compressed image too");

	// A format nobody declared measures as nothing rather than as something plausible.
	check(getFormatImageSize(ImageFormat::Undefined, Extent3(8, 8, 1)) == 0,
			"image-format: an undefined format has no size");
}

} // namespace STAPPLER_VERSIONIZED stappler
