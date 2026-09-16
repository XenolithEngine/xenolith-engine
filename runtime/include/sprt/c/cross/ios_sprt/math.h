// iOS shares the Darwin/libSystem ABI with macOS for this target/arch.
// Forward to the macOS cross-config so the two stay in lockstep; if iOS ever
// needs to diverge, replace this include with iOS-specific definitions.
#include <sprt/c/cross/macos_sprt/math.h>
