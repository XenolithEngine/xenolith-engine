/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLAccelerationStructure.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURE_H_
#define __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURE_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLStageInputOutputDescriptor.h>
#import <Metal/MTLAccelerationStructureTypes.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLBuffer;
@protocol MTLAccelerationStructure;

typedef NS_OPTIONS(NSUInteger, MTLAccelerationStructureUsage) {
    MTLAccelerationStructureUsageNone = 0,
    MTLAccelerationStructureUsageRefit = (1 << 0),
    MTLAccelerationStructureUsagePreferFastBuild = (1 << 1),
    MTLAccelerationStructureUsageExtendedLimits = (1 << 2),
};

typedef NS_OPTIONS(uint32_t, MTLAccelerationStructureInstanceOptions) {
    MTLAccelerationStructureInstanceOptionNone = 0,
    MTLAccelerationStructureInstanceOptionDisableTriangleCulling = (1 << 0),
    MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise = (1 << 1),
    MTLAccelerationStructureInstanceOptionOpaque = (1 << 2),
    MTLAccelerationStructureInstanceOptionNonOpaque = (1 << 3),
};

/**
 * @brief Base class for acceleration structure descriptors. Do not use this class directly. Use
 * one of the derived classes instead.
 */
@interface MTLAccelerationStructureDescriptor : NSObject <NSCopying>

@property (nonatomic) MTLAccelerationStructureUsage usage;

@end

/**
 * @brief Base class for all geometry descriptors. Do not use this class directly. Use one of the derived
 * classes instead.
 */
@interface MTLAccelerationStructureGeometryDescriptor : NSObject <NSCopying>

@property (nonatomic) NSUInteger intersectionFunctionTableOffset;
@property (nonatomic) BOOL opaque;
@property (nonatomic) BOOL allowDuplicateIntersectionFunctionInvocation;
@property (nonatomic, copy, nullable) NSString* label;
@property (nonatomic, retain, nullable) id <MTLBuffer> primitiveDataBuffer;
@property (nonatomic) NSUInteger primitiveDataBufferOffset;
@property (nonatomic) NSUInteger primitiveDataStride;
@property (nonatomic) NSUInteger primitiveDataElementSize;

@end

/**
 * @brief Describes what happens to the object before the first motion key and after the last
 * motion key.
 */
typedef NS_ENUM(uint32_t, MTLMotionBorderMode) {
    MTLMotionBorderModeClamp = 0,
    MTLMotionBorderModeVanish = 1,
};

/**
 * @brief Descriptor for a primitive acceleration structure
 */
@interface MTLPrimitiveAccelerationStructureDescriptor : MTLAccelerationStructureDescriptor

@property (nonatomic, retain, nullable) NSArray <MTLAccelerationStructureGeometryDescriptor *> * geometryDescriptors;
@property (nonatomic) MTLMotionBorderMode motionStartBorderMode;
@property (nonatomic) MTLMotionBorderMode motionEndBorderMode;
@property (nonatomic) float motionStartTime;
@property (nonatomic) float motionEndTime;
@property (nonatomic) NSUInteger motionKeyframeCount;

+ (instancetype)descriptor;

@end

/**
 * @brief Descriptor for triangle geometry
 */
@interface MTLAccelerationStructureTriangleGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

@property (nonatomic, retain, nullable) id <MTLBuffer> vertexBuffer;
@property (nonatomic) NSUInteger vertexBufferOffset;
@property (nonatomic) MTLAttributeFormat vertexFormat;
@property (nonatomic) NSUInteger vertexStride;
@property (nonatomic, retain, nullable) id <MTLBuffer> indexBuffer;
@property (nonatomic) NSUInteger indexBufferOffset;
@property (nonatomic) MTLIndexType indexType;
@property (nonatomic) NSUInteger triangleCount;
@property (nonatomic, retain, nullable) id<MTLBuffer> transformationMatrixBuffer;
@property (nonatomic) NSUInteger transformationMatrixBufferOffset;

+ (instancetype)descriptor;

@end

/**
 * @brief Descriptor for bounding box geometry
 */
@interface MTLAccelerationStructureBoundingBoxGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

@property (nonatomic, retain, nullable) id <MTLBuffer> boundingBoxBuffer;
@property (nonatomic) NSUInteger boundingBoxBufferOffset;
@property (nonatomic) NSUInteger boundingBoxStride;
@property (nonatomic) NSUInteger boundingBoxCount;

+ (instancetype)descriptor;

@end

/**
 * @brief MTLbuffer and description how the data is stored in it.
 */
@interface MTLMotionKeyframeData : NSObject

@property (nonatomic, retain, nullable) id <MTLBuffer> buffer;
@property (nonatomic) NSUInteger offset;

+ (instancetype)data;

@end

/**
 * @brief Descriptor for motion triangle geometry
 */
@interface MTLAccelerationStructureMotionTriangleGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

@property (nonatomic, copy) NSArray <MTLMotionKeyframeData *> * vertexBuffers;
@property (nonatomic) MTLAttributeFormat vertexFormat;
@property (nonatomic) NSUInteger vertexStride;
@property (nonatomic, retain, nullable) id <MTLBuffer> indexBuffer;
@property (nonatomic) NSUInteger indexBufferOffset;
@property (nonatomic) MTLIndexType indexType;
@property (nonatomic) NSUInteger triangleCount;
@property (nonatomic, retain, nullable) id<MTLBuffer> transformationMatrixBuffer;
@property (nonatomic) NSUInteger transformationMatrixBufferOffset;

+ (instancetype)descriptor;

@end

/**
 * @brief Descriptor for motion bounding box geometry
 */
@interface MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

@property (nonatomic, copy) NSArray <MTLMotionKeyframeData *> * boundingBoxBuffers;
@property (nonatomic) NSUInteger boundingBoxStride;
@property (nonatomic) NSUInteger boundingBoxCount;

+ (instancetype)descriptor;

@end

/**
 * @brief Curve types
 */
typedef NS_ENUM(NSInteger, MTLCurveType) {
    MTLCurveTypeRound = 0,
    MTLCurveTypeFlat = 1,
};

/**
 * @brief Basis function to use to interpolate curve control points
 */
typedef NS_ENUM(NSInteger, MTLCurveBasis) {
    MTLCurveBasisBSpline = 0,
    MTLCurveBasisCatmullRom = 1,
    MTLCurveBasisLinear = 2,
    MTLCurveBasisBezier = 3,
};

/**
 * @brief Type of end cap to insert at the beginning and end of each connected
 * sequence of curve segments.
 */
typedef NS_ENUM(NSInteger, MTLCurveEndCaps) {
    MTLCurveEndCapsNone = 0,
    MTLCurveEndCapsDisk = 1,
    MTLCurveEndCapsSphere = 2,
};

/**
 * @brief Acceleration structure geometry descriptor describing geometry
 * made of curve primitives
 */
@interface MTLAccelerationStructureCurveGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

@property (nonatomic, retain, nullable) id <MTLBuffer> controlPointBuffer;
@property (nonatomic) NSUInteger controlPointBufferOffset;
@property (nonatomic) NSUInteger controlPointCount;
@property (nonatomic) NSUInteger controlPointStride;
@property (nonatomic) MTLAttributeFormat controlPointFormat;
@property (nonatomic, retain, nullable) id <MTLBuffer> radiusBuffer;
@property (nonatomic) NSUInteger radiusBufferOffset;
@property (nonatomic) MTLAttributeFormat radiusFormat;
@property (nonatomic) NSUInteger radiusStride;
@property (nonatomic, retain, nullable) id <MTLBuffer> indexBuffer;
@property (nonatomic) NSUInteger indexBufferOffset;
@property (nonatomic) MTLIndexType indexType;
@property (nonatomic) NSUInteger segmentCount;
@property (nonatomic) NSUInteger segmentControlPointCount;
@property (nonatomic) MTLCurveType curveType;
@property (nonatomic) MTLCurveBasis curveBasis;
@property (nonatomic) MTLCurveEndCaps curveEndCaps;

+ (instancetype)descriptor;

@end

/**
 * @brief Acceleration structure motion geometry descriptor describing
 * geometry made of curve primitives
 */
@interface MTLAccelerationStructureMotionCurveGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

@property (nonatomic, copy) NSArray <MTLMotionKeyframeData *> *controlPointBuffers;
@property (nonatomic) NSUInteger controlPointCount;
@property (nonatomic) NSUInteger controlPointStride;
@property (nonatomic) MTLAttributeFormat controlPointFormat;
@property (nonatomic, copy) NSArray <MTLMotionKeyframeData *> *radiusBuffers;
@property (nonatomic) MTLAttributeFormat radiusFormat;
@property (nonatomic) NSUInteger radiusStride;
@property (nonatomic, retain, nullable) id <MTLBuffer> indexBuffer;
@property (nonatomic) NSUInteger indexBufferOffset;
@property (nonatomic) MTLIndexType indexType;
@property (nonatomic) NSUInteger segmentCount;
@property (nonatomic) NSUInteger segmentControlPointCount;
@property (nonatomic) MTLCurveType curveType;
@property (nonatomic) MTLCurveBasis curveBasis;
@property (nonatomic) MTLCurveEndCaps curveEndCaps;

+ (instancetype)descriptor;

@end

typedef struct {
    MTLPackedFloat4x3 transformationMatrix;
    MTLAccelerationStructureInstanceOptions options;
    uint32_t mask;
    uint32_t intersectionFunctionTableOffset;
    uint32_t accelerationStructureIndex;
} MTLAccelerationStructureInstanceDescriptor;

typedef struct {
    MTLPackedFloat4x3 transformationMatrix;
    MTLAccelerationStructureInstanceOptions options;
    uint32_t mask;
    uint32_t intersectionFunctionTableOffset;
    uint32_t accelerationStructureIndex;
    uint32_t userID;
} MTLAccelerationStructureUserIDInstanceDescriptor;

typedef NS_ENUM(NSUInteger, MTLAccelerationStructureInstanceDescriptorType) {
    MTLAccelerationStructureInstanceDescriptorTypeDefault = 0,
    MTLAccelerationStructureInstanceDescriptorTypeUserID = 1,
    MTLAccelerationStructureInstanceDescriptorTypeMotion = 2,
    MTLAccelerationStructureInstanceDescriptorTypeIndirect = 3,
    MTLAccelerationStructureInstanceDescriptorTypeIndirectMotion = 4,
};

typedef struct {
    MTLAccelerationStructureInstanceOptions options;
    uint32_t mask;
    uint32_t intersectionFunctionTableOffset;
    uint32_t accelerationStructureIndex;
    uint32_t userID;
    uint32_t motionTransformsStartIndex;
    uint32_t motionTransformsCount;
    MTLMotionBorderMode motionStartBorderMode;
    MTLMotionBorderMode motionEndBorderMode;
    float motionStartTime;
    float motionEndTime;
} MTLAccelerationStructureMotionInstanceDescriptor;

typedef struct {
    MTLPackedFloat4x3 transformationMatrix;
    MTLAccelerationStructureInstanceOptions options;
    uint32_t mask;
    uint32_t intersectionFunctionTableOffset;
    uint32_t userID;
    MTLResourceID accelerationStructureID;
} MTLIndirectAccelerationStructureInstanceDescriptor;

typedef struct {
    MTLAccelerationStructureInstanceOptions options;
    uint32_t mask;
    uint32_t intersectionFunctionTableOffset;
    uint32_t userID;
    MTLResourceID accelerationStructureID;
    uint32_t motionTransformsStartIndex;
    uint32_t motionTransformsCount;
    MTLMotionBorderMode motionStartBorderMode;
    MTLMotionBorderMode motionEndBorderMode;
    float motionStartTime;
    float motionEndTime;
} MTLIndirectAccelerationStructureMotionInstanceDescriptor;

/**
 * @brief Descriptor for an instance acceleration structure
 */
@interface MTLInstanceAccelerationStructureDescriptor : MTLAccelerationStructureDescriptor

@property (nonatomic, retain, nullable) id <MTLBuffer> instanceDescriptorBuffer;
@property (nonatomic) NSUInteger instanceDescriptorBufferOffset;
@property (nonatomic) NSUInteger instanceDescriptorStride;
@property (nonatomic) NSUInteger instanceCount;
@property (nonatomic, retain, nullable) NSArray <id <MTLAccelerationStructure>> * instancedAccelerationStructures;
@property (nonatomic) MTLAccelerationStructureInstanceDescriptorType instanceDescriptorType;
@property (nonatomic, retain, nullable) id <MTLBuffer> motionTransformBuffer;
@property (nonatomic) NSUInteger motionTransformBufferOffset;
@property (nonatomic) NSUInteger motionTransformCount;

+ (instancetype)descriptor;

@end

/**
 * @brief Descriptor for an instance acceleration structure built with an indirected buffer of instances.
 */
@interface MTLIndirectInstanceAccelerationStructureDescriptor : MTLAccelerationStructureDescriptor

@property (nonatomic, retain, nullable) id <MTLBuffer> instanceDescriptorBuffer;
@property (nonatomic) NSUInteger instanceDescriptorBufferOffset;
@property (nonatomic) NSUInteger instanceDescriptorStride;
@property (nonatomic) NSUInteger maxInstanceCount;
@property (nonatomic, retain, nullable) id <MTLBuffer> instanceCountBuffer;
@property (nonatomic) NSUInteger instanceCountBufferOffset;
@property (nonatomic) MTLAccelerationStructureInstanceDescriptorType instanceDescriptorType;
@property (nonatomic, retain, nullable) id <MTLBuffer> motionTransformBuffer;
@property (nonatomic) NSUInteger motionTransformBufferOffset;
@property (nonatomic) NSUInteger maxMotionTransformCount;
@property (nonatomic, retain, nullable) id <MTLBuffer> motionTransformCountBuffer;
@property (nonatomic) NSUInteger motionTransformCountBufferOffset;

+ (instancetype)descriptor;

@end

@protocol MTLAccelerationStructure <MTLResource>

@property (nonatomic, readonly) NSUInteger size;

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID;

@end

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLACCELERATIONSTRUCTURE_H_ */
