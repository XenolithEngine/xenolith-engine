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

/*
	Hand-written <CoreFoundation/CFCGTypes.h> for the Xcode-SDK-free macOS target
	(*-apple-macosx+open). The swift-corelibs CoreFoundation we use ships no
	CFCGTypes.h; this reconstructs the canonical CoreGraphics geometry value types
	(the SDK defines them here, and CoreGraphics/Foundation alias to them). ABI-exact
	on 64-bit: CGFloat is double, the structs are POD. No symbols — pure types.
*/

#ifndef __CF_CG_TYPES__
#define __CF_CG_TYPES__
#include <__sprt_abi/CoreFoundation/CFCGTypes.h>

#include <CoreFoundation/CFBase.h>
#include <stdint.h>

CF_EXTERN_C_BEGIN

CF_EXTERN_C_END

#endif /* __CF_CG_TYPES__ */
