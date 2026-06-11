/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCoreRenderSession.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

// Out-of-line virtual destructors: each is the vtable key function (anchoring the vtable and
// typeinfo in this single TU). They are defaulted, so the deleting destructor variant calls
// operator delete -- safe in this freestanding build with exceptions disabled, so the warning is
// suppressed here.
__SPRT_PUSH_ALLOW_CXXABI_ALLOC

RenderClientChannel::~RenderClientChannel() = default;

RenderServerChannel::~RenderServerChannel() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

void RenderServerChannel::setRenderClient(core::RenderClientChannel *c) {
	_clientRef = nullptr;
	_client = c;

	if (auto ref = dynamic_cast<Ref *>(c)) {
		_clientRef = ref;
	}
}

} // namespace stappler::xenolith::core
