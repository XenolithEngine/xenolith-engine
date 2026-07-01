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

#ifndef UTILS_XLMAKE_EXECUTOR_H_
#define UTILS_XLMAKE_EXECUTOR_H_

// The build executor now lives in the engine (stappler_makefile, SPMakefileBuilder.{h,cc}); xlmake is
// a thin CLI over it. Re-export the entry point into the xlmake namespace so main.cpp is unchanged.

#include "SPMakefileBuilder.h"

namespace xlmake {

using sp::makefile::BuildConfig;
using sp::makefile::JobsUnlimited;
using sp::makefile::runBuild;

} // namespace xlmake

#endif /* UTILS_XLMAKE_EXECUTOR_H_ */
