/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLDepthStencil.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_DEPTHSTENCIL_H_
#define __SPRT_OPEN_METAL_DEPTHSTENCIL_H_

#import <Foundation/Foundation.h>

@protocol MTLDevice;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, MTLCompareFunction) {
	MTLCompareFunctionNever        = 0,
	MTLCompareFunctionLess         = 1,
	MTLCompareFunctionEqual        = 2,
	MTLCompareFunctionLessEqual    = 3,
	MTLCompareFunctionGreater      = 4,
	MTLCompareFunctionNotEqual     = 5,
	MTLCompareFunctionGreaterEqual = 6,
	MTLCompareFunctionAlways       = 7,
};

typedef NS_ENUM(NSUInteger, MTLStencilOperation) {
	MTLStencilOperationKeep           = 0,
	MTLStencilOperationZero           = 1,
	MTLStencilOperationReplace        = 2,
	MTLStencilOperationIncrementClamp = 3,
	MTLStencilOperationDecrementClamp = 4,
	MTLStencilOperationInvert         = 5,
	MTLStencilOperationIncrementWrap  = 6,
	MTLStencilOperationDecrementWrap  = 7,
};

@interface MTLStencilDescriptor : NSObject <NSCopying>
@property (nonatomic) MTLCompareFunction stencilCompareFunction;
@property (nonatomic) MTLStencilOperation stencilFailureOperation;
@property (nonatomic) MTLStencilOperation depthFailureOperation;
@property (nonatomic) MTLStencilOperation depthStencilPassOperation;
@property (nonatomic) uint32_t readMask;
@property (nonatomic) uint32_t writeMask;
@end

@interface MTLDepthStencilDescriptor : NSObject <NSCopying>
@property (nonatomic) MTLCompareFunction depthCompareFunction;
@property (nonatomic, getter=isDepthWriteEnabled) BOOL depthWriteEnabled;
@property (copy, nonatomic, null_resettable) MTLStencilDescriptor *frontFaceStencil;
@property (copy, nonatomic, null_resettable) MTLStencilDescriptor *backFaceStencil;
@property (nullable, copy, nonatomic) NSString *label;
@end

@protocol MTLDepthStencilState <NSObject>
@property (nullable, readonly) NSString *label;
@property (readonly) id <MTLDevice> device;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_DEPTHSTENCIL_H_ */
