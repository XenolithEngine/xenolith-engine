# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
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

# Заголовки Vulkan + ICD-лоадер (арх-база, слой 1).
#
# Лоадер принадлежит арх-базе, а не пер-вендорному GPU-слою: он вендор-независим
# и является одновременно рантайм-компонентом образа и тем, что движок открывает
# через dlopen("libvulkan.so.1"). В рантайме он перечисляет
# /usr/share/vulkan/icd.d/*.json и делает dlopen драйвера оттуда — сам драйвер
# стейджится отдельно (gpu.mk, слой 2).
#
# Собирается намеренно БЕЗ WSI: Xenolith OS рендерит через VK_KHR_display прямо в
# KMS, дисплейного сервера на устройстве нет, а включённые X11/Wayland-бэкенды
# притащили бы зависимости, которых в rootfs не будет.
#
# Про кросс-сборку: диспетчер неизвестных функций у лоадера требует
# gen_defines.asm, который обычно получают ЗАПУСКОМ asm_offset. Под
# CMAKE_CROSSCOMPILING сборка вместо этого компилирует asm_offset в ассемблер и
# разбирает его через scripts/parse_asm_values.py — отсюда python3 в
# check-engine. CMAKE_SYSTEM_NAME всегда задан в clang.cmake, так что эта ветка
# выбирается даже когда арка цели совпадает с архой хоста.

TARGET_VULKAN_HEADERS := $(TARGET_SYSROOT)/usr/include/vulkan/vulkan_core.h
TARGET_VULKAN_LOADER  := $(TARGET_SYSROOT)/usr/lib/libvulkan.so.1

# ── заголовки (header-only; ставят заодно cmake-конфиг VulkanHeaders) ─────────
# Лоадер находит их через find_package(VulkanHeaders CONFIG), поэтому они должны
# оказаться в sysroot до его конфигурации.
#
# Замыкающий touch обязателен, а не косметика: проект ставит исходные файлы
# как есть, и cmake сохраняет их upstream-mtime (старше toolchain-файла, от
# которого зависит цель). Без него цель вечно устаревшая, и каждый `make out`
# молча пересобирает лоадер и перекладывает стейджинг.
$(TARGET_VULKAN_HEADERS): $(TARGET_CMAKE_TOOLCHAIN) | $(VULKAN_HEADERS_SRC_DIR)
	rm -rf $(TBUILD)/vulkan-headers
	cmake -G Ninja -S $(VULKAN_HEADERS_SRC_DIR) -B $(TBUILD)/vulkan-headers \
		-DCMAKE_TOOLCHAIN_FILE=$(TARGET_CMAKE_TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(SYSROOT_ABS)/usr \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DVULKAN_HEADERS_ENABLE_MODULE=OFF
	cmake --install $(TBUILD)/vulkan-headers
	touch $@

# ── лоадер ────────────────────────────────────────────────────────────────────
# SYSCONFDIR / FALLBACK_*_DIRS прибиты к ДЕВАЙСОВЫМ путям, а не к префиксу
# сборки. Без этого лоадер вшил бы $(TARGET_SYSROOT)/usr/etc и не нашёл бы на
# устройстве ни одного ICD-манифеста — тот же класс бага, что glibc с чужим
# --prefix.
#
# CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ослаблен до BOTH только для этой сборки:
# clang.cmake фиксирует ONLY, что перекорневало бы абсолютный CMAKE_PREFIX_PATH
# под sysroot и потеряло конфиг VulkanHeaders.
$(TARGET_VULKAN_LOADER): $(TARGET_VULKAN_HEADERS) $(TARGET_LIBCXX) | $(VULKAN_LOADER_SRC_DIR)
	rm -rf $(TBUILD)/vulkan-loader
	cmake -G Ninja -S $(VULKAN_LOADER_SRC_DIR) -B $(TBUILD)/vulkan-loader \
		-DCMAKE_TOOLCHAIN_FILE=$(TARGET_CMAKE_TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(SYSROOT_ABS)/usr \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DCMAKE_INSTALL_INCLUDEDIR=include \
		-DCMAKE_PREFIX_PATH=$(SYSROOT_ABS)/usr \
		-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
		-DSYSCONFDIR=/etc \
		-DFALLBACK_CONFIG_DIRS=/etc/xdg \
		-DFALLBACK_DATA_DIRS=/usr/local/share:/usr/share \
		-DBUILD_TESTS=OFF \
		-DBUILD_WSI_XCB_SUPPORT=OFF \
		-DBUILD_WSI_XLIB_SUPPORT=OFF \
		-DBUILD_WSI_XLIB_XRANDR_SUPPORT=OFF \
		-DBUILD_WSI_WAYLAND_SUPPORT=OFF \
		-DBUILD_WSI_DIRECTFB_SUPPORT=OFF \
		-DCMAKE_C_FLAGS_INIT="-resource-dir $(TARGET_RESOURCE_DIR)" \
		-DCMAKE_EXE_LINKER_FLAGS_INIT="-fuse-ld=lld" \
		-DCMAKE_SHARED_LINKER_FLAGS_INIT="-fuse-ld=lld"
	cmake --build $(TBUILD)/vulkan-loader $(SP_NJOBS)
	cmake --install $(TBUILD)/vulkan-loader
	@echo "[vulkan] loader -> $(TARGET_VULKAN_LOADER)"

vulkan: $(TARGET_VULKAN_HEADERS) $(TARGET_VULKAN_LOADER)

.PHONY: vulkan
