/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLTypes.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLTYPES_H_
#define __SPRT_OPEN_METAL_MTLTYPES_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#include <stdint.h>

typedef struct {
    NSUInteger x, y, z;
} MTLOrigin;

MTL_INLINE MTLOrigin MTLOriginMake(NSUInteger x, NSUInteger y, NSUInteger z)
{
    MTLOrigin origin = {x, y, z};
    return origin;
}

typedef struct {
    NSUInteger width, height, depth;
} MTLSize;

MTL_INLINE MTLSize MTLSizeMake(NSUInteger width, NSUInteger height, NSUInteger depth)
{
    MTLSize size = {width, height, depth};
    return size;
}

typedef struct {
    MTLOrigin origin;
    MTLSize   size;
} MTLRegion;

MTL_INLINE MTLRegion MTLRegionMake2D(NSUInteger x, NSUInteger y, NSUInteger width, NSUInteger height)
{
    MTLRegion region;
    region.origin.x = x; region.origin.y = y; region.origin.z = 0;
    region.size.width = width; region.size.height = height; region.size.depth = 1;
    return region;
}

MTL_INLINE MTLRegion MTLRegionMake3D(NSUInteger x, NSUInteger y, NSUInteger z, NSUInteger width, NSUInteger height, NSUInteger depth)
{
    MTLRegion region;
    region.origin.x = x; region.origin.y = y; region.origin.z = z;
    region.size.width = width; region.size.height = height; region.size.depth = depth;
    return region;
}

typedef struct {
    float x, y;
} MTLSamplePosition;

MTL_INLINE MTLSamplePosition MTLSamplePositionMake(float x, float y)
{
    MTLSamplePosition position = {x, y};
    return position;
}

typedef struct MTLResourceID {
    uint64_t _impl;
} MTLResourceID;

#endif /* __SPRT_OPEN_METAL_MTLTYPES_H_ */
