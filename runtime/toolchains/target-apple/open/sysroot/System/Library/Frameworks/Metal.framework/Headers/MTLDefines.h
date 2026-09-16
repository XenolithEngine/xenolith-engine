/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLDefines.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLDEFINES_H_
#define __SPRT_OPEN_METAL_MTLDEFINES_H_

#import <Foundation/Foundation.h>

/* Apple's MTLDefines.h holds visibility/export attribute macros. The +open
 * reconstruction omits the actual attributes (nothing is exported: there is no
 * Metal.tbd), but still provides the macro NAMES as portable no-ops so any
 * sub-header that references them keeps compiling. */

#ifndef MTL_EXPORT
#define MTL_EXPORT
#endif

#ifndef MTL_EXTERN
#ifdef __cplusplus
#define MTL_EXTERN extern "C"
#else
#define MTL_EXTERN extern
#endif
#endif

#ifndef MTL_INLINE
#define MTL_INLINE static inline
#endif

#endif /* __SPRT_OPEN_METAL_MTLDEFINES_H_ */
