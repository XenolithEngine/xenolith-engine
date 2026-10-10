/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLLibrary.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLLIBRARY_H_
#define __SPRT_OPEN_METAL_MTLLIBRARY_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLFunctionDescriptor.h>   /* MTLFunctionOptions, MTLFunctionDescriptor, MTLIntersectionFunctionDescriptor, MTLFunctionConstantValues */
#include <__sprt_abi/Metal/MTLLibrary.h>

@protocol MTLDevice;
@protocol MTLDynamicLibrary;
@protocol MTLArgumentEncoder;
@protocol MTLComputePipelineState;
@protocol MTLRenderPipelineState;
@protocol MTLFunction;
@protocol MTLLibrary;

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
