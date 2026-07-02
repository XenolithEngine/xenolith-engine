/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLFunctionDescriptor.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLFUNCTIONDESCRIPTOR_H_
#define __SPRT_OPEN_METAL_MTLFUNCTIONDESCRIPTOR_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLFunctionConstantValues.h>   /* MTLFunctionConstantValues (+ MTLDataType) */

@protocol MTLBinaryArchive;

typedef NS_OPTIONS(NSUInteger, MTLFunctionOptions) {
    MTLFunctionOptionNone = 0,
    MTLFunctionOptionCompileToBinary = 1 << 0,
    MTLFunctionOptionStoreFunctionInMetalScript = 1 << 1,
};

@interface MTLFunctionDescriptor : NSObject <NSCopying>

+ (MTLFunctionDescriptor *)functionDescriptor;

@property (copy, nonatomic) NSString *name;
@property (copy, nonatomic) NSString *specializedName;
@property (nonatomic, copy) MTLFunctionConstantValues *constantValues;
@property (nonatomic) MTLFunctionOptions options;
@property (readwrite, nonatomic, copy) NSArray<id<MTLBinaryArchive>> *binaryArchives;

@end

@interface MTLIntersectionFunctionDescriptor : MTLFunctionDescriptor <NSCopying>
@end

#endif /* __SPRT_OPEN_METAL_MTLFUNCTIONDESCRIPTOR_H_ */
