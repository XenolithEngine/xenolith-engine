/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLLinkedFunctions.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLLINKEDFUNCTIONS_H_
#define __SPRT_OPEN_METAL_MTLLINKEDFUNCTIONS_H_

#import <Foundation/Foundation.h>

@protocol MTLFunction;

@interface MTLLinkedFunctions : NSObject <NSCopying>

+ (MTLLinkedFunctions *)linkedFunctions;

@property (readwrite, nonatomic, copy) NSArray<id<MTLFunction>> *functions;
@property (readwrite, nonatomic, copy) NSArray<id<MTLFunction>> *binaryFunctions;
@property (readwrite, nonatomic, copy) NSDictionary<NSString *, NSArray<id<MTLFunction>> *> *groups;
@property (readwrite, nonatomic, copy) NSArray<id<MTLFunction>> *privateFunctions;

@end

#endif /* __SPRT_OPEN_METAL_MTLLINKEDFUNCTIONS_H_ */
