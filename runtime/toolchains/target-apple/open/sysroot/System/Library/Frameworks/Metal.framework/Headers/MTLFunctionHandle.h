/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLFunctionHandle.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLFUNCTIONHANDLE_H_
#define __SPRT_OPEN_METAL_MTLFUNCTIONHANDLE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLLibrary.h>   /* MTLFunctionType, @protocol MTLDevice */

@protocol MTLFunctionHandle <NSObject>
@property (readonly) MTLFunctionType functionType;
@property (readonly) NSString *name;
@property (readonly) id<MTLDevice> device;
@end

#endif /* __SPRT_OPEN_METAL_MTLFUNCTIONHANDLE_H_ */
