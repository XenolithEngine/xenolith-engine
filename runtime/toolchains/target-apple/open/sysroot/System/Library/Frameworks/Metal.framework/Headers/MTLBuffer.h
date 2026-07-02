/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLBuffer.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_BUFFER_H_
#define __SPRT_OPEN_METAL_BUFFER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLResource.h>

@class MTLTextureDescriptor;
@protocol MTLTexture;

NS_ASSUME_NONNULL_BEGIN

/* A typeless allocation accessible by both the CPU and the GPU (MTLDevice), or by
 * only the GPU when the storage mode is MTLResourceStorageModePrivate. */
@protocol MTLBuffer <MTLResource>

/* The length of the buffer in bytes. */
@property (readonly) NSUInteger length;

/* Returns the data pointer of this buffer's shared copy. */
- (void *)contents NS_RETURNS_INNER_POINTER;

/* Inform the device of the range of a buffer that the CPU has modified (Managed storage only). */
- (void)didModifyRange:(NSRange)range;

/* Create a 2D texture or texture buffer that shares storage with this buffer. */
- (nullable id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor offset:(NSUInteger)offset bytesPerRow:(NSUInteger)bytesPerRow;

/* The GPU virtual address of a buffer resource. */
@property (readonly) uint64_t gpuAddress;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_BUFFER_H_ */
