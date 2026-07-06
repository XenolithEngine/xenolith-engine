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

#ifndef XENOLITH_RENDERER_PUG_XLPUGCONFIG_H_
#define XENOLITH_RENDERER_PUG_XLPUGCONFIG_H_

#include "XLSimpleUiConfig.h" // IWYU pragma: keep
#include "XLSimpleLayoutSystem.h" // IWYU pragma: keep
#include "XLSimpleButton.h" // IWYU pragma: keep
#include "XLSimpleStyle.h" // IWYU pragma: keep
#include "SPPugCache.h" // IWYU pragma: keep
#include "SPPugContext.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::pugui {

// stappler::pug works with pool-backed types, the alias keeps them clearly marked
namespace spug = STAPPLER_VERSIONIZED_NAMESPACE::pug;

using namespace simpleui; // pulls basic2d in as well

} // namespace stappler::xenolith::pugui

#endif /* XENOLITH_RENDERER_PUG_XLPUGCONFIG_H_ */
