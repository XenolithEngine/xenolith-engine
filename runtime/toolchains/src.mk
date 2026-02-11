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

LIBS = \
	bzip2 \
	xz \
	zstd \
	jpeg \
	libjpeg-turbo \
	libpng \
	giflib \
	libwebp \
	tiff \
	brotli \
	curl \
	freetype \
	harfbuzz \
	sqlite \
	libuidna \
	mbedtls \
	nghttp3 \
	libzip \
	zlib \
	openssl \
	openssl-gost-engine \
	wasm-micro-runtime \
	vulkan-headers \
	spirv-headers \
	glslang \
	spirv-tools \
	icu4c \
	ffi \
	expat \
	libbacktrace \
	simde

TAR_XF = tar -xzf

VULKAN_SDK_VER := 1.4.328.1

ifeq ($(findstring Windows,$(OS)),Windows)

unpack_tar = $(MKDIR) $(SRC_ROOT) | Out-Null; $(MKDIR) $(TMP_DIR)/$(firstword $(2)) | Out-Null; \
	cd $(TMP_DIR); \
	$(WGET) -o $(notdir $(1)) $(1); \
	$(TAR_XF) $(notdir $(1)) --strip-components=1 -C $(firstword $(2)); \
	Remove-Item -Path "$(SRC_ROOT)/$(firstword $(2))" -Recurse -Force -ErrorAction SilentlyContinue; \
	Move-Item -Path $(firstword $(2)) -Destination $(SRC_ROOT)/$(firstword $(2)); \
	(Get-Item "$(SRC_ROOT)/$(firstword $(2))").LastWriteTime = $$(Get-Date); \

else

get_tar_top_dir = `tar -tf $(1)  | head -1 | cut -f1 -d"/"`

unpack_tar = $(MKDIR) $(SRC_ROOT); $(MKDIR) $(TMP_DIR); \
	cd $(TMP_DIR); \
	$(WGET)  -O $(notdir $(1)) $(1); \
	$(TAR_XF) $(notdir $(1)) --strip-components=1 -C $(firstword $(2)); \
	rm -rf $(SRC_ROOT)/$(firstword $(2)); \
	mv -f $(firstword $(2)) $(SRC_ROOT)/$(firstword $(2)); \
	touch $(SRC_ROOT)/$(firstword $(2)); \
	rm $(notdir $(1))

endif


# https://www.zlib.net/
$(SRC_ROOT)/zlib: | prepare
	$(call unpack_tar, https://www.zlib.net/zlib-1.3.1.tar.gz, zlib) # revised: 27 oct 2025

# https://sourceware.org/bzip2/downloads.html
$(SRC_ROOT)/bzip2: | prepare
	$(call unpack_tar, https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz, bzip2) # revised: 3 nov 2025

# https://tukaani.org/xz/#_source_packages
$(SRC_ROOT)/xz: | prepare
	$(call unpack_tar, https://github.com/tukaani-project/xz/releases/download/v5.8.2/xz-5.8.2.tar.xz, xz) # revised: 11 feb 2026

# https://github.com/facebook/zstd/releases/tag/v1.5.7
$(SRC_ROOT)/zstd: | prepare
	$(call unpack_tar, https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz, zstd) # revised: 3 nov 2025

# https://github.com/libjpeg-turbo/libjpeg-turbo/releases
$(SRC_ROOT)/libjpeg-turbo: | prepare
	$(call unpack_tar, https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.2/libjpeg-turbo-3.1.2.tar.gz, libjpeg-turbo) # revised: 27 oct 2025

# Move to github source releases; sourceforge distribution can block downloads from Russia
# https://github.com/pnggroup/libpng
$(SRC_ROOT)/libpng: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libpng)
	cd $(SRC_ROOT); git clone  --recurse-submodules  --branch v1.6.55 https://github.com/pnggroup/libpng.git --depth 1 libpng # revised: 12 feb 2026

#  Move to Void Linux source archives; sourceforge distribution can block downloads from Russia
# https://sources.voidlinux.org/giflib-5.2.2/giflib-5.2.2.tar.gz
$(SRC_ROOT)/giflib: | prepare
	$(call unpack_tar, https://sources.voidlinux.org/giflib-5.2.2/giflib-5.2.2.tar.gz, giflib) # revised: 27 oct 2025

# https://storage.googleapis.com/downloads.webmproject.org/releases/webp/index.html
$(SRC_ROOT)/libwebp: | prepare
	$(call unpack_tar, https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz, libwebp) # revised: 27 oct 2025

# https://download.osgeo.org/libtiff/?C=M&O=D
$(SRC_ROOT)/tiff: | prepare
	$(call unpack_tar, https://download.osgeo.org/libtiff/tiff-4.7.1.tar.xz, tiff) # revised: 3 nov 2025

# https://github.com/google/brotli/releases
$(SRC_ROOT)/brotli: | prepare
	$(call unpack_tar, https://github.com/google/brotli/archive/refs/tags/v1.1.0.tar.gz, brotli) # revised: 27 oct 2025

# https://github.com/Mbed-TLS/mbedtls/releases
$(SRC_ROOT)/mbedtls: | prepare
	$(call unpack_tar, https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.5/mbedtls-3.6.5.tar.bz2, mbedtls) # revised: 27 oct 2025

# https://github.com/ngtcp2/nghttp3/releases
$(SRC_ROOT)/nghttp3: | prepare
	$(call unpack_tar, https://github.com/ngtcp2/nghttp3/releases/download/v1.15.0/nghttp3-1.15.0.tar.xz, nghttp3) # revised: 27 oct 2025

# https://curl.se/download.html
$(SRC_ROOT)/curl: | prepare
	$(call unpack_tar, https://curl.se/download/curl-8.16.0.tar.xz, curl) # revised: 27 oct 2025

# https://download.savannah.gnu.org/releases/freetype/?C=M&O=D
$(SRC_ROOT)/freetype: | prepare
	$(call unpack_tar, https://download.savannah.gnu.org/releases/freetype/freetype-2.14.1.tar.xz, freetype) # revised: 27 oct 2025

# https://github.com/harfbuzz/harfbuzz/releases/
$(SRC_ROOT)/harfbuzz: | prepare
	$(call unpack_tar, https://github.com/harfbuzz/harfbuzz/releases/download/12.1.0/harfbuzz-12.1.0.tar.xz, harfbuzz) # revised: 3 nov 2025


# https://www.sqlite.org/download.html
SQLITE_URL := https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip # revised: 27 oct 2025
ifeq ($(findstring Windows,$(OS)),Windows)
$(SRC_ROOT)/sqlite: | prepare
	@$(MKDIR) $(SRC_ROOT); $(MKDIR) $(TMP_DIR)
	cd $(TMP_DIR); $(WGET) $(SQLITE_URL) -O sqlite-amalgamation.zip
	cd $(TMP_DIR); Expand-Archive -Path sqlite-amalgamation.zip -DestinationPath .
	$(RM) $(TMP_DIR)/sqlite-amalgamation.zip
	powershell Move-Item -Path $(TMP_DIR)/sqlite-amalgamation-3500400  -Destination $(SRC_ROOT)/sqlite
else
$(SRC_ROOT)/sqlite: | prepare
	@$(MKDIR) $(SRC_ROOT); $(MKDIR) $(TMP_DIR)
	cd $(TMP_DIR); $(WGET) $(SQLITE_URL) -O sqlite-amalgamation.zip
	cd $(TMP_DIR); unzip sqlite-amalgamation.zip -d .
	rm $(TMP_DIR)/sqlite-amalgamation.zip
	mv -f $(TMP_DIR)/sqlite-amalgamation-3500400 $(SRC_ROOT)/sqlite
endif


# https://github.com/SBKarr/libuidna
$(SRC_ROOT)/libuidna: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libuidna)
	cd $(SRC_ROOT); git clone https://github.com/SBKarr/libuidna.git $(SRC_ROOT)/libuidna # use upstream: 27 oct 2025

# https://libzip.org/download/
$(SRC_ROOT)/libzip: | prepare
	$(call unpack_tar, https://libzip.org/download/libzip-1.11.4.tar.xz, libzip) # revised: 27 oct 2025

# https://openssl-library.org/source/index.html
$(SRC_ROOT)/openssl: | prepare
	$(call unpack_tar, https://github.com/openssl/openssl/releases/download/openssl-3.5.4/openssl-3.5.4.tar.gz, openssl) # revised: 27 oct 2025
	$(call rule_cp,replacements/openssl/async_posix.c,$(SRC_ROOT)/openssl/crypto/async/arch/async_posix.c)

# https://github.com/gost-engine/engine
$(SRC_ROOT)/openssl-gost-engine: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/openssl-gost-engine)
	cd $(SRC_ROOT); git clone  --recurse-submodules https://github.com/gost-engine/engine.git --depth 1 openssl-gost-engine # revised: 27 oct 2025
	$(call rule_cp,replacements/openssl-gost-engine/CMakeLists.txt,$(SRC_ROOT)/openssl-gost-engine)

# https://github.com/bytecodealliance/wasm-micro-runtime
$(SRC_ROOT)/wasm-micro-runtime: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/wasm-micro-runtime)
	cd $(SRC_ROOT); git clone  --recurse-submodules  --branch WAMR-2.4.3 https://github.com/bytecodealliance/wasm-micro-runtime.git --depth 1 wasm-micro-runtime # revised: 27 oct 2025

# https://github.com/KhronosGroup/Vulkan-Headers
$(SRC_ROOT)/vulkan-headers: | prepare  # revised: 28 oct 2025
	cd $(SRC_ROOT); git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-Headers.git vulkan-headers

# https://github.com/KhronosGroup/SPIRV-Headers
$(SRC_ROOT)/spirv-headers: | prepare  # revised: 28 oct 2025
	cd $(SRC_ROOT); git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/SPIRV-Headers.git spirv-headers

# https://github.com/KhronosGroup/glslang
$(SRC_ROOT)/glslang: | prepare # revised: 28 oct 2025
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/glslang.git glslang

# https://github.com/KhronosGroup/SPIRV-Tools
$(SRC_ROOT)/spirv-tools: | prepare # revised: 28 oct 2025
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/SPIRV-Tools.git spirv-tools

# https://github.com/KhronosGroup/Vulkan-Loader
$(SRC_ROOT)/vulkan-loader: | prepare # revised: 9 nov 2025
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-Loader.git vulkan-loader

# https://github.com/unicode-org/icu/releases
$(SRC_ROOT)/icu4c: | prepare
	$(call unpack_tar, https://github.com/unicode-org/icu/releases/download/release-78.1/icu4c-78.1-sources.tgz, icu4c) # revised: 6 nov 2025

# https://github.com/libffi/libffi/releases
$(SRC_ROOT)/ffi: | prepare
	$(call unpack_tar, https://github.com/libffi/libffi/releases/download/v3.5.2/libffi-3.5.2.tar.gz, ffi) # revised: 6 nov 2025

# https://github.com/libexpat/libexpat/releases
$(SRC_ROOT)/expat: | prepare
	$(call unpack_tar, https://github.com/libexpat/libexpat/releases/download/R_2_7_3/expat-2.7.3.tar.xz, expat) # revised: 6 nov 2025

# https://github.com/ianlancetaylor/libbacktrace.git
$(SRC_ROOT)/libbacktrace: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libbacktrace)
	cd $(SRC_ROOT); git clone https://github.com/ianlancetaylor/libbacktrace.git  --depth 1 $(SRC_ROOT)/libbacktrace # use upstream: 15 dec 2025

# https://github.com/simd-everywhere/simde.git
$(SRC_ROOT)/simde: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/simde)
	cd $(SRC_ROOT); git clone https://github.com/simd-everywhere/simde.git  --depth 1 $(SRC_ROOT)/simde # use upstream: 15 dec 2025

# https://dbus.freedesktop.org/releases/dbus/?C=M;O=D
$(SRC_ROOT)/dbus: | prepare
	$(call unpack_tar, https://dbus.freedesktop.org/releases/dbus/dbus-1.16.2.tar.xz, dbus) # revised: 5 nov 2025

# https://xcb.freedesktop.org/dist/?C=M;O=D
$(SRC_ROOT)/libxcb: | prepare
	$(call unpack_tar, https://xcb.freedesktop.org/dist/libxcb-1.17.0.tar.xz, libxcb) # revised: 5 nov 2025

# https://xcb.freedesktop.org/dist/?C=M;O=D
$(SRC_ROOT)/xcb-proto: | prepare
	$(call unpack_tar, https://xcb.freedesktop.org/dist/xcb-proto-1.17.0.tar.xz, xcb-proto) # revised: 5 nov 2025

# https://www.x.org/releases/individual/lib/?C=M;O=D
$(SRC_ROOT)/libXau: | prepare
	$(call unpack_tar, https://www.x.org/archive/individual/lib/libXau-1.0.12.tar.xz, libXau) # revised: 5 nov 2025
	
# https://www.x.org/releases/individual/proto/?C=M;O=D
$(SRC_ROOT)/xorgproto: | prepare
	$(call unpack_tar, https://www.x.org/releases/individual/proto/xorgproto-2024.1.tar.xz, xorgproto) # revised: 5 nov 2025

# https://github.com/xkbcommon/libxkbcommon/releases
$(SRC_ROOT)/xkbcommon: | prepare
	$(call unpack_tar, https://github.com/xkbcommon/libxkbcommon/archive/refs/tags/xkbcommon-1.12.3.tar.gz, xkbcommon) # revised: 6 nov 2025

# https://www.x.org/releases/individual/lib/?C=M;O=D
$(SRC_ROOT)/xcb-util-keysyms: | prepare
	$(call unpack_tar, https://www.x.org/archive/individual/lib/xcb-util-keysyms-0.4.1.tar.xz, xcb-util-keysyms) # revised: 6 nov 2025

# https://www.x.org/releases/individual/lib/?C=M;O=D
$(SRC_ROOT)/xcb-util-errors: | prepare
	$(call unpack_tar, https://www.x.org/archive/individual/lib/xcb-util-errors-1.0.1.tar.xz, xcb-util-errors) # revised: 6 nov 2025

# https://www.x.org/releases/individual/lib/?C=M;O=D
$(SRC_ROOT)/xcb-util-cursor: | prepare
	$(call unpack_tar, https://www.x.org/archive/individual/lib/xcb-util-cursor-0.1.6.tar.xz, xcb-util-cursor) # revised: 6 nov 2025

# https://wayland.freedesktop.org/releases.html
$(SRC_ROOT)/wayland: | prepare
	$(call unpack_tar, https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.24.0/downloads/wayland-1.24.0.tar.xz, wayland) # revised: 6 nov 2025

# Inject Russia Ministry of Digital Development certificates
# https://curl.se/ca
# https://www.gosuslugi.ru/crt

CERT_NAME := cacert-2025-12-02.pem
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
	$(call unpack_tar, https://github.com/Jake-Shadle/xwin/releases/download/0.6.7/xwin-0.6.7-x86_64-unknown-linux-musl.tar.gz, xwin)
	touch $(SRC_ROOT)/xwin

$(SRC_ROOT)/xwin/splat: $(SRC_ROOT)/xwin
	cd $(SRC_ROOT)/xwin; ./xwin --accept-license splat
	cd $(SRC_ROOT)/xwin; ln -s .xwin-cache/splat splat
