/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLIOCompressor.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLIOCOMPRESSOR_H_
#define __SPRT_OPEN_METAL_MTLIOCOMPRESSOR_H_

#import <Foundation/Foundation.h>
#include <__sprt_abi/Metal/MTLIOCompressor.h>

NS_ASSUME_NONNULL_BEGIN

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
