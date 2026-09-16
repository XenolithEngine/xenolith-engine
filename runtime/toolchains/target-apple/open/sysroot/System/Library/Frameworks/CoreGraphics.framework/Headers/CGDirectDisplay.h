/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <CoreGraphics/CGDirectDisplay.h> for the +open macOS target: the
direct-display identifiers, display-mode handle + accessors, and per-display info
the window backend's display-config manager uses. Symbols resolve from the baked
CoreGraphics.tbd. Signatures mirror the documented CoreGraphics ABI.
**/

#ifndef CGDIRECTDISPLAY_H_
#define CGDIRECTDISPLAY_H_

#include <CoreGraphics/CGBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>

CG_EXTERN_C_BEGIN

typedef uint32_t CGDirectDisplayID;
typedef uint32_t CGOpenGLDisplayMask;

typedef struct CGDisplayMode *CGDisplayModeRef;

/* CGError (subset; only kCGErrorSuccess is referenced). */
typedef int32_t CGError;
enum { kCGErrorSuccess = 0 };

#define kCGNullDirectDisplay ((CGDirectDisplayID)0)
#define kCGDirectMainDisplay CGMainDisplayID()

CG_EXTERN CGDirectDisplayID CGMainDisplayID(void);

CG_EXTERN CGError CGGetOnlineDisplayList(uint32_t maxDisplays,
		CGDirectDisplayID * __nullable onlineDisplays, uint32_t * __nullable displayCount);
CG_EXTERN CGError CGGetActiveDisplayList(uint32_t maxDisplays,
		CGDirectDisplayID * __nullable activeDisplays, uint32_t * __nullable displayCount);

CG_EXTERN int      CGDisplayIsActive(CGDirectDisplayID display);
CG_EXTERN int      CGDisplayIsMain(CGDirectDisplayID display);
CG_EXTERN uint32_t CGDisplayUnitNumber(CGDirectDisplayID display);
CG_EXTERN uint32_t CGDisplayVendorNumber(CGDirectDisplayID display);
CG_EXTERN uint32_t CGDisplayModelNumber(CGDirectDisplayID display);
CG_EXTERN uint32_t CGDisplaySerialNumber(CGDirectDisplayID display);
CG_EXTERN CGDirectDisplayID CGDisplayPrimaryDisplay(CGDirectDisplayID display);
CG_EXTERN CGSize   CGDisplayScreenSize(CGDirectDisplayID display);
CG_EXTERN CGRect   CGDisplayBounds(CGDirectDisplayID display);
CG_EXTERN size_t   CGDisplayPixelsWide(CGDirectDisplayID display);
CG_EXTERN size_t   CGDisplayPixelsHigh(CGDirectDisplayID display);

/* Display modes. */
CG_EXTERN CGDisplayModeRef __nullable CGDisplayCopyDisplayMode(CGDirectDisplayID display);
CG_EXTERN CFArrayRef __nullable CGDisplayCopyAllDisplayModes(CGDirectDisplayID display,
		CFDictionaryRef __nullable options);
CG_EXTERN CFStringRef kCGDisplayShowDuplicateLowResolutionModes;

CG_EXTERN size_t CGDisplayModeGetWidth(CGDisplayModeRef __nullable mode);
CG_EXTERN size_t CGDisplayModeGetHeight(CGDisplayModeRef __nullable mode);
CG_EXTERN size_t CGDisplayModeGetPixelWidth(CGDisplayModeRef __nullable mode);
CG_EXTERN size_t CGDisplayModeGetPixelHeight(CGDisplayModeRef __nullable mode);
CG_EXTERN double CGDisplayModeGetRefreshRate(CGDisplayModeRef __nullable mode);
CG_EXTERN uint32_t CGDisplayModeGetIOFlags(CGDisplayModeRef __nullable mode);
CG_EXTERN int32_t CGDisplayModeGetIODisplayModeID(CGDisplayModeRef __nullable mode);
CG_EXTERN void CGDisplayModeRelease(CGDisplayModeRef __nullable mode);
CG_EXTERN CGDisplayModeRef __nullable CGDisplayModeRetain(CGDisplayModeRef __nullable mode);

CG_EXTERN_C_END

#endif /* CGDIRECTDISPLAY_H_ */
