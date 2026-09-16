/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/
#define __SPRT_BUILD 1
#include <sprt/runtime/config.h>
#include <sprt/c/bits/__sprt_def.h>
#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW && SPRT_WASM
#include "SPRTWinWasmController.cc"
#include "SPRTWinWasmWindow.cc"
#endif
