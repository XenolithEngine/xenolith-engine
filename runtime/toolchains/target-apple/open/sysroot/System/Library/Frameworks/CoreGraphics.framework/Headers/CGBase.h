/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Hand-written <CoreGraphics/CGBase.h> for the Xcode-SDK-free macOS target
(*-apple-macosx+open). CoreGraphics is a closed framework; this reconstructs only
the base macros + CGFloat that the window backend needs. See open-sysroot.mk.
**/

#ifndef CGBASE_H_
#define CGBASE_H_

#include <sys/cdefs.h>
#include <stdint.h>
#include <stddef.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFCGTypes.h>

#ifdef __cplusplus
# define CG_EXTERN extern "C" __attribute__((visibility("default")))
#else
# define CG_EXTERN extern __attribute__((visibility("default")))
#endif

#define CG_EXTERN_C_BEGIN __BEGIN_DECLS
#define CG_EXTERN_C_END   __END_DECLS
#define CG_INLINE static __inline__

#endif /* CGBASE_H_ */
