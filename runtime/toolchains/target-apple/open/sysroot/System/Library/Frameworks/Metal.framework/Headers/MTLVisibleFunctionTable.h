/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLVisibleFunctionTable.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLVISIBLEFUNCTIONTABLE_H_
#define __SPRT_OPEN_METAL_MTLVISIBLEFUNCTIONTABLE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLResource.h>

@protocol MTLFunctionHandle;

@interface MTLVisibleFunctionTableDescriptor : NSObject <NSCopying>

/*!
 @method visibleFunctionTableDescriptor
 @abstract Create an autoreleased visible function table descriptor
 */
+ (nonnull MTLVisibleFunctionTableDescriptor *)visibleFunctionTableDescriptor;

/*!
 * @property functionCount
 * @abstract The number of functions in the table.
 */
@property (nonatomic) NSUInteger functionCount;

@end

@protocol MTLVisibleFunctionTable <MTLResource>

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID;

- (void)setFunction:(nullable id <MTLFunctionHandle>)function atIndex:(NSUInteger)index;

- (void)setFunctions:(const id <MTLFunctionHandle> __nullable [__nonnull])functions withRange:(NSRange)range;

@end

#endif /* __SPRT_OPEN_METAL_MTLVISIBLEFUNCTIONTABLE_H_ */
