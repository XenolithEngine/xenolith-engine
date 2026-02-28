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
	simde \
	llvm-project \
	libxml2

TAR_XF = tar -xf

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
	$(MKDIR) $(firstword $(2)); \
	$(TAR_XF) $(notdir $(1)) --strip-components=1 -C $(firstword $(2)); \
	rm -rf $(SRC_ROOT)/$(firstword $(2)); \
	mv -f $(firstword $(2)) $(SRC_ROOT)/$(firstword $(2)); \
	touch $(SRC_ROOT)/$(firstword $(2)); \
	rm $(notdir $(1))

endif


# https://www.zlib.net/ # revised: 22 feb 2026
$(SRC_ROOT)/zlib: | prepare
	$(call unpack_tar, https://www.zlib.net/zlib-1.3.2.tar.gz, zlib)

# https://sourceware.org/bzip2/downloads.html # revised: 22 feb 2026
$(SRC_ROOT)/bzip2: | prepare
	$(call unpack_tar, https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz, bzip2)

# https://tukaani.org/xz/#_source_packages # revised: 22 feb 2026
$(SRC_ROOT)/xz: | prepare
	$(call unpack_tar, https://github.com/tukaani-project/xz/releases/download/v5.8.2/xz-5.8.2.tar.xz, xz)

# https://github.com/facebook/zstd/releases # revised: 22 feb 2026
$(SRC_ROOT)/zstd: | prepare
	$(call unpack_tar, https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz, zstd)

# https://github.com/libjpeg-turbo/libjpeg-turbo/releases # revised: 22 feb 2026
$(SRC_ROOT)/libjpeg-turbo: | prepare
	$(call unpack_tar, https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.3/libjpeg-turbo-3.1.3.tar.gz, libjpeg-turbo)

# Move to github source releases; sourceforge distribution can block downloads from Russia
# https://github.com/pnggroup/libpng # revised: 22 feb 2026
$(SRC_ROOT)/libpng: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libpng)
	cd $(SRC_ROOT); git clone  --recurse-submodules  --branch v1.6.55 https://github.com/pnggroup/libpng.git --depth 1 libpng

#  Move to Void Linux source archives; sourceforge distribution can block downloads from Russia
# https://sources.voidlinux.org # revised: 22 feb 2026
$(SRC_ROOT)/giflib: | prepare
	$(call unpack_tar, https://sources.voidlinux.org/giflib-5.2.2/giflib-5.2.2.tar.gz, giflib)

# https://storage.googleapis.com/downloads.webmproject.org/releases/webp/index.html # revised: 22 feb 2026
$(SRC_ROOT)/libwebp: | prepare
	$(call unpack_tar, https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz, libwebp)

# https://download.osgeo.org/libtiff/?C=M&O=D # revised: 22 feb 2026
$(SRC_ROOT)/tiff: | prepare
	$(call unpack_tar, https://download.osgeo.org/libtiff/tiff-4.7.1.tar.xz, tiff)

# https://github.com/google/brotli/releases # revised: 22 feb 2026
$(SRC_ROOT)/brotli: | prepare
	$(call unpack_tar, https://github.com/google/brotli/archive/refs/tags/v1.2.0.tar.gz, brotli)

# Use Mbed TLS 3.6 until at least 2027
# https://github.com/Mbed-TLS/mbedtls/releases # revised: 22 feb 2026
$(SRC_ROOT)/mbedtls: | prepare
	$(call unpack_tar, https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.5/mbedtls-3.6.5.tar.bz2, mbedtls)

# https://github.com/ngtcp2/nghttp3/releases # revised: 22 feb 2026
$(SRC_ROOT)/nghttp3: | prepare
	$(call unpack_tar, https://github.com/ngtcp2/nghttp3/releases/download/v1.15.0/nghttp3-1.15.0.tar.xz, nghttp3)

# https://curl.se/download.html # revised: 22 feb 2026
$(SRC_ROOT)/curl: | prepare
	$(call unpack_tar, https://curl.se/download/curl-8.18.0.tar.xz, curl)

# https://download.savannah.gnu.org/releases/freetype/?C=M&O=D # revised: 22 feb 2026
$(SRC_ROOT)/freetype: | prepare
	$(call unpack_tar, https://download.savannah.gnu.org/releases/freetype/freetype-2.14.1.tar.xz, freetype)

# https://github.com/harfbuzz/harfbuzz/releases/ # revised: 22 feb 2026
$(SRC_ROOT)/harfbuzz: | prepare
	$(call unpack_tar, https://github.com/harfbuzz/harfbuzz/releases/download/12.3.2/harfbuzz-12.3.2.tar.xz, harfbuzz)


# https://www.sqlite.org/download.html # revised: 22 feb 2026
SQLITE_URL := https://www.sqlite.org/2026/sqlite-amalgamation-3510200.zip
ifeq ($(findstring Windows,$(OS)),Windows)
$(SRC_ROOT)/sqlite: | prepare
	@$(MKDIR) $(SRC_ROOT); $(MKDIR) $(TMP_DIR)
	cd $(TMP_DIR); $(WGET) $(SQLITE_URL) -O sqlite-amalgamation.zip
	cd $(TMP_DIR); Expand-Archive -Path sqlite-amalgamation.zip -DestinationPath .
	$(RM) $(TMP_DIR)/sqlite-amalgamation.zip
	powershell Move-Item -Path $(TMP_DIR)/sqlite-amalgamation-3510200  -Destination $(SRC_ROOT)/sqlite
else
$(SRC_ROOT)/sqlite: | prepare
	@$(MKDIR) $(SRC_ROOT); $(MKDIR) $(TMP_DIR)
	cd $(TMP_DIR); $(WGET) $(SQLITE_URL) -O sqlite-amalgamation.zip
	cd $(TMP_DIR); unzip sqlite-amalgamation.zip -d .
	rm $(TMP_DIR)/sqlite-amalgamation.zip
	mv -f $(TMP_DIR)/sqlite-amalgamation-3510200 $(SRC_ROOT)/sqlite
endif


# https://github.com/SBKarr/libuidna # revised: 22 feb 2026
$(SRC_ROOT)/libuidna: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libuidna)
	cd $(SRC_ROOT); git clone https://github.com/SBKarr/libuidna.git $(SRC_ROOT)/libuidna

# https://libzip.org/download/ # revised: 22 feb 2026
$(SRC_ROOT)/libzip: | prepare
	$(call unpack_tar, https://libzip.org/download/libzip-1.11.4.tar.xz, libzip)

# Use 3.5 LTS until new LTS
# https://openssl-library.org/source/index.html # revised: 22 feb 2026
$(SRC_ROOT)/openssl: | prepare
	$(call unpack_tar, https://github.com/openssl/openssl/releases/download/openssl-3.5.5/openssl-3.5.5.tar.gz, openssl)
	$(call rule_cp,replacements/openssl/async_posix.c,$(SRC_ROOT)/openssl/crypto/async/arch/async_posix.c)

# https://github.com/gost-engine/engine # revised: 22 feb 2026
$(SRC_ROOT)/openssl-gost-engine: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/openssl-gost-engine)
	cd $(SRC_ROOT); git clone  --recurse-submodules https://github.com/gost-engine/engine.git --depth 1 --branch v3.0.3 openssl-gost-engine

# # https://github.com/bytecodealliance/wasm-micro-runtime # revised: 22 feb 2026
$(SRC_ROOT)/wasm-micro-runtime: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/wasm-micro-runtime)
	cd $(SRC_ROOT); git clone  --recurse-submodules  --branch WAMR-2.4.4 https://github.com/bytecodealliance/wasm-micro-runtime.git --depth 1 wasm-micro-runtime

# https://github.com/KhronosGroup/Vulkan-Headers # revised: 22 feb 2026
$(SRC_ROOT)/vulkan-headers: | prepare
	cd $(SRC_ROOT); git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-Headers.git vulkan-headers

# https://github.com/KhronosGroup/SPIRV-Headers # revised: 22 feb 2026
$(SRC_ROOT)/spirv-headers: | prepare
	cd $(SRC_ROOT); git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/SPIRV-Headers.git spirv-headers

# https://github.com/KhronosGroup/glslang # revised: 22 feb 2026
$(SRC_ROOT)/glslang: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/glslang.git glslang

# https://github.com/KhronosGroup/SPIRV-Tools # revised: 22 feb 2026
$(SRC_ROOT)/spirv-tools: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/SPIRV-Tools.git spirv-tools

# https://github.com/KhronosGroup/Vulkan-Loader # revised: 22 feb 2026
$(SRC_ROOT)/vulkan-loader: | prepare
	cd $(SRC_ROOT);  git clone  --recurse-submodules --branch vulkan-sdk-$(VULKAN_SDK_VER) --depth 1 https://github.com/KhronosGroup/Vulkan-Loader.git vulkan-loader

# https://github.com/unicode-org/icu/releases # revised: 22 feb 2026
$(SRC_ROOT)/icu4c: | prepare
	$(call unpack_tar, https://github.com/unicode-org/icu/releases/download/release-78.2/icu4c-78.2-sources.tgz, icu4c)

# https://github.com/libffi/libffi/releases # revised: 22 feb 2026
$(SRC_ROOT)/ffi: | prepare
	$(call unpack_tar, https://github.com/libffi/libffi/releases/download/v3.5.2/libffi-3.5.2.tar.gz, ffi)

# https://github.com/libexpat/libexpat/releases # revised: 22 feb 2026
$(SRC_ROOT)/expat: | prepare
	$(call unpack_tar, https://github.com/libexpat/libexpat/releases/download/R_2_7_4/expat-2.7.4.tar.gz, expat)

# Use upstream - releases bound with GCC
# https://github.com/ianlancetaylor/libbacktrace.git # revised: 22 feb 2026
$(SRC_ROOT)/libbacktrace: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/libbacktrace)
	cd $(SRC_ROOT); git clone https://github.com/ianlancetaylor/libbacktrace.git  --depth 1 $(SRC_ROOT)/libbacktrace

# Use upstream - releases are too old
# https://github.com/simd-everywhere/simde.git # revised: 22 feb 2026
$(SRC_ROOT)/simde: | prepare
	@$(MKDIR) $(SRC_ROOT)
	$(call rule_rm,$(SRC_ROOT)/simde)
	cd $(SRC_ROOT); git clone https://github.com/simd-everywhere/simde.git  --depth 1 $(SRC_ROOT)/simde

# https://github.com/llvm/llvm-project/releases # revised: 24 feb 2026
$(SRC_ROOT)/llvm-project: | prepare
	cd $(SRC_ROOT); git clone https://github.com/llvm/llvm-project.git --branch llvmorg-21.1.8  --depth 1  --recurse-submodules
	cd $(SRC_ROOT)/llvm-project; git apply -p1 ../../replacements/llvm/21.1.8/0001-replaced-__ulock-with-os_sync_wait_on_address.patch

# https://download.gnome.org/sources/libxml2  # revised: 26 feb 2026
$(SRC_ROOT)/libxml2: | prepare
	$(call unpack_tar, https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.1.tar.xz, libxml2)

# Inject Russia Ministry of Digital Development certificates
# https://curl.se/ca # revised: 22 feb 2026
# https://www.gosuslugi.ru/crt # revised: 22 feb 2026

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
