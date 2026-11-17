/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "SPCommon.h"
#include "SPBitmap.h"

#include <png.h>

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler {

void performBitmapEncodeTests() {
	using test::check;

	mem_std::Bitmap source;
	source.alloc(32, 16, bitmap::PixelFormat::RGBA8888);
	for (size_t i = 0; i < source.size(); ++i) { source.dataPtr()[i] = uint8_t(i * 17 + i / 32); }
	auto defaults = source.write(bitmap::FileFormat::Png);
	source.setPngEncodeEffort(0, PNG_FILTER_NONE);
	auto expected = source.write(bitmap::FileFormat::Png);
	check(!expected.empty() && expected != defaults,
			"bitmap-encode: explicit compression and filtering change the encoded PNG");

	mem_std::Bitmap moved(sp::move(source));
	check(moved.write(bitmap::FileFormat::Png) == expected,
			"bitmap-encode: move construction preserves PNG encode options");

	mem_std::Bitmap assigned;
	assigned.setPngEncodeEffort(9, PNG_FILTER_UP);
	assigned = sp::move(moved);
	check(assigned.write(bitmap::FileFormat::Png) == expected,
			"bitmap-encode: move assignment replaces the destination encode options");

	mem_std::Bitmap decoded(expected);
	check(decoded.width() == assigned.width() && decoded.height() == assigned.height()
					&& decoded.data() == assigned.data(),
			"bitmap-encode: the PNG preserves every source pixel");
}

} // namespace STAPPLER_VERSIONIZED stappler
