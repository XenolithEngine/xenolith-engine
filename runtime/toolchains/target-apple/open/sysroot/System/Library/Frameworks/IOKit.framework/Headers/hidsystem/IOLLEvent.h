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

/* device-dependent modifier-key masks (event.flags low bits) */
#define NX_DEVICELCTLKEYMASK    0x00000001
#define NX_DEVICELSHIFTKEYMASK  0x00000002
#define NX_DEVICERSHIFTKEYMASK  0x00000004
#define NX_DEVICELCMDKEYMASK    0x00000008
#define NX_DEVICERCMDKEYMASK    0x00000010
#define NX_DEVICELALTKEYMASK    0x00000020
#define NX_DEVICERALTKEYMASK    0x00000040
#define NX_DEVICE_ALPHASHIFT_STATELESS_MASK 0x00000080
#define NX_DEVICERCTLKEYMASK    0x00002000

#endif /* _IOKIT_HIDSYSTEM_IOLLEVENT_H_ */
