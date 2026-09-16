/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <CoreGraphics/CGColorSpace.h> for *-apple-macosx+open — only the surface
 * MoltenVK uses: the opaque CGColorSpaceRef, the named-colorspace CFString constants it maps
 * a swapchain's colorspace to, and create/release. Symbols resolve from CoreGraphics.tbd. */
#ifndef __SPRT_OPEN_CGCOLORSPACE_H_
#define __SPRT_OPEN_CGCOLORSPACE_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGBase.h>

CF_EXTERN_C_BEGIN

typedef struct CGColorSpace *CGColorSpaceRef;

CG_EXTERN const CFStringRef kCGColorSpaceSRGB;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedSRGB;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedLinearSRGB;
CG_EXTERN const CFStringRef kCGColorSpaceDisplayP3;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedLinearDisplayP3;
CG_EXTERN const CFStringRef kCGColorSpaceDCIP3;
CG_EXTERN const CFStringRef kCGColorSpaceAdobeRGB1998;
CG_EXTERN const CFStringRef kCGColorSpaceITUR_709;
CG_EXTERN const CFStringRef kCGColorSpaceITUR_2100_PQ;
CG_EXTERN const CFStringRef kCGColorSpaceITUR_2100_HLG;
CG_EXTERN const CFStringRef kCGColorSpaceExtendedLinearITUR_2020;

CG_EXTERN CGColorSpaceRef CGColorSpaceCreateWithName(CFStringRef name);
CG_EXTERN CGColorSpaceRef CGColorSpaceRetain(CGColorSpaceRef space);
CG_EXTERN CFStringRef CGColorSpaceGetName(CGColorSpaceRef space);
CG_EXTERN void CGColorSpaceRelease(CGColorSpaceRef space);

CF_EXTERN_C_END

#endif /* __SPRT_OPEN_CGCOLORSPACE_H_ */
