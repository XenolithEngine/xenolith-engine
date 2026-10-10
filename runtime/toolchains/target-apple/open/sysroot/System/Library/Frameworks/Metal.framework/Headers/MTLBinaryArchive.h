/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLBinaryArchive.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLBINARYARCHIVE_H_
#define __SPRT_OPEN_METAL_MTLBINARYARCHIVE_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLBinaryArchive.h>

@protocol MTLDevice;
@protocol MTLLibrary;
@class MTLFunctionDescriptor;
@class MTLComputePipelineDescriptor;
@class MTLRenderPipelineDescriptor;

@interface MTLBinaryArchiveDescriptor : NSObject <NSCopying>
@property (readwrite, nonatomic, copy) NSURL *url;
@end

@protocol MTLBinaryArchive <NSObject>

@property (copy, atomic) NSString *label;
@property (readonly) id<MTLDevice> device;

- (BOOL)addComputePipelineFunctionsWithDescriptor:(MTLComputePipelineDescriptor *)descriptor error:(NSError **)error;
- (BOOL)addRenderPipelineFunctionsWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor error:(NSError **)error;
- (BOOL)serializeToURL:(NSURL *)url error:(NSError **)error;
- (BOOL)addFunctionWithDescriptor:(MTLFunctionDescriptor *)descriptor library:(id<MTLLibrary>)library error:(NSError **)error;

@end

#endif /* __SPRT_OPEN_METAL_MTLBINARYARCHIVE_H_ */
