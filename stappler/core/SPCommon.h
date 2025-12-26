/**
 Copyright (c) 2016-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef STAPPLER_CORE_SPCOMMON_H_
#define STAPPLER_CORE_SPCOMMON_H_

// Basic precompiled header of SDK
// Should be included as a first header in translation unit (first in SCU, not single file)
// Use it instead of SPCore.h to enable precompiled headers

#include "SPCore.h" // IWYU pragma: keep
#include "SPMemInterface.h"
#include "SPMemUuid.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler {

SP_PUBLIC void getBacktrace(size_t offset, const Callback<void(StringView)> &);

}

#endif /* STAPPLER_CORE_SPCOMMON_H_ */
