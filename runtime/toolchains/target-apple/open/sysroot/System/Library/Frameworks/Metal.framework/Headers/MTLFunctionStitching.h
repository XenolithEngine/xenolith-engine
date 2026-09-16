/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLFunctionStitching.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLFUNCTIONSTITCHING_H_
#define __SPRT_OPEN_METAL_MTLFUNCTIONSTITCHING_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLResource.h>

@protocol MTLFunction;

@protocol MTLFunctionStitchingAttribute <NSObject>
@end

@interface MTLFunctionStitchingAttributeAlwaysInline : NSObject <MTLFunctionStitchingAttribute>
@end

@protocol MTLFunctionStitchingNode <NSObject, NSCopying>
@end

@interface MTLFunctionStitchingInputNode : NSObject <MTLFunctionStitchingNode>
@property (readwrite, nonatomic) NSUInteger argumentIndex;
- (instancetype)initWithArgumentIndex:(NSUInteger)argument;
@end

@interface MTLFunctionStitchingFunctionNode : NSObject <MTLFunctionStitchingNode>
@property (readwrite, copy, nonatomic) NSString *name;
@property (readwrite, copy, nonatomic) NSArray<id<MTLFunctionStitchingNode>> *arguments;
@property (readwrite, copy, nonatomic) NSArray<MTLFunctionStitchingFunctionNode *> *controlDependencies;
- (instancetype)initWithName:(NSString *)name
                   arguments:(NSArray<id<MTLFunctionStitchingNode>> *)arguments
         controlDependencies:(NSArray<MTLFunctionStitchingFunctionNode *> *)controlDependencies;
@end

@interface MTLFunctionStitchingGraph : NSObject <NSCopying>
@property (readwrite, copy, nonatomic) NSString *functionName;
@property (readwrite, copy, nonatomic) NSArray<MTLFunctionStitchingFunctionNode *> *nodes;
@property (readwrite, copy, nonatomic) MTLFunctionStitchingFunctionNode *outputNode;
@property (readwrite, copy, nonatomic) NSArray<id<MTLFunctionStitchingAttribute>> *attributes;
- (instancetype)initWithFunctionName:(NSString *)functionName
                               nodes:(NSArray<MTLFunctionStitchingFunctionNode *> *)nodes
                          outputNode:(MTLFunctionStitchingFunctionNode *)outputNode
                          attributes:(NSArray<id<MTLFunctionStitchingAttribute>> *)attributes;
@end

@interface MTLStitchedLibraryDescriptor : NSObject <NSCopying>
@property (readwrite, copy, nonatomic) NSArray<MTLFunctionStitchingGraph *> *functionGraphs;
@property (readwrite, copy, nonatomic) NSArray<id<MTLFunction>> *functions;
@end

#endif /* __SPRT_OPEN_METAL_MTLFUNCTIONSTITCHING_H_ */
