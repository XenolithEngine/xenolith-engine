/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Minimal hand-written <IOKit/hidsystem/IOLLEvent.h> for the +open macOS target.
The full header lives in the IOHIDFamily project (not among the apple-oss repos we
clone); the window backend only needs the device-dependent modifier-key masks
(NX_DEVICE*KEYMASK) to distinguish left/right modifier keys from an NSEvent's
modifierFlags. Values are the stable macOS ABI constants.
**/

#ifndef _IOKIT_HIDSYSTEM_IOLLEVENT_H_
#define _IOKIT_HIDSYSTEM_IOLLEVENT_H_
#include <__sprt_abi/IOKit/hidsystem/IOLLEvent.h>

#endif /* _IOKIT_HIDSYSTEM_IOLLEVENT_H_ */
