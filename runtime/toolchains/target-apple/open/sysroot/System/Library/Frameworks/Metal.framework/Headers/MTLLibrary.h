/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLLibrary.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLLIBRARY_H_
#define __SPRT_OPEN_METAL_MTLLIBRARY_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLFunctionDescriptor.h>   /* MTLFunctionOptions, MTLFunctionDescriptor, MTLIntersectionFunctionDescriptor, MTLFunctionConstantValues */

@protocol MTLDevice;
@protocol MTLDynamicLibrary;
@protocol MTLArgumentEncoder;
@protocol MTLComputePipelineState;
@protocol MTLRenderPipelineState;
@protocol MTLFunction;
@protocol MTLLibrary;

typedef NS_ENUM(NSUInteger, MTLPatchType) {
    MTLPatchTypeNone = 0,
    MTLPatchTypeTriangle = 1,
    MTLPatchTypeQuad = 2,
};

typedef NS_ENUM(NSUInteger, MTLFunctionType) {
    MTLFunctionTypeVertex = 1,
    MTLFunctionTypeFragment = 2,
    MTLFunctionTypeKernel = 3,
    MTLFunctionTypeVisible = 5,
    MTLFunctionTypeIntersection = 6,
    MTLFunctionTypeMesh = 7,
    MTLFunctionTypeObject = 8,
};

typedef NS_ENUM(NSUInteger, MTLLanguageVersion) {
    MTLLanguageVersion1_0 = (1 << 16),
    MTLLanguageVersion1_1 = (1 << 16) + 1,
    MTLLanguageVersion1_2 = (1 << 16) + 2,
    MTLLanguageVersion2_0 = (2 << 16),
    MTLLanguageVersion2_1 = (2 << 16) + 1,
    MTLLanguageVersion2_2 = (2 << 16) + 2,
    MTLLanguageVersion2_3 = (2 << 16) + 3,
    MTLLanguageVersion2_4 = (2 << 16) + 4,
    MTLLanguageVersion3_0 = (3 << 16) + 0,
    MTLLanguageVersion3_1 = (3 << 16) + 1,
    MTLLanguageVersion3_2 = (3 << 16) + 2,
};

typedef NS_ENUM(NSInteger, MTLLibraryType) {
    MTLLibraryTypeExecutable = 0,
    MTLLibraryTypeDynamic = 1,
};

typedef NS_ENUM(NSInteger, MTLLibraryOptimizationLevel) {
    MTLLibraryOptimizationLevelDefault = 0,
    MTLLibraryOptimizationLevelSize = 1,
};

@interface MTLFunctionConstant : NSObject
@property (readonly) NSString *name;
@property (readonly) MTLDataType type;
@property (readonly) NSUInteger index;
@property (readonly) BOOL required;
@end

@protocol MTLFunction <NSObject>
@property (copy, atomic) NSString *label;
@property (readonly) id<MTLDevice> device;
@property (readonly) MTLFunctionType functionType;
@property (readonly) MTLPatchType patchType;
@property (readonly) NSInteger patchControlPointCount;
@property (readonly) NSString *name;
@property (readonly) NSDictionary<NSString *, MTLFunctionConstant *> *functionConstantsDictionary;
- (id<MTLArgumentEncoder>)newArgumentEncoderWithBufferIndex:(NSUInteger)bufferIndex;
@property (readonly) MTLFunctionOptions options;
@end

/* macOS-15 math controls (absent from the 14.5 SDK snapshot; MoltenVK sets them behind an OS
   check). Values match the Metal 3.2 ABI. */
typedef NS_ENUM(NSInteger, MTLMathMode) {
    MTLMathModeSafe = 0,
    MTLMathModeRelaxed = 1,
    MTLMathModeFast = 2,
};
typedef NS_ENUM(NSInteger, MTLMathFloatingPointFunctions) {
    MTLMathFloatingPointFunctionsFast = 0,
    MTLMathFloatingPointFunctionsPrecise = 1,
};

@interface MTLCompileOptions : NSObject <NSCopying>
@property (readwrite, copy, nonatomic) NSDictionary<NSString *, NSObject *> *preprocessorMacros;
@property (readwrite, nonatomic) BOOL fastMathEnabled;
@property (readwrite, nonatomic) MTLLanguageVersion languageVersion;
@property (readwrite, nonatomic) MTLLibraryType libraryType;
@property (readwrite, copy, nonatomic) NSString *installName;
@property (readwrite, copy, nonatomic) NSArray<id<MTLDynamicLibrary>> *libraries;
@property (readwrite, nonatomic) BOOL preserveInvariance;
@property (readwrite, nonatomic) MTLLibraryOptimizationLevel optimizationLevel;
@property (readwrite, nonatomic) MTLMathMode mathMode;
@property (readwrite, nonatomic) MTLMathFloatingPointFunctions mathFloatingPointFunctions;
@end

@protocol MTLLibrary <NSObject>
@property (copy, atomic) NSString *label;
@property (readonly) id<MTLDevice> device;
- (id<MTLFunction>)newFunctionWithName:(NSString *)functionName;
- (id<MTLFunction>)newFunctionWithName:(NSString *)name constantValues:(MTLFunctionConstantValues *)constantValues
                                 error:(NSError **)error;
- (void)newFunctionWithName:(NSString *)name constantValues:(MTLFunctionConstantValues *)constantValues
          completionHandler:(void (^)(id<MTLFunction> function, NSError *error))completionHandler;
- (void)newFunctionWithDescriptor:(MTLFunctionDescriptor *)descriptor
                completionHandler:(void (^)(id<MTLFunction> function, NSError *error))completionHandler;
- (id<MTLFunction>)newFunctionWithDescriptor:(MTLFunctionDescriptor *)descriptor error:(NSError **)error;
- (void)newIntersectionFunctionWithDescriptor:(MTLIntersectionFunctionDescriptor *)descriptor
                            completionHandler:(void (^)(id<MTLFunction> function, NSError *error))completionHandler;
- (id<MTLFunction>)newIntersectionFunctionWithDescriptor:(MTLIntersectionFunctionDescriptor *)descriptor error:(NSError **)error;
@property (readonly) NSArray<NSString *> *functionNames;
@property (readonly) MTLLibraryType type;
@property (readonly) NSString *installName;
@end

#endif /* __SPRT_OPEN_METAL_MTLLIBRARY_H_ */
