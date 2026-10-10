/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLResourceStateCommandEncoder.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLRESOURCESTATECOMMANDENCODER_H_
#define __SPRT_OPEN_METAL_MTLRESOURCESTATECOMMANDENCODER_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>
#include <__sprt_abi/Metal/MTLResourceStateCommandEncoder.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLCommandEncoder;
@protocol MTLTexture;
@protocol MTLBuffer;
@protocol MTLFence;

@protocol MTLResourceStateCommandEncoder <MTLCommandEncoder>

@optional
- (void)updateTextureMappings:(id<MTLTexture>)texture
                         mode:(const MTLSparseTextureMappingMode)mode
                      regions:(const MTLRegion[_Nonnull])regions
                    mipLevels:(const NSUInteger[_Nonnull])mipLevels
                       slices:(const NSUInteger[_Nonnull])slices
                   numRegions:(NSUInteger)numRegions;

- (void)updateTextureMapping:(id<MTLTexture>)texture
                        mode:(const MTLSparseTextureMappingMode)mode
                      region:(const MTLRegion)region
                    mipLevel:(const NSUInteger)mipLevel
                       slice:(const NSUInteger)slice;

- (void)updateTextureMapping:(id<MTLTexture>)texture
                        mode:(const MTLSparseTextureMappingMode)mode
              indirectBuffer:(id<MTLBuffer>)indirectBuffer
        indirectBufferOffset:(NSUInteger)indirectBufferOffset;

- (void)updateFence:(id<MTLFence>)fence;
- (void)waitForFence:(id<MTLFence>)fence;

- (void)moveTextureMappingsFromTexture:(id<MTLTexture>)sourceTexture
                          sourceSlice:(NSUInteger)sourceSlice
                          sourceLevel:(NSUInteger)sourceLevel
                         sourceOrigin:(MTLOrigin)sourceOrigin
                           sourceSize:(MTLSize)sourceSize
                            toTexture:(id<MTLTexture>)destinationTexture
                     destinationSlice:(NSUInteger)destinationSlice
                     destinationLevel:(NSUInteger)destinationLevel
                    destinationOrigin:(MTLOrigin)destinationOrigin;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLRESOURCESTATECOMMANDENCODER_H_ */
