/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLIntersectionFunctionTable.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLINTERSECTIONFUNCTIONTABLE_H_
#define __SPRT_OPEN_METAL_MTLINTERSECTIONFUNCTIONTABLE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLResource.h>
#include <__sprt_abi/Metal/MTLIntersectionFunctionTable.h>

@protocol MTLBuffer;
@protocol MTLFunctionHandle;
@protocol MTLVisibleFunctionTable;

@interface MTLIntersectionFunctionTableDescriptor : NSObject <NSCopying>

/*!
 @method intersectionFunctionTableDescriptor
 @abstract Create an autoreleased intersection function table descriptor
 */
+ (nonnull MTLIntersectionFunctionTableDescriptor *)intersectionFunctionTableDescriptor;

/*!
 * @property functionCount
 * @abstract The number of functions in the table.
 */
@property (nonatomic) NSUInteger functionCount;

@end

@protocol MTLIntersectionFunctionTable <MTLResource>

- (void)setBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range;

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID;

- (void)setFunction:(nullable id <MTLFunctionHandle>)function atIndex:(NSUInteger)index;
- (void)setFunctions:(const id <MTLFunctionHandle> __nullable [__nonnull])functions withRange:(NSRange)range;

- (void)setOpaqueTriangleIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature atIndex:(NSUInteger)index;
- (void)setOpaqueTriangleIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature withRange:(NSRange)range;

- (void)setOpaqueCurveIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature atIndex:(NSUInteger)index;
- (void)setOpaqueCurveIntersectionFunctionWithSignature:(MTLIntersectionFunctionSignature)signature withRange:(NSRange)range;

- (void)setVisibleFunctionTable:(nullable id <MTLVisibleFunctionTable>)functionTable atBufferIndex:(NSUInteger)bufferIndex;
- (void)setVisibleFunctionTables:(const id <MTLVisibleFunctionTable> __nullable [__nonnull])functionTables withBufferRange:(NSRange)bufferRange;

@end

#endif /* __SPRT_OPEN_METAL_MTLINTERSECTIONFUNCTIONTABLE_H_ */
