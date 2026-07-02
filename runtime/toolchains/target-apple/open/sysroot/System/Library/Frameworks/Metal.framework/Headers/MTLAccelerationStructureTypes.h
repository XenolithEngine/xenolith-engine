/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLAccelerationStructureTypes.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURETYPES_H_
#define __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURETYPES_H_

#import <Foundation/Foundation.h>
#include <math.h>

typedef struct _MTLPackedFloat3 {
    union {
        struct {
            float x;
            float y;
            float z;
        };
        float elements[3];
    };

#ifdef __cplusplus
    _MTLPackedFloat3()
        : x(0.0f), y(0.0f), z(0.0f)
    {
    }

    _MTLPackedFloat3(float x, float y, float z)
        : x(x), y(y), z(z)
    {
    }

    float & operator[](int idx) {
        return elements[idx];
    }

    const float & operator[](int idx) const {
        return elements[idx];
    }
#endif
} MTLPackedFloat3;

static inline MTLPackedFloat3 MTLPackedFloat3Make(float x, float y, float z)
{
    MTLPackedFloat3 packedFloat3;
    packedFloat3.x = x;
    packedFloat3.y = y;
    packedFloat3.z = z;
    return packedFloat3;
}

typedef struct _MTLPackedFloat4x3 {
    MTLPackedFloat3 columns[4];

#ifdef __cplusplus
    _MTLPackedFloat4x3() {
        columns[0] = MTLPackedFloat3(0.0f, 0.0f, 0.0f);
        columns[1] = MTLPackedFloat3(0.0f, 0.0f, 0.0f);
        columns[2] = MTLPackedFloat3(0.0f, 0.0f, 0.0f);
        columns[3] = MTLPackedFloat3(0.0f, 0.0f, 0.0f);
    }

    _MTLPackedFloat4x3(MTLPackedFloat3 column0, MTLPackedFloat3 column1, MTLPackedFloat3 column2, MTLPackedFloat3 column3) {
        columns[0] = column0;
        columns[1] = column1;
        columns[2] = column2;
        columns[3] = column3;
    }

    MTLPackedFloat3 & operator[](int idx) {
        return columns[idx];
    }

    const MTLPackedFloat3 & operator[](int idx) const {
        return columns[idx];
    }
#endif
} MTLPackedFloat4x3;

/**
 * @brief An axis aligned bounding box with a min and max point
 */
typedef struct _MTLAxisAlignedBoundingBox {
    /**
     * @brief Minimum point
     */
    MTLPackedFloat3 min;

    /**
     * @brief Maximum point
     */
    MTLPackedFloat3 max;

#ifdef __cplusplus
    _MTLAxisAlignedBoundingBox()
        : min(INFINITY, INFINITY, INFINITY),
          max(-INFINITY, -INFINITY, -INFINITY)
    {
    }

    _MTLAxisAlignedBoundingBox(MTLPackedFloat3 p)
        : min(p),
          max(p)
    {
    }

    _MTLAxisAlignedBoundingBox(MTLPackedFloat3 min, MTLPackedFloat3 max)
        : min(min),
          max(max)
    {
    }
#endif
} MTLAxisAlignedBoundingBox;

#endif /* __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURETYPES_H_ */
