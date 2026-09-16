/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <CoreGraphics/CGColor.h> for *-apple-macosx+open — only the opaque
 * CGColorRef the window backend passes between NSColor.CGColor and CALayer.borderColor,
 * plus retain/release. Symbols resolve from CoreGraphics.tbd. */
#ifndef __SPRT_OPEN_CGCOLOR_H_
#define __SPRT_OPEN_CGCOLOR_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGBase.h>

CF_EXTERN_C_BEGIN

typedef struct CGColor *CGColorRef;

CG_EXTERN CGColorRef CGColorRetain(CGColorRef color);
CG_EXTERN void CGColorRelease(CGColorRef color);

CF_EXTERN_C_END

#endif /* __SPRT_OPEN_CGCOLOR_H_ */
