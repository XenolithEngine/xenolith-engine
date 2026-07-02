/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLArgument.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLARGUMENT_H_
#define __SPRT_OPEN_METAL_MTLARGUMENT_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTexture.h>   /* MTLTextureType (used by value) + MTLTexture protocol */

@protocol MTLBuffer;
@protocol MTLCommandEncoder;
@protocol MTLComputePipelineState;
@protocol MTLRenderPipelineState;

typedef NS_ENUM(NSUInteger, MTLDataType) {
    MTLDataTypeNone = 0,

    MTLDataTypeStruct = 1,
    MTLDataTypeArray  = 2,

    MTLDataTypeFloat  = 3,
    MTLDataTypeFloat2 = 4,
    MTLDataTypeFloat3 = 5,
    MTLDataTypeFloat4 = 6,

    MTLDataTypeFloat2x2 = 7,
    MTLDataTypeFloat2x3 = 8,
    MTLDataTypeFloat2x4 = 9,

    MTLDataTypeFloat3x2 = 10,
    MTLDataTypeFloat3x3 = 11,
    MTLDataTypeFloat3x4 = 12,

    MTLDataTypeFloat4x2 = 13,
    MTLDataTypeFloat4x3 = 14,
    MTLDataTypeFloat4x4 = 15,

    MTLDataTypeHalf  = 16,
    MTLDataTypeHalf2 = 17,
    MTLDataTypeHalf3 = 18,
    MTLDataTypeHalf4 = 19,

    MTLDataTypeHalf2x2 = 20,
    MTLDataTypeHalf2x3 = 21,
    MTLDataTypeHalf2x4 = 22,

    MTLDataTypeHalf3x2 = 23,
    MTLDataTypeHalf3x3 = 24,
    MTLDataTypeHalf3x4 = 25,

    MTLDataTypeHalf4x2 = 26,
    MTLDataTypeHalf4x3 = 27,
    MTLDataTypeHalf4x4 = 28,

    MTLDataTypeInt  = 29,
    MTLDataTypeInt2 = 30,
    MTLDataTypeInt3 = 31,
    MTLDataTypeInt4 = 32,

    MTLDataTypeUInt  = 33,
    MTLDataTypeUInt2 = 34,
    MTLDataTypeUInt3 = 35,
    MTLDataTypeUInt4 = 36,

    MTLDataTypeShort  = 37,
    MTLDataTypeShort2 = 38,
    MTLDataTypeShort3 = 39,
    MTLDataTypeShort4 = 40,

    MTLDataTypeUShort = 41,
    MTLDataTypeUShort2 = 42,
    MTLDataTypeUShort3 = 43,
    MTLDataTypeUShort4 = 44,

    MTLDataTypeChar  = 45,
    MTLDataTypeChar2 = 46,
    MTLDataTypeChar3 = 47,
    MTLDataTypeChar4 = 48,

    MTLDataTypeUChar  = 49,
    MTLDataTypeUChar2 = 50,
    MTLDataTypeUChar3 = 51,
    MTLDataTypeUChar4 = 52,

    MTLDataTypeBool  = 53,
    MTLDataTypeBool2 = 54,
    MTLDataTypeBool3 = 55,
    MTLDataTypeBool4 = 56,

    MTLDataTypeTexture = 58,
    MTLDataTypeSampler = 59,
    MTLDataTypePointer = 60,

    MTLDataTypeR8Unorm         = 62,
    MTLDataTypeR8Snorm         = 63,
    MTLDataTypeR16Unorm        = 64,
    MTLDataTypeR16Snorm        = 65,
    MTLDataTypeRG8Unorm        = 66,
    MTLDataTypeRG8Snorm        = 67,
    MTLDataTypeRG16Unorm       = 68,
    MTLDataTypeRG16Snorm       = 69,
    MTLDataTypeRGBA8Unorm      = 70,
    MTLDataTypeRGBA8Unorm_sRGB = 71,
    MTLDataTypeRGBA8Snorm      = 72,
    MTLDataTypeRGBA16Unorm     = 73,
    MTLDataTypeRGBA16Snorm     = 74,
    MTLDataTypeRGB10A2Unorm    = 75,
    MTLDataTypeRG11B10Float    = 76,
    MTLDataTypeRGB9E5Float     = 77,
    MTLDataTypeRenderPipeline  = 78,
    MTLDataTypeComputePipeline = 79,
    MTLDataTypeIndirectCommandBuffer   = 80,
    MTLDataTypeLong  = 81,
    MTLDataTypeLong2 = 82,
    MTLDataTypeLong3 = 83,
    MTLDataTypeLong4 = 84,

    MTLDataTypeULong  = 85,
    MTLDataTypeULong2 = 86,
    MTLDataTypeULong3 = 87,
    MTLDataTypeULong4 = 88,
    MTLDataTypeVisibleFunctionTable = 115,
    MTLDataTypeIntersectionFunctionTable = 116,
    MTLDataTypePrimitiveAccelerationStructure = 117,
    MTLDataTypeInstanceAccelerationStructure = 118,
    MTLDataTypeBFloat  = 121,
    MTLDataTypeBFloat2 = 122,
    MTLDataTypeBFloat3 = 123,
    MTLDataTypeBFloat4 = 124,
};

@class MTLArgument;

typedef NS_ENUM(NSInteger, MTLBindingType) {
    MTLBindingTypeBuffer = 0,
    MTLBindingTypeThreadgroupMemory = 1,
    MTLBindingTypeTexture = 2,
    MTLBindingTypeSampler = 3,
    MTLBindingTypeImageblockData = 16,
    MTLBindingTypeImageblock = 17,
    MTLBindingTypeVisibleFunctionTable = 24,
    MTLBindingTypePrimitiveAccelerationStructure = 25,
    MTLBindingTypeInstanceAccelerationStructure = 26,
    MTLBindingTypeIntersectionFunctionTable = 27,
    MTLBindingTypeObjectPayload = 34,
};

typedef NS_ENUM(NSUInteger, MTLArgumentType) {
    MTLArgumentTypeBuffer = 0,
    MTLArgumentTypeThreadgroupMemory = 1,
    MTLArgumentTypeTexture = 2,
    MTLArgumentTypeSampler = 3,
    MTLArgumentTypeImageblockData = 16,
    MTLArgumentTypeImageblock = 17,
    MTLArgumentTypeVisibleFunctionTable = 24,
    MTLArgumentTypePrimitiveAccelerationStructure = 25,
    MTLArgumentTypeInstanceAccelerationStructure = 26,
    MTLArgumentTypeIntersectionFunctionTable = 27,
};

typedef NS_ENUM(NSUInteger, MTLBindingAccess) {
    MTLBindingAccessReadOnly   = 0,
    MTLBindingAccessReadWrite  = 1,
    MTLBindingAccessWriteOnly  = 2,
    MTLArgumentAccessReadOnly  = MTLBindingAccessReadOnly,
    MTLArgumentAccessReadWrite = MTLBindingAccessReadWrite,
    MTLArgumentAccessWriteOnly = MTLBindingAccessWriteOnly,
};

typedef MTLBindingAccess MTLArgumentAccess;

@class MTLStructType;
@class MTLArrayType;
@class MTLTextureReferenceType;
@class MTLPointerType;

@interface MTLType : NSObject
@property (readonly) MTLDataType dataType;
@end

@interface MTLStructMember : NSObject
@property (readonly) NSString *name;
@property (readonly) NSUInteger offset;
@property (readonly) MTLDataType dataType;
- (MTLStructType *)structType;
- (MTLArrayType *)arrayType;
- (MTLTextureReferenceType *)textureReferenceType;
- (MTLPointerType *)pointerType;
@property (readonly) NSUInteger argumentIndex;
@end

@interface MTLStructType : MTLType
@property (readonly) NSArray<MTLStructMember *> *members;
- (MTLStructMember *)memberByName:(NSString *)name;
@end

@interface MTLArrayType : MTLType
@property (readonly) MTLDataType elementType;
@property (readonly) NSUInteger arrayLength;
@property (readonly) NSUInteger stride;
@property (readonly) NSUInteger argumentIndexStride;
- (MTLStructType *)elementStructType;
- (MTLArrayType *)elementArrayType;
- (MTLTextureReferenceType *)elementTextureReferenceType;
- (MTLPointerType *)elementPointerType;
@end

@interface MTLPointerType : MTLType
@property (readonly) MTLDataType elementType;
@property (readonly) MTLBindingAccess access;
@property (readonly) NSUInteger alignment;
@property (readonly) NSUInteger dataSize;
@property (readonly) BOOL elementIsArgumentBuffer;
- (MTLStructType *)elementStructType;
- (MTLArrayType *)elementArrayType;
@end

@interface MTLTextureReferenceType : MTLType
@property (readonly) MTLDataType textureDataType;
@property (readonly) MTLTextureType textureType;
@property (readonly) MTLBindingAccess access;
@property (readonly) BOOL isDepthTexture;
@end

@interface MTLArgument : NSObject
@property (readonly) NSString *name;
@property (readonly) MTLArgumentType type;
@property (readonly) MTLBindingAccess access;
@property (readonly) NSUInteger index;
@property (readonly, getter=isActive) BOOL active;

@property (readonly) NSUInteger      bufferAlignment;
@property (readonly) NSUInteger      bufferDataSize;
@property (readonly) MTLDataType     bufferDataType;
@property (readonly) MTLStructType  *bufferStructType;
@property (readonly) MTLPointerType *bufferPointerType;

@property (readonly) NSUInteger     threadgroupMemoryAlignment;
@property (readonly) NSUInteger     threadgroupMemoryDataSize;

@property (readonly) MTLTextureType textureType;
@property (readonly) MTLDataType    textureDataType;
@property (readonly) BOOL           isDepthTexture;
@property (readonly) NSUInteger     arrayLength;
@end

@protocol MTLBinding <NSObject>
@property (readonly) NSString *name;
@property (readonly) MTLBindingType type;
@property (readonly) MTLBindingAccess access;
@property (readonly) NSUInteger index;
@property (readonly, getter=isUsed) BOOL used;
@property (readonly, getter=isArgument) BOOL argument;
@end

@protocol MTLBufferBinding <MTLBinding>
@property (readonly) NSUInteger      bufferAlignment;
@property (readonly) NSUInteger      bufferDataSize;
@property (readonly) MTLDataType     bufferDataType;
@property (readonly) MTLStructType  *bufferStructType;
@property (readonly) MTLPointerType *bufferPointerType;
@end

@protocol MTLThreadgroupBinding <MTLBinding>
@property (readonly) NSUInteger     threadgroupMemoryAlignment;
@property (readonly) NSUInteger     threadgroupMemoryDataSize;
@end

@protocol MTLTextureBinding <MTLBinding>
@property (readonly) MTLTextureType textureType;
@property (readonly) MTLDataType    textureDataType;
@property (readonly, getter=isDepthTexture) BOOL depthTexture;
@property (readonly) NSUInteger     arrayLength;
@end

@protocol MTLObjectPayloadBinding <MTLBinding>
@property (readonly) NSUInteger      objectPayloadAlignment;
@property (readonly) NSUInteger      objectPayloadDataSize;
@end

#endif /* __SPRT_OPEN_METAL_MTLARGUMENT_H_ */
