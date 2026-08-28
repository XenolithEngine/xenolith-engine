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

#ifndef XENOLITH_BACKEND_GLES_XLGLESMATERIAL_H_
#define XENOLITH_BACKEND_GLES_XLGLESMATERIAL_H_

#include "XLGlesDevice.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

// The flat draw path reads a material's image, sampler and pipeline straight out of the compiled
// set at record time (there is no per-material GPU buffer to fill), so this adds nothing beyond
// what core::MaterialAttachment already does - like the software one, it exists to give the
// attachment the backend's own type. The typed wrapper is load-bearing: plain Attachment's
// makeFrameHandle answers null and a frame with such an attachment dies in FrameQueue::setup.
class SP_PUBLIC MaterialAttachment
		: public core::AttachmentTyped<core::AttachmentHandle, core::MaterialAttachment> {
public:
	virtual ~MaterialAttachment() = default;
};

} // namespace stappler::xenolith::gles

#endif /* XENOLITH_BACKEND_GLES_XLGLESMATERIAL_H_ */
