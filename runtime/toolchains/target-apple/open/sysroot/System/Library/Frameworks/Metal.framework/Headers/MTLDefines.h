/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLDefines.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLDEFINES_H_
#define __SPRT_OPEN_METAL_MTLDEFINES_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLDefines.h>

/* Apple's MTLDefines.h holds visibility/export attribute macros. The +open
 * reconstruction omits the actual attributes (nothing is exported: there is no
 * Metal.tbd), but still provides the macro NAMES as portable no-ops so any
 * sub-header that references them keeps compiling. */

#endif /* __SPRT_OPEN_METAL_MTLDEFINES_H_ */
