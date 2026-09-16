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

/* The two guards `Bitmap::resample` opens with - the target's and the source's.

WHY A SECTION FOR TWO `if`s. Because both of them were wrong for as long as they had existed, and
neither was wrong in a way anybody could see:

  * a target with EITHER extent of one or less was refused and answered with an EMPTY bitmap, which
    is what a failed allocation looks like - so a downscale to `N x 1` did not come out coarse, it
    came out MISSING. Downstream that is a sprite that silently disappears from an atlas, and it was
    found by a build doing exactly that;
  * the second half of that same condition read `max(height, height)`, so the target's WIDTH was the
    one extent nothing bounded. That is not merely a diagnostic that fails to fire: `alloc` computes
    `w * bpp` and then `stride * h` in 32 bits, and refusing an unbounded target is precisely what
    `checkImageDataSize` does on the decode path, where its own comment calls the alternative "the
    classic image-decoder heap overflow".

Neither could be caught by any test in this tree, because there was no test in this tree that
resampled anything. Four assertions is what that costs, and it is the whole of the section: the
resampler's ARITHMETIC is not asserted here and is not this file's business - what is asserted is
which calls it is allowed to refuse. */

#include "SPCommon.h"

#include "SPBitmap.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

// A source with something in it, so that "not empty" and "the right length" are two different
// statements about the answer. The pattern is a gradient across the row, which no filter can turn
// into a constant - a check that resampled a flat image would pass with the arithmetic gone.
static mem_std::Bitmap makeSource(uint32_t width, uint32_t height) {
	mem_std::Bitmap bmp;
	bmp.alloc(width, height, bitmap::PixelFormat::RGBA8888,
			bitmap::AlphaFormat::Unpremultiplied, width * 4);
	for (uint32_t y = 0; y < height; ++y) {
		auto row = bmp.dataPtr() + size_t(y) * bmp.stride();
		for (uint32_t x = 0; x < width; ++x) {
			row[x * 4 + 0] = uint8_t(x * 255 / (width - 1));
			row[x * 4 + 1] = uint8_t(y * 255 / (height - 1));
			row[x * 4 + 2] = 0;
			row[x * 4 + 3] = 255;
		}
	}
	return bmp;
}

} // namespace

void performBitmapResampleTests() {
	auto src = makeSource(8, 8);
	check(!src.empty() && src.width() == 8 && src.height() == 8,
			"bitmap-resample: the source is eight by eight");

	/* ---- an extent of ONE is a picture, not a refusal ---------------------------------------- */

	{
		auto out = src.resample(4, 1);
		check(!out.empty() && out.width() == 4 && out.height() == 1,
				"bitmap-resample: 8x8 -> 4x1 answers a picture and not an empty bitmap");
		// Nonzero AND consistent. Length alone would be satisfied by an empty answer - `0` is what
		// `stride * height` comes to for one - which is exactly the answer this section exists to
		// refuse, so the assertion has to say both halves.
		check(out.data().size() > 0 && out.data().size() == out.stride() * out.height(),
				"bitmap-resample: ... of the length its own extent and stride describe");
	}

	{
		// The end of a mip chain, and the case that used to stop it at 2x2.
		auto out = src.resample(1, 1);
		check(!out.empty() && out.width() == 1 && out.height() == 1,
				"bitmap-resample: 8x8 -> 1x1 does too, which is where a mip chain ends");
	}

	{
		// Zero IS the refusal, and it has to stay one: there is no such picture, and the
		// resampler's own precondition is `dst > 0`.
		auto out = src.resample(0, 4);
		check(out.empty(), "bitmap-resample: a target of zero is still refused");
	}

	/* ---- the cap applies to BOTH extents of the target ---------------------------------------- */

	{
		// The width alone, which is the extent the old `max(height, height)` never looked at. A
		// refusal here is cheap; accepting it means `alloc` multiplying an unbounded width by four
		// in 32 bits, which is the overflow the decode path refuses by name.
		auto out = src.resample(bitmap::MaxImageDimension + 1, 4);
		check(out.empty(), "bitmap-resample: a target wider than the cap is refused");

		auto tall = src.resample(4, bitmap::MaxImageDimension + 1);
		check(tall.empty(), "bitmap-resample: ... and so is one taller than it");
	}
}

} // namespace stappler
