/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLArgument.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLARGUMENT_H_
#define __SPRT_OPEN_METAL_MTLARGUMENT_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTexture.h>   /* MTLTextureType (used by value) + MTLTexture protocol */
#include <__sprt_abi/Metal/MTLArgument.h>

@protocol MTLBuffer;
@protocol MTLCommandEncoder;
@protocol MTLComputePipelineState;
@protocol MTLRenderPipelineState;

@class MTLArgument;

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
