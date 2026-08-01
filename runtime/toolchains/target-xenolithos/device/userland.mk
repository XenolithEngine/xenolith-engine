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

# Userland устройства: busybox + dropbear + vulkaninfo.
#
# Это исполняемые файлы, которые едут НА устройство, а не части sysroot, поэтому
# ставятся в отдельное дерево sysroot-userland-<triple>. rootfs.mk стейджит их
# вместе с рантайм-замыканием библиотек; компайл-тайм-sysroot остаётся чистым от
# девайсовых бинарей.
#
# Арх-специфичны, GPU-независимы — один и тот же userland обслуживает любого
# вендора GPU.

USERLAND_SYSROOT := sysroot-userland-$(SP_TARGET_TRIPLE)

TARGET_BUSYBOX    := $(USERLAND_SYSROOT)/bin/busybox
TARGET_DROPBEAR   := $(USERLAND_SYSROOT)/usr/sbin/dropbear
TARGET_VULKANINFO := $(USERLAND_SYSROOT)/usr/bin/vulkaninfo

# ── busybox ───────────────────────────────────────────────────────────────────
# Собирается динамически: libc.a в sysroot нет, и статическая glibc для
# поставляемого продукта запрещена лицензией. telnetd/httpd вырезаются из
# defconfig: неаутентифицированным сетевым демонам в киоск-образе не место.
$(TARGET_BUSYBOX): $(TARGET_GLIBC) $(BUSYBOX_SRC_DIR) | check-engine
	rm -rf $(TBUILD)/busybox
	mkdir -p $(TBUILD)/busybox
	make -C $(BUSYBOX_SRC_DIR) O=$(abspath $(TBUILD)/busybox) defconfig
	sed -i 's/^CONFIG_TELNETD=y/# CONFIG_TELNETD is not set/' $(TBUILD)/busybox/.config
	sed -i 's/^CONFIG_HTTPD=y/# CONFIG_HTTPD is not set/'     $(TBUILD)/busybox/.config
	make -C $(BUSYBOX_SRC_DIR) O=$(abspath $(TBUILD)/busybox) oldconfig
	PATH=$(TARGET_PATH):$$PATH make -C $(BUSYBOX_SRC_DIR) \
		O=$(abspath $(TBUILD)/busybox) \
		CC="$(CROSS_CC)" HOSTCC=cc CROSS_COMPILE=$(SP_TARGET_TRIPLE)- \
		SKIP_STRIP=y $(SP_NJOBS)
	PATH=$(TARGET_PATH):$$PATH make -C $(BUSYBOX_SRC_DIR) \
		O=$(abspath $(TBUILD)/busybox) \
		CC="$(CROSS_CC)" CROSS_COMPILE=$(SP_TARGET_TRIPLE)- \
		CONFIG_PREFIX=$(abspath $(USERLAND_SYSROOT)) install
	@echo "[userland] busybox -> $@"

# ── dropbear ──────────────────────────────────────────────────────────────────
# Отладочный/эксплуатационный SSH (swap приложения, fleet-воркфлоу). Проверка
# пароля против /etc/shadow требует crypt(3) -> $(TARGET_LIBXCRYPT).
#
# scp собирается намеренно: у dropbear нет sftp-server, поэтому macOS-овский
# OpenSSH scp по умолчанию падает и сборщик образа откатывается на ssh+cat.
# Наличие scp на устройстве включает быстрый путь.
#
# utmp/wtmp/lastlog отключены: busybox не ведёт записей логина, так что этот код
# был бы мёртвым весом.
DROPBEAR_PROGRAMS := dropbear dropbearkey scp

$(TARGET_DROPBEAR): $(TARGET_LIBXCRYPT) $(DROPBEAR_SRC_DIR) | check-engine
	rm -rf $(TBUILD)/dropbear
	mkdir -p $(TBUILD)/dropbear
	cd $(TBUILD)/dropbear; PATH=$(TARGET_PATH):$$PATH \
		$(abspath $(DROPBEAR_SRC_DIR))/configure \
		--host=$(SP_TARGET_TRIPLE) \
		--prefix=/usr --sbindir=/usr/sbin --bindir=/usr/bin \
		--disable-zlib \
		--disable-utmp --disable-utmpx \
		--disable-wtmp --disable-wtmpx \
		--disable-lastlog \
		--disable-pututline --disable-pututxline \
		CC="$(CROSS_CC)" LIBS="-lcrypt"
	PATH=$(TARGET_PATH):$$PATH make -C $(TBUILD)/dropbear \
		PROGRAMS="$(DROPBEAR_PROGRAMS)" $(SP_NJOBS)
	PATH=$(TARGET_PATH):$$PATH make -C $(TBUILD)/dropbear \
		PROGRAMS="$(DROPBEAR_PROGRAMS)" \
		DESTDIR=$(abspath $(USERLAND_SYSROOT)) install
	@echo "[userland] dropbear -> $@"

# ── vulkaninfo ────────────────────────────────────────────────────────────────
# Девайсовый пробник, отвечающий на вопрос «нашёл ли лоадер ICD и перечисляет ли
# драйвер физическое устройство». Из Vulkan-Tools берётся ТОЛЬКО vulkaninfo:
# cube тянет glslang, а mock-ICD — тестовая заглушка.
#
# vulkaninfo компилируется с VK_NO_PROTOTYPES и грузит лоадер сам, поэтому не
# несёт NEEDED на libvulkan — mkrootfs.sh обязан копировать лоадер явно.
# На Linux сборка всегда определяет VK_USE_PLATFORM_DISPLAY — ровно тот тип
# поверхности, который использует Xenolith OS (прямой KMS); бэкенды X11/Wayland
# выключены, их pkg-config-пакетов в этом sysroot нет.
$(TARGET_VULKANINFO): $(TARGET_VULKAN_HEADERS) $(TARGET_LIBCXX) | $(VULKAN_TOOLS_SRC_DIR)
	rm -rf $(TBUILD)/vulkan-tools
	cmake -G Ninja -S $(VULKAN_TOOLS_SRC_DIR) -B $(TBUILD)/vulkan-tools \
		-DCMAKE_TOOLCHAIN_FILE=$(TARGET_CMAKE_TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=$(abspath $(USERLAND_SYSROOT))/usr \
		-DCMAKE_INSTALL_BINDIR=bin \
		-DCMAKE_PREFIX_PATH=$(SYSROOT_ABS)/usr \
		-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
		-DBUILD_VULKANINFO=ON \
		-DBUILD_CUBE=OFF \
		-DBUILD_ICD=OFF \
		-DBUILD_TESTS=OFF \
		-DBUILD_WSI_XCB_SUPPORT=OFF \
		-DBUILD_WSI_XLIB_SUPPORT=OFF \
		-DBUILD_WSI_WAYLAND_SUPPORT=OFF \
		-DBUILD_WSI_DIRECTFB_SUPPORT=OFF \
		-DCMAKE_CXX_FLAGS_INIT="-resource-dir $(TARGET_RESOURCE_DIR) -stdlib=libc++" \
		-DCMAKE_EXE_LINKER_FLAGS_INIT="-fuse-ld=lld -stdlib=libc++"
	cmake --build $(TBUILD)/vulkan-tools $(SP_NJOBS)
	cmake --install $(TBUILD)/vulkan-tools
	@echo "[userland] vulkaninfo -> $@"

userland: $(TARGET_BUSYBOX) $(TARGET_DROPBEAR) $(TARGET_VULKANINFO)

.PHONY: userland
