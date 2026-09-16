/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <CoreGraphics/CGDisplayConfiguration.h> for the +open macOS target:
the display-reconfiguration callback + the begin/complete/configure transaction
API the window backend uses to switch modes. Includes CGDirectDisplay.h (types +
mode accessors), mirroring the real framework layout.
**/

#ifndef CGDISPLAYCONFIGURATION_H_
#define CGDISPLAYCONFIGURATION_H_

#include <CoreGraphics/CGDirectDisplay.h>
/* The real CGDisplayConfiguration.h pulls in IOKit (display IDs are IOKit registry entries);
   MoltenVK relies on this transitive include for io_iterator_t/IOServiceMatching/&c. in its
   GPU-registry probing (MVKDevice.mm includes no IOKit header of its own). */
#include <IOKit/IOKitLib.h>

CG_EXTERN_C_BEGIN

typedef struct _CGDisplayConfigRef *CGDisplayConfigRef;

typedef uint32_t CGDisplayChangeSummaryFlags;
enum {
	kCGDisplayBeginConfigurationFlag = (1 << 0),
	kCGDisplayMovedFlag              = (1 << 1),
	kCGDisplaySetMainFlag            = (1 << 2),
	kCGDisplaySetModeFlag            = (1 << 3),
	kCGDisplayAddFlag                = (1 << 4),
	kCGDisplayRemoveFlag             = (1 << 5),
	kCGDisplayEnabledFlag            = (1 << 8),
	kCGDisplayDisabledFlag           = (1 << 9),
	kCGDisplayMirrorFlag             = (1 << 10),
	kCGDisplayUnMirrorFlag           = (1 << 11),
	kCGDisplayDesktopShapeChangedFlag = (1 << 12),
};

typedef uint32_t CGConfigureOption;
enum {
	kCGConfigureForAppOnly       = 0,
	kCGConfigureForSession       = 1,
	kCGConfigurePermanently      = 2,
};

typedef void (*CGDisplayReconfigurationCallBack)(CGDirectDisplayID display,
		CGDisplayChangeSummaryFlags flags, void * __nullable userInfo);

CG_EXTERN CGError CGDisplayRegisterReconfigurationCallback(
		CGDisplayReconfigurationCallBack __nullable callback, void * __nullable userInfo);
CG_EXTERN CGError CGDisplayRemoveReconfigurationCallback(
		CGDisplayReconfigurationCallBack __nullable callback, void * __nullable userInfo);

CG_EXTERN CGError CGBeginDisplayConfiguration(CGDisplayConfigRef __nullable * __nullable pConfigRef);
CG_EXTERN CGError CGCompleteDisplayConfiguration(CGDisplayConfigRef __nullable configRef,
		CGConfigureOption option);
CG_EXTERN CGError CGCancelDisplayConfiguration(CGDisplayConfigRef __nullable configRef);
CG_EXTERN CGError CGConfigureDisplayWithDisplayMode(CGDisplayConfigRef __nullable configRef,
		CGDirectDisplayID display, CGDisplayModeRef __nullable mode,
		CFDictionaryRef __nullable options);
CG_EXTERN void CGRestorePermanentDisplayConfiguration(void);

CG_EXTERN_C_END

#endif /* CGDISPLAYCONFIGURATION_H_ */
