/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLFunctionConstantValues.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLFUNCTIONCONSTANTVALUES_H_
#define __SPRT_OPEN_METAL_MTLFUNCTIONCONSTANTVALUES_H_

#import <Foundation/Foundation.h>
#import <Metal/MTLArgument.h>   /* MTLDataType */

@interface MTLFunctionConstantValues : NSObject <NSCopying>

/* using indices */
- (void)setConstantValue:(const void *)value type:(MTLDataType)type atIndex:(NSUInteger)index;
- (void)setConstantValues:(const void *)values type:(MTLDataType)type withRange:(NSRange)range;

/* using names */
- (void)setConstantValue:(const void *)value type:(MTLDataType)type withName:(NSString *)name;

/* delete all the constants */
- (void)reset;

@end

#endif /* __SPRT_OPEN_METAL_MTLFUNCTIONCONSTANTVALUES_H_ */
