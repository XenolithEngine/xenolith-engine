/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <CoreGraphics/CGGeometry.h> for the +open macOS target. The value
types live in <CoreFoundation/CFCGTypes.h>; this adds the constructors/zero values
the window backend uses (CGSizeMake, CGPointMake, CGRectMake, CGPointZero, ...).
**/

#ifndef CGGEOMETRY_H_
#define CGGEOMETRY_H_

#include <CoreGraphics/CGBase.h>

CG_EXTERN_C_BEGIN

CG_EXTERN const CGPoint CGPointZero;
CG_EXTERN const CGSize  CGSizeZero;
CG_EXTERN const CGRect  CGRectZero;

CG_INLINE CGPoint CGPointMake(CGFloat x, CGFloat y) { CGPoint p; p.x = x; p.y = y; return p; }
CG_INLINE CGSize  CGSizeMake(CGFloat w, CGFloat h) { CGSize s; s.width = w; s.height = h; return s; }
CG_INLINE CGRect  CGRectMake(CGFloat x, CGFloat y, CGFloat w, CGFloat h) {
	CGRect r; r.origin.x = x; r.origin.y = y; r.size.width = w; r.size.height = h; return r;
}

CG_EXTERN_C_END

#endif /* CGGEOMETRY_H_ */
