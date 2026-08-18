/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 *
 * Hand-written <IOSurface/IOSurfaceRef.h> for the Xcode-SDK-free macOS target
 * (*-apple-macosx+open). IOSurface is a closed framework; this reconstructs ONLY the
 * opaque IOSurfaceRef handle, the IOSurfaceID surface identifier, the CFDictionary
 * property keys, and the geometry getters that MoltenVK actually references (IOSurface
 * interop for VkImage). Signatures and key names match Apple's IOSurfaceRef.h ABI
 * exactly. No IOSurface symbols are hard-linked — MoltenVK resolves them via
 * -undefined dynamic_lookup; they exist here only for typing.
 */

#ifndef __SPRT_OPEN_IOSURFACEREF_H_
#define __SPRT_OPEN_IOSURFACEREF_H_

#include <CoreFoundation/CoreFoundation.h>
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

typedef struct __IOSurface *IOSurfaceRef;

/* Global surface identifier, as returned by IOSurfaceGetID(). uint32_t per Apple's ABI. */
typedef uint32_t IOSurfaceID;

/* CFDictionary property keys accepted by IOSurfaceCreate() (MoltenVK-used subset). */
extern const CFStringRef kIOSurfaceWidth;
extern const CFStringRef kIOSurfaceHeight;
extern const CFStringRef kIOSurfaceBytesPerElement;
extern const CFStringRef kIOSurfaceElementWidth;
extern const CFStringRef kIOSurfaceElementHeight;
extern const CFStringRef kIOSurfaceIsGlobal;
extern const CFStringRef kIOSurfacePlaneInfo;
extern const CFStringRef kIOSurfacePlaneWidth;
extern const CFStringRef kIOSurfacePlaneHeight;
extern const CFStringRef kIOSurfacePlaneBytesPerElement;
extern const CFStringRef kIOSurfacePlaneElementWidth;
extern const CFStringRef kIOSurfacePlaneElementHeight;

IOSurfaceRef IOSurfaceCreate(CFDictionaryRef properties);

/* MoltenVK >= 1.4.2 (MVKImage.mm, mvkGetMTLTextureIOSurfaceID). */
IOSurfaceID IOSurfaceGetID(IOSurfaceRef buffer);

size_t IOSurfaceGetWidth(IOSurfaceRef buffer);
size_t IOSurfaceGetHeight(IOSurfaceRef buffer);
size_t IOSurfaceGetBytesPerElement(IOSurfaceRef buffer);
size_t IOSurfaceGetElementWidth(IOSurfaceRef buffer);
size_t IOSurfaceGetElementHeight(IOSurfaceRef buffer);
size_t IOSurfaceGetPlaneCount(IOSurfaceRef buffer);

size_t IOSurfaceGetWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex);
size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex);
size_t IOSurfaceGetBytesPerElementOfPlane(IOSurfaceRef buffer, size_t planeIndex);
size_t IOSurfaceGetElementWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex);
size_t IOSurfaceGetElementHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex);

__END_DECLS

#endif /* __SPRT_OPEN_IOSURFACEREF_H_ */
