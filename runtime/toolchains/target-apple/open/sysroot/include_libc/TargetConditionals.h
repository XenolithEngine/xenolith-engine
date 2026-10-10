/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)

Modern hand-written <TargetConditionals.h> for the Xcode-SDK-free macOS target
(*-apple-macosx+open). The apple-oss CarbonHeaders TargetConditionals.h predates
arm64 (it only knows __arm__ → #error on aarch64), so this replaces it with the
current TARGET_OS_* / TARGET_CPU_* / TARGET_RT_* surface, arch-dispatched for
x86_64 + arm64. macOS-only (the +open target is desktop macOS).
**/

#ifndef __TARGETCONDITIONALS__
#define __TARGETCONDITIONALS__
#include <__sprt_abi/TargetConditionals.h>

/* ---- cpu ----------------------------------------------------------------- */

#endif /* __TARGETCONDITIONALS__ */
