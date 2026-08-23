# Copyright (c) 2025 Stappler Team <admin@stappler.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# Where every third-party source comes from, and what it has to match before it
# is allowed into src/.
#
# Each dependency is a block of variables named after it plus a one-line recipe.
# The download machinery - retries, hash and signature checking, unpacking,
# atomic install - lives in common/utils/fetch.mk and fetch.sh/fetch.ps1; see the
# header of fetch.mk for the full list of variables a block may set.
#
# Three things are checked for every dependency, and any of them failing stops
# the build rather than leaving a broken tree in src/:
#
#   * the transfer succeeded - a 404 page or a truncated file no longer reaches
#     tar, which used to fail several steps later with an unrelated message;
#   * _SHA256 matches. This is the check that protects an everyday build: the
#     expected value is in our git history, not on the server we downloaded from;
#   * _SIG verifies against keys/<_KEY>.asc, for the fourteen upstreams that
#     publish a detached OpenPGP signature. That is what makes a _SHA256 pin
#     trustworthy at the one moment it is written down - when somebody bumps a
#     version - so re-pin from a host with gpg installed.
#
# Sources fetched with git are pinned by _COMMIT as well as by _TAG, because a
# tag is a mutable pointer: if upstream re-tags, the clone is refused instead of
# silently building something else. `make src-pins` re-resolves every tag to the
# commit it points at today, which is how a version bump is prepared.
#
# Where an upstream publishes neither a signature nor anything else to verify
# against, the block says so and why it is acceptable.

ifeq ($(findstring Windows,$(OS)),Windows)
SHELL = powershell.exe
endif

include $(LIBS_MAKE_ROOT)common/utils/llvm-version.mk
include $(LIBS_MAKE_ROOT)common/utils/fetch.mk

LIBS = \
	bzip2 \
	xz \
	zstd \
	libjpeg-turbo \
	libpng \
	giflib \
	libwebp \
	tiff \
	brotli \
	curl \
	freetype \
	harfbuzz \
	sheenbidi \
	sqlite \
	mbedtls \
	nghttp3 \
	ngtcp2 \
	zlib \
	openssl \
	openssl-gost-engine \
	wasm-micro-runtime \
	vulkan-headers \
	vulkan-loader \
	vulkan-validationlayers \
	vulkan-utility \
	vulkan-tools \
	moltenvk \
	spirv-headers \
	glslang \
	spirv-tools \
	icu4c \
	ffi \
	expat \
	libbacktrace \
	simde \
	llvm-project \
	libxml2 \
	wayland \
	wayland-protocols \
	plasma-wayland-protocols \
	libdrm

# One tag drives every Khronos repository below; they are released together and
# mixing SDK versions across them is not supported upstream.
VULKAN_SDK_VER := 1.4.357.0


# https://www.zlib.net/ # revised: 18 aug 2026
zlib_URL    := https://www.zlib.net/zlib-1.3.2.tar.gz
zlib_SHA256 := bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
zlib_SIG    := .asc
zlib_KEY    := zlib

$(SRC_ROOT)/zlib: | prepare
	$(call sp_fetch_tar,zlib)

# https://sourceware.org/bzip2/downloads.html # revised: 18 aug 2026
bzip2_URL    := https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
bzip2_SHA256 := ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269
bzip2_SIG    := .sig
bzip2_KEY    := bzip2

$(SRC_ROOT)/bzip2: | prepare
	$(call sp_fetch_tar,bzip2)

# https://tukaani.org/xz/#_source_packages # revised: 18 aug 2026
xz_URL    := https://github.com/tukaani-project/xz/releases/download/v5.8.3/xz-5.8.3.tar.xz
xz_SHA256 := fff1ffcf2b0da84d308a14de513a1aa23d4e9aa3464d17e64b9714bfdd0bbfb6
xz_SIG    := .sig
xz_KEY    := xz

$(SRC_ROOT)/xz: | prepare
	$(call sp_fetch_tar,xz)

# https://github.com/facebook/zstd/releases # revised: 18 aug 2026
zstd_URL    := https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
zstd_SHA256 := eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
zstd_SIG    := .sig
zstd_KEY    := zstd

$(SRC_ROOT)/zstd: | prepare
	$(call sp_fetch_tar,zstd)

# https://github.com/libjpeg-turbo/libjpeg-turbo/releases # revised: 18 aug 2026
libjpeg-turbo_URL    := https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.2.0/libjpeg-turbo-3.2.0.tar.gz
libjpeg-turbo_SHA256 := 6f30092cef9fb839779646608f4ee14ae3cbac989c47fa05e841b0841f09878e
libjpeg-turbo_SIG    := .sig
libjpeg-turbo_KEY    := libjpeg-turbo

$(SRC_ROOT)/libjpeg-turbo: | prepare
	$(call sp_fetch_tar,libjpeg-turbo)

# Move to github source releases; sourceforge distribution can block downloads from Russia
# https://github.com/pnggroup/libpng # revised: 18 aug 2026
libpng_REPO       := https://github.com/pnggroup/libpng.git
libpng_TAG        := v1.6.58
libpng_COMMIT     := 3061454d980de7d53608f594194cfac722721d2a
libpng_DEPTH      := 1
libpng_SUBMODULES := 1

$(SRC_ROOT)/libpng: | prepare
	$(call sp_fetch_clone,libpng)

#  Move to Void Linux source archives; sourceforge distribution can block downloads from Russia
# https://sources.voidlinux.org # revised: 18 aug 2026
# Security: staying on the 5.2.x line. Upstream is at 6.1.3, but the 6.x bump is a
#  deliberate API/ABI break (EGifSpew() changed signature, the E_GIF_ERR values were
#  renumbered) and it buys nothing here: 6.1.2 picked up CVE-2026-23868 (patch 0002
#  below), while CVE-2026-26740 is STILL unfixed upstream as of 6.1.3 - checked
#  EGifGCBToSavedExtension(), it has no ByteCount guard - so patch 0001 has to be
#  carried across a 6.x move anyway. Re-evaluate when the 6.x API settles.
# Both CVE-2024-45993 and CVE-2025-31344 are gif2rgb-only; common/gif.mk builds just
#  the library sources, so neither is reachable here.
# Supply chain: giflib publishes no signature anywhere, and this is a redistribution
#  mirror rather than an upstream release host, so the pinned SHA-256 is the only
#  thing tying this tarball to the one that was reviewed.
giflib_URL    := https://sources.voidlinux.org/giflib-5.2.2/giflib-5.2.2.tar.gz
giflib_SHA256 := be7ffbd057cadebe2aa144542fd90c6838c6a083b5e8a9048b8ee3b66b29d5fb

$(SRC_ROOT)/giflib: | prepare
	$(call sp_fetch_tar,giflib)
	$(call sp_patch,giflib,giflib/0001-CVE-2026-26740-egif_lib-GCE-bounds-check.patch)
	$(call sp_patch,giflib,giflib/0002-CVE-2026-23868-gifalloc-avoid-double-free.patch)

# https://storage.googleapis.com/downloads.webmproject.org/releases/webp/index.html # revised: 18 aug 2026
libwebp_URL    := https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz
libwebp_SHA256 := e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564
libwebp_SIG    := .asc
libwebp_KEY    := libwebp

$(SRC_ROOT)/libwebp: | prepare
	$(call sp_fetch_tar,libwebp)

# https://download.osgeo.org/libtiff/?C=M&O=D # revised: 18 aug 2026
# Security: 4.7.2 fixes CVE-2026-12912 (heap overflow in PixarLogDecode) and carries the
#  CVE-2026-4775 (tif_getimage signed-int overflow) fix upstream - the local backport patch
#  is no longer needed (would conflict against 4.7.2).
#  CVE-2025-61143 / CVE-2025-61144 affect only the tiffcrop/tiffdither tools (built with tiff-tools=OFF) - N/A.
tiff_URL    := https://download.osgeo.org/libtiff/tiff-4.7.2.tar.xz
tiff_SHA256 := 4996f0c4f93094719b1ca5c6279b20e588773ba8a247533e486416fb662ddb88
tiff_SIG    := .sig
tiff_KEY    := tiff

$(SRC_ROOT)/tiff: | prepare
	$(call sp_fetch_tar,tiff)

# https://github.com/google/brotli/releases # revised: 18 aug 2026
# TODO: Move to git release
# Supply chain: this is a GitHub-generated tag archive, which upstream neither signs
#  nor publishes a checksum for; the SHA-256 pin is the whole of the verification.
#  Moving to a git clone with a _COMMIT pin would be strictly better - see the TODO.
brotli_URL    := https://github.com/google/brotli/archive/refs/tags/v1.2.0.tar.gz
brotli_SHA256 := 816c96e8e8f193b40151dad7e8ff37b1221d019dbcb9c35cd3fadbfe6477dfec

$(SRC_ROOT)/brotli: | prepare
	$(call sp_fetch_tar,brotli)

# Use Mbed TLS 3.6 until at least 2027
# TODO: Move to git release or exclude - unable to properly verify supply chain
# https://github.com/Mbed-TLS/mbedtls/releases # revised: 18 aug 2026
# Security: 3.6.7 is the last 3.6 LTS patch. CVE-2025-66442 (timing side channel in
#  RSA and CBC/ECB decryption, introduced by LLVM's select-optimize pass) is open
#  against every version through 4.0.0 - there is no upstream fix to pick up. We build
#  with clang at -O3/-O2, where that pass runs. Upstream SECURITY.md declines to work
#  around individual optimizations and only scrutinizes -O2/-Os, so mitigating here
#  would mean -mllvm -disable-select-optimize (or -O2) for this library specifically.
#  Deliberately NOT done yet: it is a build-policy call, not a version bump.
# Supply chain: the release tarball carries no detached signature - this is the
#  "unable to properly verify supply chain" the TODO above refers to. Pinned by
#  SHA-256 only.
mbedtls_URL    := https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2
mbedtls_SHA256 := a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6

$(SRC_ROOT)/mbedtls: | prepare
	$(call sp_fetch_tar,mbedtls)

# https://github.com/ngtcp2/nghttp3/releases # revised: 18 aug 2026
nghttp3_URL    := https://github.com/ngtcp2/nghttp3/releases/download/v1.18.0/nghttp3-1.18.0.tar.xz
nghttp3_SHA256 := aad782c23d3f01bd4bb52c8bac7a553b631ef8115fd1612703df6183449fef19
nghttp3_SIG    := .asc
nghttp3_KEY    := tatsuhiro-t

$(SRC_ROOT)/nghttp3: | prepare
	$(call sp_fetch_tar,nghttp3)

# https://github.com/ngtcp2/ngtcp2/releases # revised: 18 aug 2026
ngtcp2_URL    := https://github.com/ngtcp2/ngtcp2/releases/download/v1.25.0/ngtcp2-1.25.0.tar.xz
ngtcp2_SHA256 := 2a34d2484ba17847a5d11965704e9dd0fac4c6d8efc75ffe1ec7de66d8c6b6fb
ngtcp2_SIG    := .asc
ngtcp2_KEY    := tatsuhiro-t

$(SRC_ROOT)/ngtcp2: | prepare
	$(call sp_fetch_tar,ngtcp2)

# https://curl.se/download.html # revised: 18 aug 2026
curl_URL    := https://curl.se/download/curl-8.21.0.tar.xz
curl_SHA256 := aa1b66a70eace83dc624508745646c08ae561de512ab403adffb93ac87fc72e6
curl_SIG    := .asc
curl_KEY    := curl

$(SRC_ROOT)/curl: | prepare
	$(call sp_fetch_tar,curl)

# https://deac-fra.dl.sourceforge.net/project/freetype/freetype2/2.14.3/freetype-2.14.3.tar.xz # revised: 18 aug 2026
# Security: 2.14.3 is the latest release; backport CVE-2026-50811 (the fix is only
#  on master, no upstream release carries it yet)
# The URL names one SourceForge mirror rather than the redirector, which hands out
#  hosts that are unreachable from some networks. The mirror only serves bytes: the
#  signature is Werner Lemberg's, so a substituted mirror copy fails verification.
freetype_URL    := https://deac-fra.dl.sourceforge.net/project/freetype/freetype2/2.14.3/freetype-2.14.3.tar.xz
freetype_SHA256 := 36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f
freetype_SIG    := .sig
freetype_KEY    := freetype

$(SRC_ROOT)/freetype: | prepare
	$(call sp_fetch_tar,freetype)
	$(call sp_patch,freetype,freetype/0001-CVE-2026-50811-ttgxvar-bound-TT_Get_Var_Design.patch)

# https://github.com/harfbuzz/harfbuzz/releases/ # revised: 18 aug 2026
# TODO: Move to git release
# Supply chain: harfbuzz release assets are unsigned; pinned by SHA-256 only.
harfbuzz_URL    := https://github.com/harfbuzz/harfbuzz/releases/download/14.3.1/harfbuzz-14.3.1.tar.xz
harfbuzz_SHA256 := 9dae9538aae2ffdf70cec31f2c27bf68e2aaeeae3112688467697d5faf6194f7

$(SRC_ROOT)/harfbuzz: | prepare
	$(call sp_fetch_tar,harfbuzz)

# https://github.com/Tehreer/SheenBidi # revised: 18 aug 2026
# Pinned to release tag v3.0.0 (Unicode 17.0); Apache-2.0, used bundled as the
# Unicode Bidirectional Algorithm resolver.
sheenbidi_REPO   := https://github.com/Tehreer/SheenBidi.git
sheenbidi_TAG    := v3.0.0
sheenbidi_COMMIT := cfe430e7375a7845b679adae9d51dac6deaa8858
sheenbidi_DEPTH  := 1

$(SRC_ROOT)/sheenbidi: | prepare
	$(call sp_fetch_clone,sheenbidi)

# https://www.sqlite.org/download.html # revised: 18 aug 2026
# Weak supply chain validation: only sha3 provided upstream, so the SHA-256 pinned
#  here is ours - computed once from a reviewed download and kept in git.
# Security: CVE-2026-50812 / CVE-2026-50813 are both in the Session Extension; common/sqlite.mk
#  compiles the amalgamation with no -D at all, so SQLITE_ENABLE_SESSION is off - N/A.
# The versioned directory inside the zip (sqlite-amalgamation-NNNNNNN) is stripped by
#  the fetcher, so the release number appears in one place only - the URL.
sqlite_URL    := https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
sqlite_SHA256 := 1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d

$(SRC_ROOT)/sqlite: | prepare
	$(call sp_fetch_zip,sqlite)

# Use 3.5 LTS until new LTS
# https://openssl-library.org/source/index.html # revised: 18 aug 2026
# Security: 3.5.7 is the last 3.5 LTS patch. CVE-2026-14456 (unbounded memory growth
#  in the QUIC server listener's incoming-channel queue) is still open - the fix is
#  slated for 3.5.8 / 3.6.4 / 4.0.2 and none of those are released yet. It needs an
#  OpenSSL QUIC *server* (Listener SSL object); we only ever act as a client, so it is
#  not reachable here. Bump to 3.5.8 as soon as it ships.
openssl_URL    := https://github.com/openssl/openssl/releases/download/openssl-3.5.7/openssl-3.5.7.tar.gz
openssl_SHA256 := a8c0d28a529ca480f9f36cf5792e2cd21984552a3c8e4aa11a24aa31aeac98e8
openssl_SIG    := .asc
openssl_KEY    := openssl

$(SRC_ROOT)/openssl: | prepare
	$(call sp_fetch_tar,openssl)
	$(call rule_cp,replacements/openssl/async_posix.c,$(SRC_ROOT)/openssl/crypto/async/arch/async_posix.c)
	$(call rule_cp,replacements/openssl/49-xwin-clang.conf,$(SRC_ROOT)/openssl/Configurations)
	$(call rule_cp,replacements/openssl/50-wasm-sprt-clang.conf,$(SRC_ROOT)/openssl/Configurations)

# https://github.com/gost-engine/engine # revised: 18 aug 2026
openssl-gost-engine_REPO       := https://github.com/gost-engine/engine.git
openssl-gost-engine_TAG        := v3.0.3
openssl-gost-engine_COMMIT     := e0a500ab877ba72cb14026a24d462dd923b90ced
openssl-gost-engine_DEPTH      := 1
openssl-gost-engine_SUBMODULES := 1

$(SRC_ROOT)/openssl-gost-engine: | prepare
	$(call sp_fetch_clone,openssl-gost-engine)

# # https://github.com/bytecodealliance/wasm-micro-runtime # revised: 18 aug 2026
wasm-micro-runtime_REPO       := https://github.com/bytecodealliance/wasm-micro-runtime.git
wasm-micro-runtime_TAG        := WAMR-2.4.5
wasm-micro-runtime_COMMIT     := 25bd7eb63e828e4bd242cc9b38d260b4b31c6605
wasm-micro-runtime_DEPTH      := 1
wasm-micro-runtime_SUBMODULES := 1

$(SRC_ROOT)/wasm-micro-runtime: | prepare
	$(call sp_fetch_clone,wasm-micro-runtime)

# The Khronos repositories below are all cut from the same SDK tag. Khronos does not
# sign the tags, so each one carries the commit that tag pointed at when the SDK was
# adopted; `make src-pins` re-resolves them all when VULKAN_SDK_VER moves.

# https://github.com/KhronosGroup/Vulkan-Headers # revised: 18 aug 2026
vulkan-headers_REPO       := https://github.com/KhronosGroup/Vulkan-Headers.git
vulkan-headers_TAG        := vulkan-sdk-$(VULKAN_SDK_VER)
vulkan-headers_COMMIT     := e3b1eec08173d6b825cd3ac88c885a63b621504a
vulkan-headers_DEPTH      := 1
vulkan-headers_SUBMODULES := 1

$(SRC_ROOT)/vulkan-headers: | prepare
	$(call sp_fetch_clone,vulkan-headers)

# https://github.com/KhronosGroup/SPIRV-Headers # revised: 18 aug 2026
spirv-headers_REPO       := https://github.com/KhronosGroup/SPIRV-Headers.git
spirv-headers_TAG        := vulkan-sdk-$(VULKAN_SDK_VER)
spirv-headers_COMMIT     := 29981f65241605e08b0ede4cfeb999fe3b723c6a
spirv-headers_DEPTH      := 1
spirv-headers_SUBMODULES := 1

$(SRC_ROOT)/spirv-headers: | prepare
	$(call sp_fetch_clone,spirv-headers)

# https://github.com/KhronosGroup/glslang # revised: 18 aug 2026
glslang_REPO       := https://github.com/KhronosGroup/glslang.git
glslang_TAG        := vulkan-sdk-$(VULKAN_SDK_VER)
glslang_COMMIT     := 168d452a4f460d24b588fed08477a81c44ee27a1
glslang_DEPTH      := 1
glslang_SUBMODULES := 1

$(SRC_ROOT)/glslang: | prepare
	$(call sp_fetch_clone,glslang)

# https://github.com/KhronosGroup/SPIRV-Tools # revised: 18 aug 2026
spirv-tools_REPO       := https://github.com/KhronosGroup/SPIRV-Tools.git
spirv-tools_TAG        := vulkan-sdk-$(VULKAN_SDK_VER)
spirv-tools_COMMIT     := 9a49b0883b9b635689a85b5647dbfcb223268151
spirv-tools_DEPTH      := 1
spirv-tools_SUBMODULES := 1

$(SRC_ROOT)/spirv-tools: | prepare
	$(call sp_fetch_clone,spirv-tools)

# https://github.com/KhronosGroup/Vulkan-Loader # revised: 18 aug 2026
vulkan-loader_REPO       := https://github.com/KhronosGroup/Vulkan-Loader.git
vulkan-loader_TAG        := vulkan-sdk-$(VULKAN_SDK_VER)
vulkan-loader_COMMIT     := 5f157b62e333c63260d05d81bf66faa216ab0fb8
vulkan-loader_DEPTH      := 1
vulkan-loader_SUBMODULES := 1

$(SRC_ROOT)/vulkan-loader: | prepare
	$(call sp_fetch_clone,vulkan-loader)

# https://github.com/KhronosGroup/Vulkan-ValidationLayers # revised: 18 aug 2026
vulkan-validationlayers_REPO       := https://github.com/KhronosGroup/Vulkan-ValidationLayers.git
vulkan-validationlayers_TAG        := vulkan-sdk-$(VULKAN_SDK_VER)
vulkan-validationlayers_COMMIT     := f4874eee15c78d7bdb2b7e60659d539f14741500
vulkan-validationlayers_DEPTH      := 1
vulkan-validationlayers_SUBMODULES := 1

$(SRC_ROOT)/vulkan-validationlayers: | prepare
	$(call sp_fetch_clone,vulkan-validationlayers)

# https://github.com/KhronosGroup/Vulkan-Utility-Libraries # revised: 18 aug 2026
vulkan-utility_REPO       := https://github.com/KhronosGroup/Vulkan-Utility-Libraries.git
vulkan-utility_TAG        := vulkan-sdk-$(VULKAN_SDK_VER)
vulkan-utility_COMMIT     := e9585c3e3d41ab608ee3b098ce4721d357308fc8
vulkan-utility_DEPTH      := 1
vulkan-utility_SUBMODULES := 1

$(SRC_ROOT)/vulkan-utility: | prepare
	$(call sp_fetch_clone,vulkan-utility)

# https://github.com/KhronosGroup/Vulkan-Tools # revised: 18 aug 2026
# Используется только target-xenolithos и только ради vulkaninfo — девайсового
# пробника «нашёл ли лоадер ICD и перечисляет ли драйвер физическое устройство».
vulkan-tools_REPO       := https://github.com/KhronosGroup/Vulkan-Tools.git
vulkan-tools_TAG        := vulkan-sdk-$(VULKAN_SDK_VER)
vulkan-tools_COMMIT     := 286299bb6b732e4b22771cfb9d7d421542d40501
vulkan-tools_DEPTH      := 1
vulkan-tools_SUBMODULES := 1

$(SRC_ROOT)/vulkan-tools: | prepare
	$(call sp_fetch_clone,vulkan-tools)

# https://github.com/KhronosGroup/MoltenVK/releases # revised: 18 aug 2026
# 1.4.2 needs IOSurfaceGetID(), which the +open sysroot did not declare; the getter was
#  added to target-apple/open/sysroot/.../IOSurface.framework/Headers/IOSurfaceRef.h.
# Supply chain: a GitHub-generated tag archive, unsigned; pinned by SHA-256 only.
moltenvk_URL    := https://github.com/KhronosGroup/MoltenVK/archive/refs/tags/v1.4.2.tar.gz
moltenvk_SHA256 := 6864db532f1dbbdb621a8d0ec13f24edae318fd9269dd3dd0cdff791334bb1cb

$(SRC_ROOT)/moltenvk: | prepare
	$(call sp_fetch_tar,moltenvk)

# https://github.com/unicode-org/icu/releases # revised: 18 aug 2026
# Kept as the Unicode reference, NOT as a runtime dependency. Nothing links ICU any
#  more - the runtime does its own case mapping, collation and IDN - but this
#  checkout is what the generators and conformance suites read: the UCD under
#  source/data/unidata, the UCA and UTS-46 test files under source/test/testdata,
#  and the collation data the tables in runtime/src/unicode are generated from
#  (see tests/runtime/tools/gen_*.py and docs/design/unicode-and-idn.adoc).
#  So it stays downloaded, and the version here is the Unicode version we are
#  claiming to support: bumping it is a deliberate Unicode-support decision, and
#  the generated tables and the conformance suites have to be regenerated with it.
#  It is no longer built for any target - see target-linux/icu.mk for the
#  on-demand build if a real libicuuc is ever needed for comparison.
icu4c_URL    := https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-sources.tgz
icu4c_SHA256 := 3a2e7a47604ba702f345878308e6fefeca612ee895cf4a5f222e7955fabfe0c0
icu4c_SIG    := .asc
icu4c_KEY    := icu4c

$(SRC_ROOT)/icu4c: | prepare
	$(call sp_fetch_tar,icu4c)

# https://github.com/libffi/libffi/releases # revised: 18 aug 2026
# TODO: move to git releases
# Supply chain: libffi release assets are unsigned; pinned by SHA-256 only.
ffi_URL    := https://github.com/libffi/libffi/releases/download/v3.8.0/libffi-3.8.0.tar.gz
ffi_SHA256 := 7da3e2d9a171eb0a038f592ecad3ff2bb2550f3496d87b3b29ad0cf4430c0db4

$(SRC_ROOT)/ffi: | prepare
	$(call sp_fetch_tar,ffi)

# https://github.com/libexpat/libexpat/releases # revised: 18 aug 2026
# Security: 2.8.3 fixes CVE-2026-72522 (OOB read + infinite loop in *_toUtf16). That one
#  only bites builds with 16-bit character support; ours is the default char/UTF-8 build
#  (no XML_UNICODE), so we were not exposed - but 2.8.3 also fixes a 2.8.2 regression on
#  2+ GiB documents. Note upstream still flags unfixed issues, see libexpat issue #1160.
expat_URL    := https://github.com/libexpat/libexpat/releases/download/R_2_8_3/expat-2.8.3.tar.xz
expat_SHA256 := f6256df90c906773d344da084402b7d3e4f22ed41b1a59c989098a83d3ea0c85
expat_SIG    := .asc
expat_KEY    := expat

$(SRC_ROOT)/expat: | prepare
	$(call sp_fetch_tar,expat)

# Use upstream - releases bound with GCC
# https://github.com/ianlancetaylor/libbacktrace.git # revised: 18 aug 2026
# Moved up from 549b81b4 (3 commits): two fixes in the built-in zstd decoder for
#  ELFCOMPRESS_ZSTD .debug_* sections (a compressed block that does not set the
#  single-segment flag, and the missing table field on an RLE sequence), plus DWARF
#  discriminator support behind a new moredata flag.
#  That last one redefines the second parameter of backtrace_create_state() from
#  "threaded" to a flag word - bit 0 is threaded, bit 1 is moredata, anything above
#  0x3 is now rejected. backtrace.h still spells the parameter "threaded" and our only
#  caller (runtime/libc_wrapper/runtime/SPRuntimeBacktrace.cpp) passes 1, i.e. bit 0,
#  so this stays source- and ABI-compatible for us.
libbacktrace_REPO   := https://github.com/ianlancetaylor/libbacktrace.git
libbacktrace_COMMIT := 6f8310e238fc3ce68f42f391cbe93fd156bb2c23

$(SRC_ROOT)/libbacktrace: | prepare
	$(call sp_fetch_clone,libbacktrace)

# Use upstream - releases are too old
# https://github.com/simd-everywhere/simde.git # revised: 18 aug 2026
simde_REPO   := https://github.com/simd-everywhere/simde.git
simde_COMMIT := f3e8262173b7089db9a9d57a9ecef8dd07ad9c97

$(SRC_ROOT)/simde: | prepare
	$(call sp_fetch_clone,simde)

# The version itself lives in common/utils/llvm-version.mk (SP_LLVM_TAG / SP_LLVM_V);
# the patch directories under replacements/llvm are named after SP_LLVM_V.
# Next version will be 23.1.X when 24.1.0 will be released
# llvm-project_COMMIT belongs next to the tag rather than in llvm-version.mk: it is
#  not a version, it is the assertion that llvmorg-$(SP_LLVM_V) still points where it
#  did when the patch series below was rebased onto it. Re-resolve it with
#  `make src-pins` whenever SP_LLVM_PATCH moves.
llvm-project_REPO       := https://github.com/llvm/llvm-project.git
llvm-project_TAG        := $(SP_LLVM_TAG)
llvm-project_COMMIT     := ca7933e47d3a3451d81e72ac174dcb5aa28b59d1
llvm-project_DEPTH      := 1
llvm-project_SUBMODULES := 1

$(SRC_ROOT)/llvm-project: | prepare
	$(call sp_fetch_clone,llvm-project)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-lldb-wine/0001-Fix-incorrect-L1-inferior-memory-cache-flushing.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-lldb-wine/0002-lldb-Add-DYLD-plugin-for-debugging-Wine.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-lldb-wine/0003-lldb-Fix-Wine-preloader-name-in-POSIX-Wine-DYLD.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-lldb-wine/0004-lldb-Adapt-POSIX-Wine-DYLD-to-the-22.1-CreateBreakpoint-API.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-noulock/0001-replaced-__ulock-with-os_sync_wait_on_address.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-libunwind-wasm/0001-libunwind-tolerate-wasm-target-in-assembly.h.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-no-delayload/0001-Support-disable-shell32-ole32-delay-load-no-delayimp.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-sprt-windows/0001-lldb-Defer-to-sprt-libc-in-PosixApi.h.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-sprt-windows/0002-lldb-Use-real-terminal-interface-on-sprt-libc.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-sprt-windows/0003-compiler-rt-Build-ORC-runtime-as-C-20.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-sprt-windows/0004-lit-Make-the-suites-usable-when-cross-testing-under-wine.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-sprt-windows/0005-clang-Do-not-require-clang-repl-for-the-test-suites.patch)
	$(call sp_patch,llvm-project,llvm/$(SP_LLVM_V)-sprt-windows/0006-compiler-rt-Include-the-POSIX-locking-headers-on-a-Windows-target.patch)

# https://download.gnome.org/sources/libxml2  # revised: 18 aug 2026
# Supply chain: GNOME publishes a .sha256sum next to the tarball but no signature;
#  a checksum served by the same host it guards adds nothing, so the pin below - which
#  lives in our git history - is what does the work.
libxml2_URL    := https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.3.tar.xz
libxml2_SHA256 := 78262a6e7ac170d6528ebfe2efccdf220191a5af6a6cd61ea4a9a9a5042c7a07

$(SRC_ROOT)/libxml2: | prepare
	$(call sp_fetch_tar,libxml2)

# https://wayland.freedesktop.org/releases.html # revised: 18 aug 2026
# Pinned to whatever wayland-scanner the BUILD HOST ships, not to the newest release.
#  target-linux/wayland.mk cross-builds with -Dscanner=false and drives protocol
#  generation through the system wayland-scanner, and src/meson.build demands an exact
#  version match. 1.26.0 was tried and fails at meson setup on a host with 1.25.0:
#  "Invalid version, need 'wayland-scanner' ['1.26.0'] found '1.25.0'".
#  Moving past 1.25.0 means building wayland-scanner for the build machine first.
#  No security reason to hurry: 1.25.0 has no known CVE.
# Supply chain: the GitLab release downloads are unsigned; pinned by SHA-256 only.
wayland_URL    := https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.25.0/downloads/wayland-1.25.0.tar.xz
wayland_SHA256 := c065f040afdff3177680600f249727e41a1afc22fccf27222f15f5306faa1f03

$(SRC_ROOT)/wayland: | prepare
	$(call sp_fetch_tar,wayland)

# https://wayland.freedesktop.org/releases.html # revised: 18 aug 2026
# Supply chain: as wayland above - unsigned GitLab release, SHA-256 pin only.
wayland-protocols_URL    := https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/1.49/downloads/wayland-protocols-1.49.tar.xz
wayland-protocols_SHA256 := ec4c8f74942d6dff7ace8b4ce4764f0ef9ff618a935d974ea77edee2ad240b14

$(SRC_ROOT)/wayland-protocols: | prepare
	$(call sp_fetch_tar,wayland-protocols)

# https://github.com/KDE/plasma-wayland-protocols # revised: 18 aug 2026
plasma-wayland-protocols_REPO   := https://github.com/KDE/plasma-wayland-protocols.git
plasma-wayland-protocols_TAG    := v1.21.0
plasma-wayland-protocols_COMMIT := 4c015e90ae6c88f2ffa766e899387ef431eade49
plasma-wayland-protocols_DEPTH  := 1

$(SRC_ROOT)/plasma-wayland-protocols: | prepare
	$(call sp_fetch_clone,plasma-wayland-protocols)

# Keep the version within 2.4.x: patch_ver feeds the SONAME version
# (libdrm.so.2.<minor>.0), so moving to 2.5 would roll it backwards — see the
# note at the top of libdrm's own meson.build.
# https://dri.freedesktop.org/libdrm/ # revised: 18 aug 2026
libdrm_URL    := https://dri.freedesktop.org/libdrm/libdrm-2.4.134.tar.xz
libdrm_SHA256 := ac5e74d157830eb8bee44c6a6bf3ad49774ef0dd2a72bdad74a8f20308b52a95
libdrm_SIG    := .sig
libdrm_KEY    := libdrm

$(SRC_ROOT)/libdrm: | prepare
	$(call sp_fetch_tar,libdrm)


#
# CA bundle
#
# Inject Russia Ministry of Digital Development certificates
# https://curl.se/ca # revised: 18 aug 2026
# https://www.gosuslugi.ru/crt # revised: 18 aug 2026
#
# curl.se serves a dated, immutable file, so its pin is stable until CERT_NAME
# moves. The gu-st.ru certificates sit at unversioned URLs and are replaced in
# place when they are reissued; pinning them means a reissue shows up as a loud
# SHA-256 mismatch here rather than as a silently changed trust store. When that
# happens, look at the new certificate before updating the pin.

CERT_NAME := cacert-2026-08-13.pem

cacert_URL    := https://curl.se/ca/$(CERT_NAME)
cacert_SHA256 := f66dff1bdf8f96060b8177976f8b7d9254bc89bc4db933d769f7384d28480bc9

ru-ca-root_URL    := https://gu-st.ru/content/lending/russian_trusted_root_ca_pem.crt
ru-ca-root_SHA256 := 936a43fea6e8e525bcc0f81acd9c3d21b4fc4b9b68acea7906d698005afc6504

ru-ca-sub_URL    := https://gu-st.ru/content/lending/russian_trusted_sub_ca_pem.crt
ru-ca-sub_SHA256 := f0ae589f36774f29ef3648f7984b08d42fcce6f1ffeeb6236d773daeb2744ea6

ru-ca-sub-2024_URL    := https://gu-st.ru/content/lending/russian_trusted_sub_ca_2024_pem.crt
ru-ca-sub-2024_SHA256 := 6f9d829c8e6712444fce3624658d8788672849c5d5b7b53fd9cf7e83eac4193e

CERT_TMP := $(TMP_DIR)/cacert

ifeq ($(findstring Windows,$(OS)),Windows)
replacements/curl/cacert.pem: $(LIBS_MAKE_FILE) | prepare
	$(MKDIR) replacements/curl | Out-Null
	$(call sp_fetch_file,cacert,$(CERT_TMP)/cacert.pem)
	$(call sp_fetch_file,ru-ca-root,$(CERT_TMP)/root.crt)
	$(call sp_fetch_file,ru-ca-sub,$(CERT_TMP)/sub.crt)
	$(call sp_fetch_file,ru-ca-sub-2024,$(CERT_TMP)/sub2024.crt)
	Get-Content "$(CERT_TMP)/cacert.pem" | Set-Content "$@";
	Add-Content "$@" "`nhttps://www.gosuslugi.ru/crt - Root`n====================";
	Get-Content "$(CERT_TMP)/root.crt" | Add-Content "$@";
	Add-Content "$@" "`nhttps://www.gosuslugi.ru/crt - Sub`n====================";
	Get-Content "$(CERT_TMP)/sub.crt" | Add-Content "$@";
	Add-Content "$@" "`nhttps://www.gosuslugi.ru/crt - Sub 2024`n====================";
	Get-Content "$(CERT_TMP)/sub2024.crt" | Add-Content "$@";
	$(RM) $(CERT_TMP)
else
replacements/curl/cacert.pem: $(LIBS_MAKE_FILE) | prepare
	@$(MKDIR) replacements/curl
	$(call sp_fetch_file,cacert,$(CERT_TMP)/cacert.pem)
	$(call sp_fetch_file,ru-ca-root,$(CERT_TMP)/root.crt)
	$(call sp_fetch_file,ru-ca-sub,$(CERT_TMP)/sub.crt)
	$(call sp_fetch_file,ru-ca-sub-2024,$(CERT_TMP)/sub2024.crt)
	printf "\nhttps://www.gosuslugi.ru/crt - Root\n====================\n" > $(CERT_TMP)/root.hdr
	printf "\nhttps://www.gosuslugi.ru/crt - Sub\n====================\n" > $(CERT_TMP)/sub.hdr
	printf "\nhttps://www.gosuslugi.ru/crt - Sub 2024\n====================\n" > $(CERT_TMP)/sub2024.hdr
	cat $(CERT_TMP)/cacert.pem \
		$(CERT_TMP)/root.hdr $(CERT_TMP)/root.crt \
		$(CERT_TMP)/sub.hdr $(CERT_TMP)/sub.crt \
		$(CERT_TMP)/sub2024.hdr $(CERT_TMP)/sub2024.crt > $@
	@rm -rf $(CERT_TMP)
endif


#
# Windows SDK splatter, used by the target-windows sysroots only
#
# https://github.com/Jake-Shadle/xwin/releases
# Supply chain: a prebuilt binary. Upstream publishes a .sha256 next to it, which
#  only guards against corruption; the pin below is what makes this reproducible.
xwin_URL    := https://github.com/Jake-Shadle/xwin/releases/download/0.10.0/xwin-0.10.0-x86_64-unknown-linux-musl.tar.gz
xwin_SHA256 := d870eb4b2f390878af6da1ccd3cf321d22fcb72720984853b4be732ae597fc88

$(SRC_ROOT)/xwin: | prepare
	$(call sp_fetch_tar,xwin)

$(SRC_ROOT)/xwin/splat: $(SRC_ROOT)/xwin
	cd $(SRC_ROOT)/xwin; ./xwin --accept-license  --arch aarch64 --arch x86_64 splat
	cd $(SRC_ROOT)/xwin; ln -s .xwin-cache/splat splat


#
# Declaration sanity checks
#
# A dependency is verified by variables whose names are built from its own name,
# so a typo - zstd_SHA26, libdrm_SIG with no libdrm_KEY - would not be a syntax
# error, it would be a dependency that quietly stops being checked. These run at
# parse time, on every invocation, and turn that class of mistake into a message
# instead of a silently weakened build.

# Fetched by name but not part of LIBS: the CA bundle inputs and the Windows SDK tool.
SRC_EXTRA_NAMES := cacert ru-ca-root ru-ca-sub ru-ca-sub-2024 xwin

SRC_ALL_NAMES := $(LIBS) $(SRC_EXTRA_NAMES)

$(foreach l,$(SRC_ALL_NAMES),$(if $($(l)_URL)$($(l)_REPO),,\
	$(error src.mk: $(l) declares neither $(l)_URL nor $(l)_REPO)))

$(foreach l,$(SRC_ALL_NAMES),$(if $($(l)_REPO),,$(if $($(l)_SHA256),,\
	$(error src.mk: $(l) is downloaded but has no $(l)_SHA256 pin))))

$(foreach l,$(SRC_ALL_NAMES),$(if $($(l)_SIG),$(if $($(l)_KEY),,\
	$(error src.mk: $(l) declares $(l)_SIG but no $(l)_KEY to verify it against))))

$(foreach l,$(SRC_ALL_NAMES),$(if $($(l)_KEY),$(if $(wildcard $(SP_KEYS_DIR)/$($(l)_KEY).asc),,\
	$(error src.mk: $(l) names key '$($(l)_KEY)' but $(SP_KEYS_DIR)/$($(l)_KEY).asc does not exist))))


#
# Maintenance helpers
#

# Everything that is fetched with git, so the two helpers below do not have to
# repeat the list. Both helpers assume a POSIX host - they are maintenance tools,
# not part of a build.
SRC_GIT_NAMES := $(sort $(foreach l,$(LIBS),$(if $($(l)_REPO),$(l))))

# An annotated tag resolves to two refs: the tag object and, under "^{}", the
# commit it points at. Prefer the peeled one; git ls-remote lists them in ref
# order rather than in the order the patterns were given, so picking the first
# line would return the tag object for some repositories and the commit for others.
sp_ls_remote = git ls-remote $($(1)_REPO) \
	$(if $($(1)_TAG),"refs/tags/$($(1)_TAG)^{}" "refs/tags/$($(1)_TAG)" "refs/heads/$($(1)_TAG)",HEAD) \
	| awk -F'\t' '$$2 ~ /\^\{\}$$/ {peeled=$$1} $$2 !~ /\^\{\}$$/ && !plain {plain=$$1} END {print (peeled ? peeled : plain)}'

# Re-resolve every pinned tag to the commit it points at today. Run this after
# changing a _TAG (or VULKAN_SDK_VER, or SP_LLVM_PATCH) and paste the output into
# the _COMMIT lines - and if a line comes back changed for a tag you did NOT touch,
# upstream re-tagged and that is worth understanding before you update the pin.
#
# The few sources that have a _COMMIT but no _TAG (libbacktrace, simde) are
# reported at their remote's HEAD, since there is no tag to resolve: for those the
# output is "what upstream is at now", not "what the pin should be".
src-pins:
	@$(foreach l,$(SRC_GIT_NAMES), \
		printf '%-32s := %s\n' '$(l)_COMMIT' "`$(call sp_ls_remote,$(l))`";)

# Show which key each signed dependency is pinned to, and the fingerprint of the
# key file actually sitting in keys/. Cross-check a fingerprint against the
# project's own site before trusting a new or rotated key - keys/README.adoc
# records where each of the current ones was confirmed.
src-keys:
	@$(foreach l,$(LIBS),$(if $($(l)_KEY),echo "$(l): keys/$($(l)_KEY).asc"; \
		gpg --batch --with-colons --import-options show-only --import "$(SP_KEYS_DIR)/$($(l)_KEY).asc" \
		2>/dev/null | awk -F: '/^fpr/{print "    "$$10; exit}';))

.PHONY: src-pins src-keys
