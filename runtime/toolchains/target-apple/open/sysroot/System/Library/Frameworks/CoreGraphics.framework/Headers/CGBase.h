/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <CoreGraphics/CGBase.h> for the Xcode-SDK-free macOS target
(*-apple-macosx+open). CoreGraphics is a closed framework; this reconstructs only
the base macros + CGFloat that the window backend needs. See open-sysroot.mk.
**/

#ifndef CGBASE_H_
#define CGBASE_H_
#include <__sprt_abi/CoreGraphics/CGBase.h>

#include <sys/cdefs.h>
#include <stdint.h>
#include <stddef.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFCGTypes.h>

#endif /* CGBASE_H_ */
