/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 * Hand-written <Metal/MTLPipeline.h> for *-apple-macosx+open — only the surface MoltenVK uses. */
#ifndef __SPRT_OPEN_METAL_MTLPIPELINE_H_
#define __SPRT_OPEN_METAL_MTLPIPELINE_H_

#import <Foundation/Foundation.h>

/* MoltenVK only pulls MTLDevice in through this header (Apple's MTLPipeline.h
 * does the same #import); MTLMutability / MTLPipelineBufferDescriptor* are
 * unused, so they are intentionally omitted. */
#import <Metal/MTLDevice.h>

#endif /* __SPRT_OPEN_METAL_MTLPIPELINE_H_ */
