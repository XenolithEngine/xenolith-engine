/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLVertexDescriptor.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLVERTEXDESCRIPTOR_H_
#define __SPRT_OPEN_METAL_MTLVERTEXDESCRIPTOR_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLVertexDescriptor.h>

NS_ASSUME_NONNULL_BEGIN

static const NSUInteger MTLBufferLayoutStrideDynamic = NSUIntegerMax;

@interface MTLVertexBufferLayoutDescriptor : NSObject <NSCopying>
@property (assign, nonatomic) NSUInteger stride;
@property (assign, nonatomic) MTLVertexStepFunction stepFunction;
@property (assign, nonatomic) NSUInteger stepRate;
@end

@interface MTLVertexBufferLayoutDescriptorArray : NSObject
- (MTLVertexBufferLayoutDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLVertexBufferLayoutDescriptor *)bufferDesc atIndexedSubscript:(NSUInteger)index;
@end

@interface MTLVertexAttributeDescriptor : NSObject <NSCopying>
@property (assign, nonatomic) MTLVertexFormat format;
@property (assign, nonatomic) NSUInteger offset;
@property (assign, nonatomic) NSUInteger bufferIndex;
@end

@interface MTLVertexAttributeDescriptorArray : NSObject
- (MTLVertexAttributeDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLVertexAttributeDescriptor *)attributeDesc atIndexedSubscript:(NSUInteger)index;
@end

@interface MTLVertexDescriptor : NSObject <NSCopying>

+ (MTLVertexDescriptor *)vertexDescriptor;

@property (readonly) MTLVertexBufferLayoutDescriptorArray *layouts;
@property (readonly) MTLVertexAttributeDescriptorArray *attributes;

- (void)reset;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLVERTEXDESCRIPTOR_H_ */
