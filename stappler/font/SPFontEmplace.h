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

#ifndef CORE_FONT_SPFONTEMPLACE_H_
#define CORE_FONT_SPFONTEMPLACE_H_

#include "SPFont.h"

namespace STAPPLER_VERSIONIZED stappler::font {

struct SP_PUBLIC EmplaceCharInterface {
	uint16_t (*getX) (void *) = nullptr;
	uint16_t (*getY) (void *) = nullptr;
	uint16_t (*getWidth) (void *) = nullptr;
	uint16_t (*getHeight) (void *) = nullptr;
	void (*setX) (void *, uint16_t) = nullptr;
	void (*setY) (void *, uint16_t) = nullptr;
	void (*setTex) (void *, uint16_t) = nullptr;
};

SP_PUBLIC geom::Extent2 emplaceChars(const EmplaceCharInterface &, const SpanView<void *> &,
		float totalSquare = std::numeric_limits<float>::quiet_NaN());

}

#endif /* CORE_FONT_SPFONTEMPLACE_H_ */
