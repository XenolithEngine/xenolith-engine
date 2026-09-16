/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLIOCompressor.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLIOCOMPRESSOR_H_
#define __SPRT_OPEN_METAL_MTLIOCOMPRESSOR_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, MTLIOCompressionStatus) {
    MTLIOCompressionStatusComplete = 0,
    MTLIOCompressionStatusError    = 1,
};

typedef NS_ENUM(NSInteger, MTLIOCompressionMethod) {
    MTLIOCompressionMethodZlib     = 0,
    MTLIOCompressionMethodLZFSE    = 1,
    MTLIOCompressionMethodLZ4      = 2,
    MTLIOCompressionMethodLZMA     = 3,
    MTLIOCompressionMethodLZBitmap = 4,
};

typedef void *MTLIOCompressionContext;

#ifdef __cplusplus
extern "C" {
#endif

size_t MTLIOCompressionContextDefaultChunkSize(void);

_Nullable MTLIOCompressionContext MTLIOCreateCompressionContext(const char *path, MTLIOCompressionMethod type, size_t chunkSize);

void MTLIOCompressionContextAppendData(MTLIOCompressionContext context, const void *data, size_t size);

MTLIOCompressionStatus MTLIOFlushAndDestroyCompressionContext(MTLIOCompressionContext context);

#ifdef __cplusplus
}
#endif

NS_ASSUME_NONNULL_END

#endif /* __SPRT_OPEN_METAL_MTLIOCOMPRESSOR_H_ */
