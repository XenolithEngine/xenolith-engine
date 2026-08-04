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

#include "SPRaster.h"

// The whole rasterizer is one translation unit on purpose. It is what lets the kernels stay
// `static` while still seeing each other, and - once the ISA-specific kernels land - what lets
// them be selected per function with an attribute instead of per file with a compiler flag.

// Order matters: the triangle setup calls into the kernels, and the kernels call into the sampler.
#include "SPRasterSample.cc"
#include "SPRasterKernelsScalar.cc"
#include "SPRasterKernelsSwar.cc"
#include "SPRasterCpu.cc"
#include "SPRasterKernelsX86.cc"
#include "SPRasterKernelsNeon.cc"
#include "SPRasterDispatch.cc"
#include "SPRasterSetup.cc"
#include "SPRasterTile.cc"
