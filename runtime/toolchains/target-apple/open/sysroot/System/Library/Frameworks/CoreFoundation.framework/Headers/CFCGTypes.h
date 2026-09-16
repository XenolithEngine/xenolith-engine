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

#include <CoreFoundation/CFBase.h>
#include <stdint.h>

CF_EXTERN_C_BEGIN

#if defined(__LP64__) && __LP64__
# define CGFLOAT_TYPE double
# define CGFLOAT_IS_DOUBLE 1
# define CGFLOAT_MIN 2.2250738585072014e-308
# define CGFLOAT_MAX 1.7976931348623157e+308
#else
# define CGFLOAT_TYPE float
# define CGFLOAT_IS_DOUBLE 0
# define CGFLOAT_MIN 1.17549435e-38f
# define CGFLOAT_MAX 3.40282347e+38f
#endif

typedef CGFLOAT_TYPE CGFloat;
#define CGFLOAT_DEFINED 1

struct CGPoint { CGFloat x; CGFloat y; };
typedef struct CGPoint CGPoint;

struct CGSize { CGFloat width; CGFloat height; };
typedef struct CGSize CGSize;

struct CGVector { CGFloat dx; CGFloat dy; };
typedef struct CGVector CGVector;

struct CGRect { CGPoint origin; CGSize size; };
typedef struct CGRect CGRect;

typedef enum : uint32_t {
	CGRectMinXEdge = 0,
	CGRectMinYEdge = 1,
	CGRectMaxXEdge = 2,
	CGRectMaxYEdge = 3,
} CGRectEdge;

struct CGAffineTransform {
	CGFloat a, b, c, d, tx, ty;
};
typedef struct CGAffineTransform CGAffineTransform;

CF_EXTERN_C_END

#endif /* __CF_CG_TYPES__ */
