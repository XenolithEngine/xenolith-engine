/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLDynamicLibrary.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLDYNAMICLIBRARY_H_
#define __SPRT_OPEN_METAL_MTLDYNAMICLIBRARY_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLDynamicLibrary.h>

@protocol MTLDevice;

@protocol MTLDynamicLibrary <NSObject>

@property (copy, atomic) NSString *label;
@property (readonly) id<MTLDevice> device;
@property (readonly) NSString *installName;

- (BOOL)serializeToURL:(NSURL *)url error:(NSError **)error;

@end

#endif /* __SPRT_OPEN_METAL_MTLDYNAMICLIBRARY_H_ */
