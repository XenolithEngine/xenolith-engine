/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTFLOATCONTEXT_H_
#define EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTFLOATCONTEXT_H_

#include "SPDocLayout.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::document {

struct LayoutBlock;

struct SP_PUBLIC FloatContext {
	template <typename Value>
	using Vector = typename memory::PoolInterface::VectorType<Value>;

	LayoutBlock *root;
	float pageHeight = nan();
	Vector<Rect> floatRight;
	Vector<Rect> floatLeft;

	Pair<float, float> getAvailablePosition(float yPos, float height) const;

	bool pushFloatingNode(LayoutBlock &origin, LayoutBlock &l, Vec2 &vec);
	bool pushFloatingNodeToStack(LayoutBlock &origin, LayoutBlock &l, Rect &s, const Rect &bbox,
			Vec2 &vec);
	bool pushFloatingNodeToNewStack(LayoutBlock &origin, LayoutBlock &l, Vector<Rect> &s,
			const Rect &bbox, Vec2 &vec);
};

} // namespace stappler::document

#endif /* EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTFLOATCONTEXT_H_ */
