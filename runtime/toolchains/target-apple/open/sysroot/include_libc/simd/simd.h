/* Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
 *
 * Hand-written <simd/simd.h> for the Xcode-SDK-free macOS target (*-apple-macosx+open).
 * Apple's simd library is SDK-only (not in apple-oss). MoltenVK's MVKFoundation.h pulls
 * <simd/simd.h> but only references the packed float vector types simd::float2/3/4, so this
 * reconstructs exactly those (as OpenCL/GCC ext_vector_type vectors, matching Apple's ABI)
 * plus the C `vector_floatN` and C++ `simd::floatN` spellings. Not the full simd surface.
 */

#ifndef __SPRT_OPEN_SIMD_SIMD_H_
#define __SPRT_OPEN_SIMD_SIMD_H_

typedef __attribute__((__ext_vector_type__(2))) float simd_float2;
typedef __attribute__((__ext_vector_type__(3))) float simd_float3;
typedef __attribute__((__ext_vector_type__(4))) float simd_float4;

typedef simd_float2 vector_float2;
typedef simd_float3 vector_float3;
typedef simd_float4 vector_float4;

#ifdef __cplusplus
namespace simd {
	typedef ::simd_float2 float2;
	typedef ::simd_float3 float3;
	typedef ::simd_float4 float4;
}
#endif /* __cplusplus */

#endif /* __SPRT_OPEN_SIMD_SIMD_H_ */
