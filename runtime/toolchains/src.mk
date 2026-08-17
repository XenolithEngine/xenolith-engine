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

ifeq ($(findstring Windows,$(OS)),Windows)
SHELL = powershell.exe
endif

include $(LIBS_MAKE_ROOT)common/utils/llvm-version.mk

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
	libuidna \
	mbedtls \
	nghttp3 \
	ngtcp2 \
	libzip \
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

TAR_XF = tar -xf

VULKAN_SDK_VER := 1.4.350.0

ifeq ($(findstring Windows,$(OS)),Windows)

unpack_tar = $(MKDIR) $(SRC_ROOT) | Out-Null; $(MKDIR) $(TMP_DIR)/$(firstword $(2)) | Out-Null; \
	cd $(TMP_DIR); \
	Invoke-WebRequest -Uri "$(1)" -OutFile "$(notdir $(1))"; \
	$(TAR_XF) $(notdir $(1)) --strip-components=1 -C $(firstword $(2)); \
	Remove-Item -Path "$(SRC_ROOT)/$(firstword $(2))" -Recurse -Force -ErrorAction SilentlyContinue; \
	Move-Item -Path $(firstword $(2)) -Destination $(SRC_ROOT)/$(firstword $(2)); \
	(Get-Item "$(SRC_ROOT)/$(firstword $(2))").LastWriteTime = $$(Get-Date); \

else

get_tar_top_dir = `tar -tf $(1)  | head -1 | cut -f1 -d"/"`

unpack_tar = $(MKDIR) $(SRC_ROOT); $(MKDIR) $(TMP_DIR); \
	cd $(TMP_DIR); \
	$(WGET)  -O $(notdir $(1)) $(1); \
	$(MKDIR) $(firstword $(2)); \
	$(TAR_XF) $(notdir $(1)) --strip-components=1 -C $(firstword $(2)); \
	rm -rf $(SRC_ROOT)/$(firstword $(2)); \
	mv -f $(firstword $(2)) $(SRC_ROOT)/$(firstword $(2)); \
	touch $(SRC_ROOT)/$(firstword $(2)); \
	rm $(notdir $(1))

endif


# https://www.zlib.net/ # revised: 2 jun 2026
$(SRC_ROOT)/zlib: | prepare
	$(call unpack_tar, https://www.zlib.net/zlib-1.3.2.tar.gz, zlib)

# https://sourceware.org/bzip2/downloads.html # revised: 2 jun 2026
$(SRC_ROOT)/bzip2: | prepare
	$(call unpack_tar, https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz, bzip2)

# https://tukaani.org/xz/#_source_packages # revised: 2 jun 2026
$(SRC_ROOT)/xz: | prepare
	$(call unpack_tar, https://github.com/tukaani-project/xz/releases/download/v5.8.3/xz-5.8.3.tar.xz, xz)

# https://github.com/facebook/zstd/releases # revised: 2 jun 2026
$(SRC_ROOT)/zstd: | prepare
	$(call unpack_tar, https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz, zstd)

# https://github.com/libjpeg-turbo/libjpeg-turbo/releases # revised: 14 jul 2026
$(SRC_ROOT)/libjpeg-turbo: | prepare
	$(call unpack_tar, https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.2.0/libjpeg-turbo-3.2.0.tar.gz, libjpeg-turbo)

# Move to github source releases; sourceforge distribution can block downloads from Russia
# https://github.com/pnggroup/libpng # revised: 2 jun 2026
$(SRC_ROOT)/libpng: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libpng)
	cd $(SRC_ROOT); git clone  --recurse-submodules  --branch v1.6.58 https://github.com/pnggroup/libpng.git --depth 1 libpng

#  Move to Void Linux source archives; sourceforge distribution can block downloads from Russia
# https://sources.voidlinux.org # revised: 23 jun 2026
# Security: 5.2.2 is the latest release; backport CVE-2026-26740 + CVE-2026-23868
#  (no upstream release carries the fixes yet)
$(SRC_ROOT)/giflib: | prepare
	$(call unpack_tar, https://sources.voidlinux.org/giflib-5.2.2/giflib-5.2.2.tar.gz, giflib)
	cd $(SRC_ROOT)/giflib; git apply -p1 ../../replacements/giflib/0001-CVE-2026-26740-egif_lib-GCE-bounds-check.patch
	cd $(SRC_ROOT)/giflib; git apply -p1 ../../replacements/giflib/0002-CVE-2026-23868-gifalloc-avoid-double-free.patch

# https://storage.googleapis.com/downloads.webmproject.org/releases/webp/index.html # revised: 2 jun 2026
$(SRC_ROOT)/libwebp: | prepare
	$(call unpack_tar, https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz, libwebp)

# https://download.osgeo.org/libtiff/?C=M&O=D # revised: 14 jul 2026
# Security: 4.7.2 fixes CVE-2026-12912 (heap overflow in PixarLogDecode) and carries the
#  CVE-2026-4775 (tif_getimage signed-int overflow) fix upstream - the local backport patch
#  is no longer needed (would conflict against 4.7.2).
#  CVE-2025-61143 / CVE-2025-61144 affect only the tiffcrop/tiffdither tools (built with tiff-tools=OFF) - N/A.
$(SRC_ROOT)/tiff: | prepare
	$(call unpack_tar, https://download.osgeo.org/libtiff/tiff-4.7.2.tar.xz, tiff)

# https://github.com/google/brotli/releases # revised: 2 jun 2026
# TODO: Move to git release
$(SRC_ROOT)/brotli: | prepare
	$(call unpack_tar, https://github.com/google/brotli/archive/refs/tags/v1.2.0.tar.gz, brotli)

# Use Mbed TLS 3.6 until at least 2027
# TODO: Move to git release or exclude - unable to properly verify supply chain
# https://github.com/Mbed-TLS/mbedtls/releases # revised: 14 jul 2026
$(SRC_ROOT)/mbedtls: | prepare
	$(call unpack_tar, https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2, mbedtls)

# https://github.com/ngtcp2/nghttp3/releases # revised: 14 jul 2026
$(SRC_ROOT)/nghttp3: | prepare
	$(call unpack_tar, https://github.com/ngtcp2/nghttp3/releases/download/v1.17.0/nghttp3-1.17.0.tar.xz, nghttp3)

# https://github.com/ngtcp2/ngtcp2/releases # revised: 12 jul 2026
$(SRC_ROOT)/ngtcp2: | prepare
	$(call unpack_tar, https://github.com/ngtcp2/ngtcp2/releases/download/v1.24.0/ngtcp2-1.24.0.tar.xz, ngtcp2)

# https://curl.se/download.html # revised: 14 jul 2026
$(SRC_ROOT)/curl: | prepare
	$(call unpack_tar, https://curl.se/download/curl-8.21.0.tar.xz, curl)

# https://deac-fra.dl.sourceforge.net/project/freetype/freetype2/2.14.3/freetype-2.14.3.tar.xz # revised: 5 aug 2026
$(SRC_ROOT)/freetype: | prepare
	$(call unpack_tar, https://deac-fra.dl.sourceforge.net/project/freetype/freetype2/2.14.3/freetype-2.14.3.tar.xz, freetype)

# https://github.com/harfbuzz/harfbuzz/releases/ # revised: 23 jun 2026
# TODO: Move to git release
$(SRC_ROOT)/harfbuzz: | prepare
	$(call unpack_tar, https://github.com/harfbuzz/harfbuzz/releases/download/14.2.1/harfbuzz-14.2.1.tar.xz, harfbuzz)

# https://github.com/Tehreer/SheenBidi # revised: 25 jun 2026
# Pinned to release tag v3.0.0 (Unicode 17.0); Apache-2.0, used bundled as the
# Unicode Bidirectional Algorithm resolver.
$(SRC_ROOT)/sheenbidi: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/sheenbidi)
	cd $(SRC_ROOT); git clone --branch v3.0.0 --depth 1 https://github.com/Tehreer/SheenBidi.git sheenbidi


# https://www.sqlite.org/download.html # revised: 14 jul 2026
# Weak supply chain validation: only sha3 provided
SQLITE_URL := https://www.sqlite.org/2026/sqlite-amalgamation-3530300.zip
ifeq ($(findstring Windows,$(OS)),Windows)
$(SRC_ROOT)/sqlite: | prepare
	@$(MKDIR) $(SRC_ROOT); $(MKDIR) $(TMP_DIR)
	cd $(TMP_DIR); Invoke-WebRequest -Uri "$(SQLITE_URL)" -OutFile "sqlite-amalgamation.zip";
	cd $(TMP_DIR); Expand-Archive -Path sqlite-amalgamation.zip -DestinationPath .
	$(RM) $(TMP_DIR)/sqlite-amalgamation.zip
	powershell Move-Item -Path $(TMP_DIR)/sqlite-amalgamation-3530300  -Destination $(SRC_ROOT)/sqlite
else
$(SRC_ROOT)/sqlite: | prepare
	@$(MKDIR) $(SRC_ROOT); $(MKDIR) $(TMP_DIR)
	cd $(TMP_DIR); $(WGET) $(SQLITE_URL) -O sqlite-amalgamation.zip
	cd $(TMP_DIR); unzip sqlite-amalgamation.zip -d .
	rm $(TMP_DIR)/sqlite-amalgamation.zip
	mv -f $(TMP_DIR)/sqlite-amalgamation-3530300 $(SRC_ROOT)/sqlite
endif


# https://github.com/SBKarr/libuidna # revised: 2 jun 2026
# Not used in actual source code - no supply chain validation required
$(SRC_ROOT)/libuidna: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libuidna)
	cd $(SRC_ROOT); git clone https://github.com/SBKarr/libuidna.git $(SRC_ROOT)/libuidna

# https://libzip.org/download/ # revised: 2 jun 2026
# TODO: move to git releases
$(SRC_ROOT)/libzip: | prepare
	$(call unpack_tar, https://libzip.org/download/libzip-1.11.4.tar.xz, libzip)

# Use 3.5 LTS until new LTS
# https://openssl-library.org/source/index.html # revised: 23 jun 2026
$(SRC_ROOT)/openssl: | prepare
	$(call unpack_tar, https://github.com/openssl/openssl/releases/download/openssl-3.5.7/openssl-3.5.7.tar.gz, openssl)
	$(call rule_cp,replacements/openssl/async_posix.c,$(SRC_ROOT)/openssl/crypto/async/arch/async_posix.c)
	$(call rule_cp,replacements/openssl/49-xwin-clang.conf,$(SRC_ROOT)/openssl/Configurations)
	$(call rule_cp,replacements/openssl/50-wasm-sprt-clang.conf,$(SRC_ROOT)/openssl/Configurations)

# https://github.com/gost-engine/engine # revised: 2 jun 2026
$(SRC_ROOT)/openssl-gost-engine: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/openssl-gost-engine)
	cd $(SRC_ROOT); git clone  --recurse-submodules https://github.com/gost-engine/engine.git --depth 1 --branch v3.0.3 openssl-gost-engine

# # https://github.com/bytecodealliance/wasm-micro-runtime # revised: 14 jul 2026
$(SRC_ROOT)/wasm-micro-runtime: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/wasm-micro-runtime)
	cd $(SRC_ROOT); git clone  --recurse-submodules  --branch WAMR-2.4.5 https://github.com/bytecodealliance/wasm-micro-runtime.git --depth 1 wasm-micro-runtime

# https://github.com/KhronosGroup/Vulkan-Headers # revised: 2 jun 2026
$(SRC_ROOT)/vulkan-headers: | prepare
	cd $(SRC_ROOT); git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-Headers.git vulkan-headers

# https://github.com/KhronosGroup/SPIRV-Headers # revised: 2 jun 2026
$(SRC_ROOT)/spirv-headers: | prepare
	cd $(SRC_ROOT); git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/SPIRV-Headers.git spirv-headers

# https://github.com/KhronosGroup/glslang # revised: 2 jun 2026
$(SRC_ROOT)/glslang: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/glslang.git glslang

# https://github.com/KhronosGroup/SPIRV-Tools # revised: 2 jun 2026
$(SRC_ROOT)/spirv-tools: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/SPIRV-Tools.git spirv-tools

# https://github.com/KhronosGroup/Vulkan-Loader # revised: 2 jun 2026
$(SRC_ROOT)/vulkan-loader: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-Loader.git vulkan-loader

# https://github.com/KhronosGroup/Vulkan-ValidationLayers # revised: 2 jun 2026
$(SRC_ROOT)/vulkan-validationlayers: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-ValidationLayers vulkan-validationlayers

# https://github.com/KhronosGroup/Vulkan-Utility-Libraries # revised: 2 jun 2026
$(SRC_ROOT)/vulkan-utility: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-Utility-Libraries vulkan-utility

# https://github.com/KhronosGroup/Vulkan-Tools # revised: 31 jul 2026
# Используется только target-xenolithos и только ради vulkaninfo — девайсового
# пробника «нашёл ли лоадер ICD и перечисляет ли драйвер физическое устройство».
$(SRC_ROOT)/vulkan-tools: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-Tools.git vulkan-tools

# https://github.com/KhronosGroup/MoltenVK/releases # revised: 2 jun 2026
$(SRC_ROOT)/moltenvk: | prepare
	$(call unpack_tar, https://github.com/KhronosGroup/MoltenVK/archive/refs/tags/v1.4.1.tar.gz, moltenvk)

# https://github.com/unicode-org/icu/releases # revised: 2 jun 2026
$(SRC_ROOT)/icu4c: | prepare
	$(call unpack_tar, https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-sources.tgz, icu4c)

# https://github.com/libffi/libffi/releases # revised: 14 jul 2026
# TODO: move to git releases
$(SRC_ROOT)/ffi: | prepare
	$(call unpack_tar, https://github.com/libffi/libffi/releases/download/v3.7.1/libffi-3.7.1.tar.gz, ffi)

# https://github.com/libexpat/libexpat/releases # revised: 14 jul 2026
$(SRC_ROOT)/expat: | prepare
	$(call unpack_tar, https://github.com/libexpat/libexpat/releases/download/R_2_8_2/expat-2.8.2.tar.xz, expat)

# Use upstream - releases bound with GCC
#  Pin: 549b81b43b46c0f361680561a626bf0e7b79dcbd
# https://github.com/ianlancetaylor/libbacktrace.git # revised: 23 jun 2026
$(SRC_ROOT)/libbacktrace: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libbacktrace)
	cd $(SRC_ROOT); git clone https://github.com/ianlancetaylor/libbacktrace.git $(SRC_ROOT)/libbacktrace
	cd $(SRC_ROOT)/libbacktrace; git checkout 549b81b43b46c0f361680561a626bf0e7b79dcbd

# Use upstream - releases are too old
#  Pin: f3e8262173b7089db9a9d57a9ecef8dd07ad9c97
# https://github.com/simd-everywhere/simde.git # revised: 2 jun 2026
$(SRC_ROOT)/simde: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/simde)
	cd $(SRC_ROOT); git clone https://github.com/simd-everywhere/simde.git $(SRC_ROOT)/simde
	cd $(SRC_ROOT)/simde; git checkout f3e8262173b7089db9a9d57a9ecef8dd07ad9c97

# The version itself lives in common/utils/llvm-version.mk (SP_LLVM_TAG / SP_LLVM_V);
# the patch directories under replacements/llvm are named after SP_LLVM_V.
# Next version will be 23.1.X when 24.1.0 will be released
$(SRC_ROOT)/llvm-project: | prepare
	cd $(SRC_ROOT); git clone https://github.com/llvm/llvm-project.git --branch $(SP_LLVM_TAG)  --depth 1  --recurse-submodules
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-lldb-wine/0001-Fix-incorrect-L1-inferior-memory-cache-flushing.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-lldb-wine/0002-lldb-Add-DYLD-plugin-for-debugging-Wine.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-lldb-wine/0003-lldb-Fix-Wine-preloader-name-in-POSIX-Wine-DYLD.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-lldb-wine/0004-lldb-Adapt-POSIX-Wine-DYLD-to-the-22.1-CreateBreakpoint-API.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-noulock/0001-replaced-__ulock-with-os_sync_wait_on_address.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-libunwind-wasm/0001-libunwind-tolerate-wasm-target-in-assembly.h.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-no-delayload/0001-Support-disable-shell32-ole32-delay-load-no-delayimp.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-sprt-windows/0001-lldb-Defer-to-sprt-libc-in-PosixApi.h.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-sprt-windows/0002-lldb-Use-real-terminal-interface-on-sprt-libc.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-sprt-windows/0003-compiler-rt-Build-ORC-runtime-as-C-20.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-sprt-windows/0004-lit-Make-the-suites-usable-when-cross-testing-under-wine.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-sprt-windows/0005-clang-Do-not-require-clang-repl-for-the-test-suites.patch
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/$(SP_LLVM_V)-sprt-windows/0006-compiler-rt-Include-the-POSIX-locking-headers-on-a-Windows-target.patch

# https://download.gnome.org/sources/libxml2  # revised: 2 jun 2026
$(SRC_ROOT)/libxml2: | prepare
	$(call unpack_tar, https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.3.tar.xz, libxml2)

# https://wayland.freedesktop.org/releases.html # revised: 2 jun 2026
$(SRC_ROOT)/wayland: | prepare
	$(call unpack_tar, https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.25.0/downloads/wayland-1.25.0.tar.xz, wayland)

# https://wayland.freedesktop.org/releases.html # revised: 23 jun 2026
$(SRC_ROOT)/wayland-protocols: | prepare
	$(call unpack_tar, https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/1.49/downloads/wayland-protocols-1.49.tar.xz, wayland-protocols)

# https://github.com/KDE/plasma-wayland-protocols # revised: 2 jun 2026
$(SRC_ROOT)/plasma-wayland-protocols: | prepare
	cd $(SRC_ROOT); git clone https://github.com/KDE/plasma-wayland-protocols.git --branch v1.21.0  --depth 1
 
# Keep the version within 2.4.x: patch_ver feeds the SONAME version
# (libdrm.so.2.<minor>.0), so moving to 2.5 would roll it backwards — see the
# note at the top of libdrm's own meson.build.
# https://dri.freedesktop.org/libdrm/ # revised: 1 aug 2026
$(SRC_ROOT)/libdrm: | prepare
	$(call unpack_tar, https://dri.freedesktop.org/libdrm/libdrm-2.4.134.tar.xz, libdrm)

# Inject Russia Ministry of Digital Development certificates
# https://curl.se/ca # revised: 2 jun 2026
# https://www.gosuslugi.ru/crt # revised: 2 jun 2026

CERT_NAME := cacert-2026-05-14.pem
CERT_URL := https://curl.se/ca/$(CERT_NAME)

ifeq ($(findstring Windows,$(OS)),Windows)
replacements/curl/cacert.pem: $(LIBS_MAKE_FILE) | prepare
	$(MKDIR) $(TMP_DIR) | Out-Null
	$(MKDIR) replacements/curl | Out-Null
	cd $(TMP_DIR); curl $(CERT_URL) -O $(CERT_NAME)
	cd $(TMP_DIR); curl https://gu-st.ru/content/lending/russian_trusted_root_ca_pem.crt -O russian_trusted_root_ca_pem.crt
	cd $(TMP_DIR); curl https://gu-st.ru/content/lending/russian_trusted_sub_ca_pem.crt -O russian_trusted_sub_ca_pem.crt
	cd $(TMP_DIR); curl https://gu-st.ru/content/lending/russian_trusted_sub_ca_2024_pem.crt -O russian_trusted_sub_ca_2024_pem.crt

	Get-Content "$(TMP_DIR)/$(CERT_NAME)" | Set-Content "replacements/curl/cacert.pem";
	Add-Content "replacements/curl/cacert.pem" "`nhttps://www.gosuslugi.ru/crt - Root`n====================";
	Get-Content "$(TMP_DIR)/russian_trusted_root_ca_pem.crt" | Add-Content "replacements/curl/cacert.pem";
	Add-Content "replacements/curl/cacert.pem" "`nhttps://www.gosuslugi.ru/crt - Sub`n====================";
	Get-Content "$(TMP_DIR)/russian_trusted_sub_ca_pem.crt" | Add-Content "replacements/curl/cacert.pem";
	Add-Content "replacements/curl/cacert.pem" "`nhttps://www.gosuslugi.ru/crt - Sub 2024`n====================";
	Get-Content "$(TMP_DIR)/russian_trusted_sub_ca_2024_pem.crt" | Add-Content "replacements/curl/cacert.pem";
	$(RM) $(TMP_DIR)/$(CERT_NAME)
	$(RM) $(TMP_DIR)/russian_trusted_root_ca_pem.crt
	$(RM) $(TMP_DIR)/russian_trusted_sub_ca_pem.crt
	$(RM) $(TMP_DIR)/russian_trusted_sub_ca_2024_pem.crt
else
replacements/curl/cacert.pem: $(LIBS_MAKE_FILE) | prepare
	@$(MKDIR) $(TMP_DIR)
	@$(MKDIR) replacements/curl
	cd $(TMP_DIR); wget $(CERT_URL) # revised: 11 feb 2026
	printf "\nhttps://www.gosuslugi.ru/crt - Root\n====================\n" > $(TMP_DIR)/russian_trusted_root_ca_pem.crt.txt
	cd $(TMP_DIR); wget https://gu-st.ru/content/lending/russian_trusted_root_ca_pem.crt
	printf "\nhttps://www.gosuslugi.ru/crt - Sub\n====================\n" > $(TMP_DIR)/russian_trusted_sub_ca_pem.crt.txt
	cd $(TMP_DIR); wget https://gu-st.ru/content/lending/russian_trusted_sub_ca_pem.crt
	printf "\nhttps://www.gosuslugi.ru/crt - Sub 2024\n====================\n" > $(TMP_DIR)/russian_trusted_sub_ca_2024_pem.crt.txt
	cd $(TMP_DIR); wget https://gu-st.ru/content/lending/russian_trusted_sub_ca_2024_pem.crt
	cd replacements/curl; cat \
		$(TMP_DIR)/$(CERT_NAME) \
		$(TMP_DIR)/russian_trusted_root_ca_pem.crt.txt \
		$(TMP_DIR)/russian_trusted_root_ca_pem.crt \
		$(TMP_DIR)/russian_trusted_sub_ca_pem.crt.txt \
		$(TMP_DIR)/russian_trusted_sub_ca_pem.crt \
		$(TMP_DIR)/russian_trusted_sub_ca_2024_pem.crt.txt \
		$(TMP_DIR)/russian_trusted_sub_ca_2024_pem.crt > cacert.pem
	@rm \
		$(TMP_DIR)/$(CERT_NAME) \
		$(TMP_DIR)/russian_trusted_root_ca_pem.crt.txt \
		$(TMP_DIR)/russian_trusted_root_ca_pem.crt \
		$(TMP_DIR)/russian_trusted_sub_ca_pem.crt.txt \
		$(TMP_DIR)/russian_trusted_sub_ca_pem.crt \
		$(TMP_DIR)/russian_trusted_sub_ca_2024_pem.crt.txt \
		$(TMP_DIR)/russian_trusted_sub_ca_2024_pem.crt
endif

# https://github.com/Jake-Shadle/xwin/releases
$(SRC_ROOT)/xwin: | prepare
	$(call unpack_tar, https://github.com/Jake-Shadle/xwin/releases/download/0.9.0/xwin-0.9.0-x86_64-unknown-linux-musl.tar.gz, xwin)
	touch $(SRC_ROOT)/xwin

$(SRC_ROOT)/xwin/splat: $(SRC_ROOT)/xwin
	cd $(SRC_ROOT)/xwin; ./xwin --accept-license  --arch aarch64 --arch x86_64 splat
	cd $(SRC_ROOT)/xwin; ln -s .xwin-cache/splat splat
