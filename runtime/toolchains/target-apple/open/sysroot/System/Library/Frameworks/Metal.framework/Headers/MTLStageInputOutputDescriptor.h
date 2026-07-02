/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLStageInputOutputDescriptor.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLSTAGEINPUTOUTPUTDESCRIPTOR_H_
#define __SPRT_OPEN_METAL_MTLSTAGEINPUTOUTPUTDESCRIPTOR_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLVertexDescriptor.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, MTLAttributeFormat)
{
    MTLAttributeFormatInvalid = 0,

    MTLAttributeFormatUChar2 = 1,
    MTLAttributeFormatUChar3 = 2,
    MTLAttributeFormatUChar4 = 3,

    MTLAttributeFormatChar2 = 4,
    MTLAttributeFormatChar3 = 5,
    MTLAttributeFormatChar4 = 6,

    MTLAttributeFormatUChar2Normalized = 7,
    MTLAttributeFormatUChar3Normalized = 8,
    MTLAttributeFormatUChar4Normalized = 9,

    MTLAttributeFormatChar2Normalized = 10,
    MTLAttributeFormatChar3Normalized = 11,
    MTLAttributeFormatChar4Normalized = 12,

    MTLAttributeFormatUShort2 = 13,
    MTLAttributeFormatUShort3 = 14,
    MTLAttributeFormatUShort4 = 15,

    MTLAttributeFormatShort2 = 16,
    MTLAttributeFormatShort3 = 17,
    MTLAttributeFormatShort4 = 18,

    MTLAttributeFormatUShort2Normalized = 19,
    MTLAttributeFormatUShort3Normalized = 20,
    MTLAttributeFormatUShort4Normalized = 21,

    MTLAttributeFormatShort2Normalized = 22,
    MTLAttributeFormatShort3Normalized = 23,
    MTLAttributeFormatShort4Normalized = 24,

    MTLAttributeFormatHalf2 = 25,
    MTLAttributeFormatHalf3 = 26,
    MTLAttributeFormatHalf4 = 27,

    MTLAttributeFormatFloat = 28,
    MTLAttributeFormatFloat2 = 29,
    MTLAttributeFormatFloat3 = 30,
    MTLAttributeFormatFloat4 = 31,

    MTLAttributeFormatInt = 32,
    MTLAttributeFormatInt2 = 33,
    MTLAttributeFormatInt3 = 34,
    MTLAttributeFormatInt4 = 35,

    MTLAttributeFormatUInt = 36,
    MTLAttributeFormatUInt2 = 37,
    MTLAttributeFormatUInt3 = 38,
    MTLAttributeFormatUInt4 = 39,

    MTLAttributeFormatInt1010102Normalized = 40,
    MTLAttributeFormatUInt1010102Normalized = 41,

    MTLAttributeFormatUChar4Normalized_BGRA = 42,

    MTLAttributeFormatUChar = 45,
    MTLAttributeFormatChar = 46,
    MTLAttributeFormatUCharNormalized = 47,
    MTLAttributeFormatCharNormalized = 48,

    MTLAttributeFormatUShort = 49,
    MTLAttributeFormatShort = 50,
    MTLAttributeFormatUShortNormalized = 51,
    MTLAttributeFormatShortNormalized = 52,

    MTLAttributeFormatHalf = 53,

    MTLAttributeFormatFloatRG11B10 = 54,
    MTLAttributeFormatFloatRGB9E5 = 55,
};

typedef NS_ENUM(NSUInteger, MTLIndexType) {
    MTLIndexTypeUInt16 = 0,
    MTLIndexTypeUInt32 = 1,
};

typedef NS_ENUM(NSUInteger, MTLStepFunction)
{
    MTLStepFunctionConstant = 0,

    MTLStepFunctionPerVertex = 1,
    MTLStepFunctionPerInstance = 2,
    MTLStepFunctionPerPatch = 3,
    MTLStepFunctionPerPatchControlPoint = 4,

    MTLStepFunctionThreadPositionInGridX = 5,
    MTLStepFunctionThreadPositionInGridY = 6,
    MTLStepFunctionThreadPositionInGridXIndexed = 7,
    MTLStepFunctionThreadPositionInGridYIndexed = 8,
};

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
