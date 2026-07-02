/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 *
 * Hand-written <Metal/Metal.h> umbrella for the Xcode-SDK-free macOS target
 * (*-apple-macosx+open). Metal is a closed framework; these headers reconstruct ONLY the
 * surface MoltenVK compiles against (no symbols are linked — MoltenVK resolves Metal at run
 * time via -undefined dynamic_lookup). Foundation-first so the shared attribute-macro shims
 * (MTL_EXPORT/MTL_INLINE/API_AVAILABLE/…) and NS_ENUM/NS_ASSUME_NONNULL are in scope for all.
 */

#ifndef __SPRT_OPEN_METAL_H_
#define __SPRT_OPEN_METAL_H_

#import <Foundation/Foundation.h>

/* --- foundation types shared by the rest --- */
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLPipeline.h>
#import <Metal/MTLDrawable.h>

/* --- reflection / vertex / stage IO --- */
#import <Metal/MTLArgument.h>
#import <Metal/MTLStageInputOutputDescriptor.h>
#import <Metal/MTLVertexDescriptor.h>

/* --- resource objects --- */
#import <Metal/MTLTexture.h>
#import <Metal/MTLBuffer.h>
#import <Metal/MTLSampler.h>
#import <Metal/MTLDepthStencil.h>
#import <Metal/MTLHeap.h>

/* --- library / functions --- */
#import <Metal/MTLLibrary.h>
#import <Metal/MTLArgumentEncoder.h>
#import <Metal/MTLFunctionConstantValues.h>
#import <Metal/MTLFunctionDescriptor.h>
#import <Metal/MTLFunctionHandle.h>
#import <Metal/MTLFunctionLog.h>
#import <Metal/MTLFunctionStitching.h>
#import <Metal/MTLLinkedFunctions.h>
#import <Metal/MTLBinaryArchive.h>
#import <Metal/MTLDynamicLibrary.h>

/* --- pipelines --- */
#import <Metal/MTLRenderPipeline.h>
#import <Metal/MTLComputePipeline.h>

/* --- device --- */
#import <Metal/MTLDevice.h>
#import <Metal/MTLResidencySet.h>

/* --- command queue / buffer / encoders --- */
#import <Metal/MTLCommandEncoder.h>
#import <Metal/MTLCommandBuffer.h>
#import <Metal/MTLCommandQueue.h>
#import <Metal/MTLRenderPass.h>
#import <Metal/MTLComputePass.h>
#import <Metal/MTLBlitPass.h>
#import <Metal/MTLResourceStatePass.h>
#import <Metal/MTLRenderCommandEncoder.h>
#import <Metal/MTLComputeCommandEncoder.h>
#import <Metal/MTLBlitCommandEncoder.h>
#import <Metal/MTLParallelRenderCommandEncoder.h>
#import <Metal/MTLResourceStateCommandEncoder.h>

/* --- ray tracing / function tables --- */
#import <Metal/MTLAccelerationStructureTypes.h>
#import <Metal/MTLAccelerationStructure.h>
#import <Metal/MTLAccelerationStructureCommandEncoder.h>
#import <Metal/MTLVisibleFunctionTable.h>
#import <Metal/MTLIntersectionFunctionTable.h>

/* --- indirect command buffers --- */
#import <Metal/MTLIndirectCommandBuffer.h>
#import <Metal/MTLIndirectCommandEncoder.h>

/* --- sync / counters / capture / rasterization rate --- */
#import <Metal/MTLCounters.h>
#import <Metal/MTLFence.h>
#import <Metal/MTLEvent.h>
#import <Metal/MTLCaptureManager.h>
#import <Metal/MTLCaptureScope.h>
#import <Metal/MTLRasterizationRate.h>

/* --- fast resource IO --- */
#import <Metal/MTLIOCommandQueue.h>
#import <Metal/MTLIOCommandBuffer.h>
#import <Metal/MTLIOCompressor.h>

#endif /* __SPRT_OPEN_METAL_H_ */
