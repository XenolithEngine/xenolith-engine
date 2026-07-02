/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLFunctionLog.h> for *-apple-macosx+open — only the surface MoltenVK uses. */

#ifndef __SPRT_OPEN_METAL_MTLFUNCTIONLOG_H_
#define __SPRT_OPEN_METAL_MTLFUNCTIONLOG_H_

#import <Foundation/Foundation.h>

@protocol MTLFunction;

typedef NS_ENUM(NSUInteger, MTLFunctionLogType) {
    MTLFunctionLogTypeValidation = 0,
};

@protocol MTLLogContainer <NSFastEnumeration>
@end

@protocol MTLFunctionLogDebugLocation <NSObject>
@property (readonly, nonatomic) NSString *functionName;   /* faulting function */
@property (readonly, nonatomic) NSURL *URL;               /* source location */
@property (readonly, nonatomic) NSUInteger line;          /* line number */
@property (readonly, nonatomic) NSUInteger column;        /* column in line */
@end

@protocol MTLFunctionLog <NSObject>
@property (readonly, nonatomic) MTLFunctionLogType type;
@property (readonly, nonatomic) NSString *encoderLabel;
@property (readonly, nonatomic) id<MTLFunction> function;
@property (readonly, nonatomic) id<MTLFunctionLogDebugLocation> debugLocation;
@end

#endif /* __SPRT_OPEN_METAL_MTLFUNCTIONLOG_H_ */
