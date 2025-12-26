/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "MaterialEasing.h"
#include "XLActionEase.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

Rc<ActionEaseCommon> makeEasing(Rc<ActionInterval> &&action, EasingType type) {
	switch (type) {
	case EasingType::Standard:
		return Rc<EaseActionTyped>::create(move(action),
				EaseBezierInfo{{0.2f, 0.0f}, {0.0f, 1.0f}});
		break;
	case EasingType::StandardAccelerate:
		return Rc<EaseActionTyped>::create(move(action),
				EaseBezierInfo{{0.3f, 0.0f}, {1.0f, 1.0f}});
		break;
	case EasingType::StandardDecelerate:
		return Rc<EaseActionTyped>::create(move(action),
				EaseBezierInfo{{0.0f, 0.0f}, {0.0f, 1.0f}});
		break;
	case EasingType::Emphasized:
		return Rc<EaseActionTyped>::create(move(action),
				EaseBezierInfo{{0.2f, 0.0f}, {0.0f, 1.0f}});
		break;
	case EasingType::EmphasizedAccelerate:
		return Rc<EaseActionTyped>::create(move(action),
				EaseBezierInfo{{0.3f, 0.0f}, {0.8f, 0.15f}});
		break;
	case EasingType::EmphasizedDecelerate:
		return Rc<EaseActionTyped>::create(move(action),
				EaseBezierInfo{{0.05f, 0.7f}, {0.1f, 1.0f}});
		break;
	}
	return nullptr;
}

} // namespace stappler::xenolith::material2d
