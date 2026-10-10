/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLStageInputOutputDescriptor.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLSTAGEINPUTOUTPUTDESCRIPTOR_H_
#define __SPRT_OPEN_METAL_MTLSTAGEINPUTOUTPUTDESCRIPTOR_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLVertexDescriptor.h>
#include <__sprt_abi/Metal/MTLStageInputOutputDescriptor.h>

NS_ASSUME_NONNULL_BEGIN

@interface MTLBufferLayoutDescriptor : NSObject <NSCopying>
@property (assign, nonatomic) NSUInteger stride;
@property (assign, nonatomic) MTLStepFunction stepFunction;
@property (assign, nonatomic) NSUInteger stepRate;
@end

@interface MTLBufferLayoutDescriptorArray : NSObject
- (MTLBufferLayoutDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLBufferLayoutDescriptor *)bufferDesc atIndexedSubscript:(NSUInteger)index;
@end

@interface MTLAttributeDescriptor : NSObject <NSCopying>
@property (assign, nonatomic) MTLAttributeFormat format;
@property (assign, nonatomic) NSUInteger offset;
@property (assign, nonatomic) NSUInteger bufferIndex;
@end

@interface MTLAttributeDescriptorArray : NSObject
- (MTLAttributeDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLAttributeDescriptor *)attributeDesc atIndexedSubscript:(NSUInteger)index;
@end

@interface MTLStageInputOutputDescriptor : NSObject <NSCopying>

+ (MTLStageInputOutputDescriptor *)stageInputOutputDescriptor;

@property (readonly) MTLBufferLayoutDescriptorArray *layouts;
@property (readonly) MTLAttributeDescriptorArray *attributes;

@property (assign, nonatomic) MTLIndexType indexType;
@property (assign, nonatomic) NSUInteger indexBufferIndex;

- (void)reset;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLSTAGEINPUTOUTPUTDESCRIPTOR_H_ */
