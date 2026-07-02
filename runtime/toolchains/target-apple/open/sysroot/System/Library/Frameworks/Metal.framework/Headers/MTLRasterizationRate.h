/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLRasterizationRate.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_RASTERIZATIONRATE_H_
#define __SPRT_OPEN_METAL_RASTERIZATIONRATE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>   /* MTLSize, MTLSizeAndAlign */

@protocol MTLDevice;
@protocol MTLBuffer;

NS_ASSUME_NONNULL_BEGIN

/* Convenient access to the samples stored in a layer descriptor. */
@interface MTLRasterizationRateSampleArray : NSObject
- (NSNumber *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(NSNumber *)value atIndexedSubscript:(NSUInteger)index;
@end

/* Describes the minimum rasterization rate in screen space via two piecewise linear functions. */
@interface MTLRasterizationRateLayerDescriptor : NSObject
- (instancetype)init;
- (instancetype)initWithSampleCount:(MTLSize)sampleCount;
- (instancetype)initWithSampleCount:(MTLSize)sampleCount horizontal:(const float *)horizontal vertical:(const float *)vertical;
@property (readwrite, nonatomic) MTLSize sampleCount;
@property (readonly, nonatomic) MTLSize maxSampleCount;
@property (readonly, nonatomic) float *horizontalSampleStorage;
@property (readonly, nonatomic) float *verticalSampleStorage;
@property (readonly, nonatomic) MTLRasterizationRateSampleArray *horizontal;
@property (readonly, nonatomic) MTLRasterizationRateSampleArray *vertical;
@end

/* Mutable array of MTLRasterizationRateLayerDescriptor. */
@interface MTLRasterizationRateLayerArray : NSObject
- (MTLRasterizationRateLayerDescriptor * _Nullable)objectAtIndexedSubscript:(NSUInteger)layerIndex;
- (void)setObject:(MTLRasterizationRateLayerDescriptor * _Nullable)layer atIndexedSubscript:(NSUInteger)layerIndex;
@end

/* Describes a rate map containing an arbitrary number of layers. */
@interface MTLRasterizationRateMapDescriptor : NSObject
+ (MTLRasterizationRateMapDescriptor *)rasterizationRateMapDescriptorWithScreenSize:(MTLSize)screenSize;
+ (MTLRasterizationRateMapDescriptor *)rasterizationRateMapDescriptorWithScreenSize:(MTLSize)screenSize layer:(MTLRasterizationRateLayerDescriptor * _Nonnull)layer;
+ (MTLRasterizationRateMapDescriptor *)rasterizationRateMapDescriptorWithScreenSize:(MTLSize)screenSize layerCount:(NSUInteger)layerCount layers:(MTLRasterizationRateLayerDescriptor * const _Nonnull * _Nonnull)layers;
- (MTLRasterizationRateLayerDescriptor * _Nullable)layerAtIndex:(NSUInteger)layerIndex;
- (void)setLayer:(MTLRasterizationRateLayerDescriptor * _Nullable)layer atIndex:(NSUInteger)layerIndex;
@property (nonatomic, readonly) MTLRasterizationRateLayerArray *layers;
@property (nonatomic) MTLSize screenSize;
@property (nullable, copy, nonatomic) NSString *label;
@property (nonatomic, readonly) NSUInteger layerCount;
@end

/* Compiled read-only object determining how variable rasterization rate is applied. */
@protocol MTLRasterizationRateMap <NSObject>
@property (readonly) id<MTLDevice> device;
@property (nullable, readonly) NSString *label;
@property (readonly) MTLSize screenSize;
@property (readonly) MTLSize physicalGranularity;
@property (readonly) NSUInteger layerCount;
@property (readonly) MTLSizeAndAlign parameterBufferSizeAndAlign;
- (void)copyParameterDataToBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset;
- (MTLSize)physicalSizeForLayer:(NSUInteger)layerIndex;
@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_RASTERIZATIONRATE_H_ */
