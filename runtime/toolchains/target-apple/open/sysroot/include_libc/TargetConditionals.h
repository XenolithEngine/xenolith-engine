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

/* ---- platform: macOS (desktop) ------------------------------------------- */
#define TARGET_OS_MAC               1
#define TARGET_OS_OSX               1
#define TARGET_OS_IPHONE            0
#define TARGET_OS_IOS               0
#define TARGET_OS_WATCH             0
#define TARGET_OS_TV                0
#define TARGET_OS_VISION            0
#define TARGET_OS_MACCATALYST       0
#define TARGET_OS_UIKITFORMAC       0
#define TARGET_OS_SIMULATOR         0
#define TARGET_OS_EMBEDDED          0
#define TARGET_OS_DRIVERKIT         0
#define TARGET_OS_WINDOWS           0
#define TARGET_OS_LINUX             0
#define TARGET_OS_BRIDGE            0
#define TARGET_IPHONE_SIMULATOR     0   /* deprecated alias */
#define TARGET_OS_NANO              0   /* deprecated alias */

/* ---- cpu ----------------------------------------------------------------- */
#if defined(__x86_64__)
    #define TARGET_CPU_X86          0
    #define TARGET_CPU_X86_64       1
    #define TARGET_CPU_ARM          0
    #define TARGET_CPU_ARM64        0
    #define TARGET_RT_64_BIT        1
#elif defined(__i386__)
    #define TARGET_CPU_X86          1
    #define TARGET_CPU_X86_64       0
    #define TARGET_CPU_ARM          0
    #define TARGET_CPU_ARM64        0
    #define TARGET_RT_64_BIT        0
#elif defined(__arm64__) || defined(__aarch64__)
    #define TARGET_CPU_X86          0
    #define TARGET_CPU_X86_64       0
    #define TARGET_CPU_ARM          0
    #define TARGET_CPU_ARM64        1
    #define TARGET_RT_64_BIT        1
#elif defined(__arm__)
    #define TARGET_CPU_X86          0
    #define TARGET_CPU_X86_64       0
    #define TARGET_CPU_ARM          1
    #define TARGET_CPU_ARM64        0
    #define TARGET_RT_64_BIT        0
#else
    #error unsupported architecture for the +open macOS target
#endif

#define TARGET_CPU_PPC              0
#define TARGET_CPU_PPC64           0
#define TARGET_CPU_68K             0
#define TARGET_CPU_MIPS            0
#define TARGET_CPU_SPARC          0
#define TARGET_CPU_ALPHA          0

/* ---- runtime ------------------------------------------------------------- */
#define TARGET_RT_MAC_CFM          0
#define TARGET_RT_MAC_MACHO        1
#define TARGET_RT_LITTLE_ENDIAN    1
#define TARGET_RT_BIG_ENDIAN       0
#if defined(__x86_64__) || defined(__arm64__) || defined(__aarch64__)
#define TARGET_RT_MAC_64           1
#else
#define TARGET_RT_MAC_64           0
#endif

#endif /* __TARGETCONDITIONALS__ */
