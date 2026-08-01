#!/usr/bin/env bash
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
#
# Собрать рантайм-rootfs из готовых sysroot'ов. Управляется из rootfs.mk.
#
# Производит base-rootfs НА АРКУ: бинари userland, транзитивное замыкание
# разделяемых библиотек, лоадер Vulkan с пустым каталогом ICD и минимальный /etc.
#
# Дерево намеренно без приложения, без борды и БЕЗ ДРАЙВЕРА GPU: драйвер зависит
# от железа, поэтому его .so и его ICD-манифест кладёт поверх сборщик образа в
# xenolith-os — туда же, куда inittab, fstab, init-скрипты борды, слот
# приложения и отладочный пароль.
#
# usage: mkrootfs.sh <target-sysroot> <userland-sysroot> <out-dir> \
#                    <readelf> <strip> <arch> <libgcc_s> <extra-lib-dir> \
#                    <locales>

set -euo pipefail
export LC_ALL=C

SYS=$1; UL=$2; OUT=$3; READELF=$4; STRIP=$5; ARCH=$6; LIBGCC_S=$7; EXTRA=$8
LOCALES=${9-}

# Два источника библиотек, в порядке поиска:
#   $EXTRA   usr/lib разложенного sysroot'а — там лежит то, что положил общий
#            набор сторонних библиотек: libdrm и разделяемые
#            libc++/libc++abi/libunwind. Именно против них линкуется
#            приложение, поэтому они ПЕРВЫЕ: в device-sysroot есть своя
#            сборочная копия libc++ с тем же SONAME.
#   $SYS/lib device-sysroot — libc, загрузчик, NSS-бэкенды, libvulkan, libcrypt.
#            (usr/lib в этом sysroot — симлинк на lib.)
LIBDIRS=("$EXTRA" "$SYS/lib")
missing=0

say() { echo "[rootfs] $*"; }

# ── скелет ────────────────────────────────────────────────────────────────────
rm -rf "$OUT"
mkdir -p "$OUT"/{bin,sbin,lib,etc,proc,sys,dev,tmp,run,root,opt/app,var/data} \
         "$OUT"/usr/{bin,sbin,lib} "$OUT"/usr/share/vulkan/icd.d

# Бинари x86_64 несут интерпретатор /lib64/ld-linux-x86-64.so.2, aarch64 —
# /lib/ld-linux-aarch64.so.1. Один симлинк закрывает обе арки без дублирования
# загрузчика.
ln -sfn lib "$OUT/lib64"

# ── бинари userland (ферма апплетов busybox, dropbear, vulkaninfo) ────────────
for d in bin sbin usr/bin usr/sbin; do
  [ -d "$UL/$d" ] || continue
  cp -a "$UL/$d/." "$OUT/$d/"
done
say "userland: $(find "$OUT/bin" "$OUT/sbin" "$OUT/usr/bin" "$OUT/usr/sbin" -type f | wc -l) binaries, $(find "$OUT/bin" "$OUT/sbin" "$OUT/usr/bin" "$OUT/usr/sbin" -type l | wc -l) applet links"

# /usr/share/vulkan/icd.d остаётся ПУСТЫМ намеренно: это тот каталог, который
# лоадер перечисляет в рантайме, и именно сюда сборщик образа кладёт манифест
# драйвера своей борды (library_path в манифесте — абсолютный девайсовый путь,
# поэтому сам драйвер кладётся в /usr/lib, а не в /lib).

# ── libgcc_s.so.1 ─────────────────────────────────────────────────────────────
# Шим из libs.mk: __gcc_personality_v0 плюс DT_NEEDED на libunwind.so.1. Живёт
# только здесь, в sysroot его нет намеренно (иначе clang мог бы подобрать его
# при линковке вместо compiler-rt). Кладётся ДО обхода замыкания, чтобы его
# зависимости разрешались общим правилом.
#
# Замыкание его не нашло бы само: на libgcc_s не объявляет NEEDED никто из
# наших бинарей — её грузит dlopen'ом сама glibc, для pthread_exit,
# pthread_cancel, backtrace() и раскрутки исключений через кадры libc.
cp -L "$LIBGCC_S" "$OUT/lib/libgcc_s.so.1"

# ── замыкание разделяемых библиотек ───────────────────────────────────────────
find_lib() {
  local d
  for d in "${LIBDIRS[@]}"; do
    if [ -e "$d/$1" ]; then printf '%s\n' "$d/$1"; return 0; fi
  done
  return 1
}

copy_lib() {
  local n=$1 src
  [ -e "$OUT/lib/$n" ] && return 0
  if ! src=$(find_lib "$n"); then
    echo "[rootfs] MISSING library: $n" >&2
    missing=1
    return 0
  fi
  cp -L "$src" "$OUT/lib/$n"
  walk "$OUT/lib/$n"
}

walk() {
  local need
  for need in $("$READELF" -d "$1" 2>/dev/null | sed -n 's/.*NEEDED.*\[\(.*\)\].*/\1/p'); do
    copy_lib "$need"
  done
}

# Обойти всё, что уже разложено.
while IFS= read -r -d '' f; do
  head -c4 "$f" 2>/dev/null | grep -q ELF && walk "$f" || true
done < <(find "$OUT" -type f -print0)

# Библиотеки, на которые никто не объявляет NEEDED, потому что их грузят через
# dlopen — обход замыкания их принципиально не находит. Разделены на
# обязательные и «если есть»: молча недоложенный рантайм даёт образ, который
# падает уже на устройстве.
#
# ОБЯЗАТЕЛЬНЫЕ:
#   libvulkan      dlopen'ится и vulkaninfo (VK_NO_PROTOTYPES), и движком
#                  (XLVkPlatformLinux.cc: sprt::Dso("libvulkan.so.1"))
#   libc++/abi/unwind
#                  рантайм приложения; база обязана нести их заранее, чтобы
#                  подмена приложения оставалась выкладкой одного файла. Едут из
#                  $EXTRA — из той же сборки, против которой линкуется движок.
#   libdrm         прямой KMS-рендер, нужен даже при GPU=none; тоже из $EXTRA
#   libnss_files/dns
#                  бэкенды NSS у glibc, грузятся на первом getpwnam() — без них
#                  dropbear не разрешит root и логин не пройдёт
for req in libvulkan.so.1 \
           libc++.so.1 libc++abi.so.1 libunwind.so.1 \
           libdrm.so.2 \
           libnss_files.so.2 libnss_dns.so.2 libresolv.so.2; do
  copy_lib "$req"
done

# «ЕСЛИ ЕСТЬ» — арх-зависимые или чисто forward-compat:
#   ld-*           сам интерпретатор; на арку существует ровно один из трёх
#   libmvec        есть на x86_64 и (с 2.39) aarch64, на riscv64 отсутствует
#   libnss_compat  собирается не всегда
#   libpthread/libdl/librt/libutil/libanl
#                  glibc 2.34 слила их в libc и оставила пустые compat-заглушки,
#                  поэтому ничто, собранное против ЭТОГО sysroot, их не
#                  потребует. Но бинари, собранные против более старой glibc,
#                  NEEDED на них несут. Несколько КБ за forward-compat.
for extra in ld-linux-aarch64.so.1 ld-linux-x86-64.so.2 ld-linux-riscv64-lp64d.so.1 \
             libmvec.so.1 libnss_compat.so.2 \
             libpthread.so.0 libdl.so.2 librt.so.1 libutil.so.1 libanl.so.1; do
  if find_lib "$extra" >/dev/null; then copy_lib "$extra"; fi
done

# Версионированным по SONAME файлам glibc нужны простые симлинки — для
# dlopen("libX.so") и для всего, что слинковано с неверсионированным именем.
( cd "$OUT/lib"
  for f in *.so.*; do
    base=${f%%.so.*}.so
    [ -e "$base" ] || ln -sf "$f" "$base"
  done )

say "libraries: $(ls "$OUT/lib" | grep -c '\.so') files"

# ── strip ─────────────────────────────────────────────────────────────────────
# Копии из sysroot несут полную отладочную информацию (одна glibc — ~500 МБ).
before=$(du -sm "$OUT" | cut -f1)
while IFS= read -r -d '' f; do
  if head -c4 "$f" 2>/dev/null | grep -q ELF; then
    "$STRIP" --strip-unneeded "$f" 2>/dev/null || true
  fi
done < <(find "$OUT" -type f -print0)
after=$(du -sm "$OUT" | cut -f1)
say "stripped: ${before} MB -> ${after} MB"

# ── /etc ──────────────────────────────────────────────────────────────────────
printf 'root:x:0:0:root:/root:/bin/sh\n'  > "$OUT/etc/passwd"
printf 'root:x:0:\n'                      > "$OUT/etc/group"

# root ЗАБЛОКИРОВАН (`*` = ни один пароль не подходит). Это переиспользуемый
# артефакт — вшить сюда отладочный пароль значило бы разослать его во все
# образы, собранные из него. Реальный хеш пишет сборщик образа.
printf 'root:*:19000:0:99999:7:::\n'      > "$OUT/etc/shadow"
chmod 600 "$OUT/etc/shadow"

# NSS: только files, кроме hosts. Соответствует набору libnss_*, разложенному выше.
cat > "$OUT/etc/nsswitch.conf" <<'EOF'
passwd:     files
group:      files
shadow:     files
hosts:      files dns
networks:   files
services:   files
protocols:  files
EOF

# ld.so.cache не генерируется: ldconfig — целевой бинарь, на хосте не
# запускается. Поэтому всё лежит в /lib и /usr/lib, которые загрузчик ищет по
# умолчанию и без кэша — тот сэкономил бы лишь несколько stat().
printf '/lib\n/usr/lib\n' > "$OUT/etc/ld.so.conf"

# /etc на устройстве только для чтения; udhcpc пишет реальный файл в /tmp.
ln -sfn /tmp/resolv.conf "$OUT/etc/resolv.conf"

printf '127.0.0.1 localhost\n' > "$OUT/etc/hosts"

# ── локали ────────────────────────────────────────────────────────────────────
# Скомпилировать локаль может ТОЛЬКО localedef той же glibc: формат бинарный и
# завязан на версию. Но «только целевой» не означает «нельзя» — целевой бинарь
# уже лежит в sysroot вместе с исходниками локалей (usr/share/i18n, ~17 МБ), и
# запустить его можно двумя способами:
#
#   арка == хостовой   напрямую через целевой ld.so, без всякой эмуляции;
#   арка чужая         через qemu-user.
#
# (Третий путь, если qemu не хочется в зависимостях хоста: собрать localedef под
# ХОСТ из того же дерева glibc и гонять с --little-endian/--big-endian — эти
# ключи есть в upstream. Формат локали — поток uint32, эндианность в нём
# единственный таргет-зависимый параметр. Здесь не сделано: лишняя host-сборка
# glibc ради того, что уже собрано.)
#
# В образ едет только скомпилированный результат (C.UTF-8 — ~410 КБ);
# usr/share/i18n НЕ копируется: он нужен, чтобы локали компилировать, а не чтобы
# ими пользоваться.
#
# Пустой SP_LOCALES — явный отказ от локалей. Молча их не потерять: без
# /usr/lib/locale setlocale(LC_ALL,"C.UTF-8") возвращает NULL и многобайтные
# функции на UTF-8 отказывают, а выглядит это как «просто C-локаль».
if [ -n "$LOCALES" ]; then
  localedef_bin="$SYS/usr/bin/localedef"
  i18n="$SYS/usr/share/i18n"
  ldso=$(ls "$SYS"/lib/ld-linux-*.so.* 2>/dev/null | head -1)
  host_arch=$(uname -m)

  runner=()
  if [ "$ARCH" = "$host_arch" ] && [ -n "$ldso" ]; then
    runner=("$ldso" --library-path "$SYS/lib")
    via="native (target ld.so)"
  elif command -v "qemu-$ARCH" >/dev/null 2>&1; then
    runner=("qemu-$ARCH" -L "$SYS")
    via="qemu-$ARCH"
  else
    echo "[rootfs] cannot run the target localedef: arch $ARCH != host $host_arch" >&2
    echo "[rootfs]   and qemu-$ARCH is not installed (package qemu-user / qemu-user-static)" >&2
    echo "[rootfs]   install it, or build with SP_LOCALES= to ship no locales at all" >&2
    exit 1
  fi

  [ -x "$localedef_bin" ] || { echo "[rootfs] no $localedef_bin in the sysroot" >&2; exit 1; }
  [ -d "$i18n" ] || { echo "[rootfs] no locale sources at $i18n" >&2; exit 1; }

  mkdir -p "$OUT/usr/lib/locale"
  for loc in $LOCALES; do
    # Имя вида <input>.<charmap>, как в localedata/SUPPORTED: C.UTF-8 собирается
    # из исходника C с картой UTF-8. localedef нормализует имя каталога
    # (C.UTF-8 -> C.utf8); setlocale находит его по любому написанию.
    case "$loc" in
      *.*) in_name=${loc%%.*}; charmap=${loc#*.} ;;
      *)   in_name=$loc;       charmap=UTF-8 ;;
    esac
    I18NPATH="$i18n" "${runner[@]}" "$localedef_bin" \
      -i "$in_name" -f "$charmap" --no-archive --prefix="$OUT" "$loc"
  done

  built=$(cd "$OUT/usr/lib/locale" && ls -1 2>/dev/null | tr '\n' ' ')
  [ -n "$built" ] || { echo "[rootfs] localedef produced nothing for '$LOCALES'" >&2; exit 1; }
  say "locales: $built via $via ($(du -sh "$OUT/usr/lib/locale" | cut -f1))"
else
  say "locales: none (SP_LOCALES empty)"
fi

# ── манифест ──────────────────────────────────────────────────────────────────
{
  echo "arch:      $ARCH"
  echo "sysroot:   $SYS"
  echo "userland:  $UL"
  echo "gpu:       none (driver overlay is board-specific, applied by the image builder)"
  echo "size:      $(du -sh "$OUT" | cut -f1)"
  echo
  echo "# gpu:    драйвер Vulkan сюда НЕ входит - он зависит от железа. Сборщик"
  echo "#         образа кладёт .so в /usr/lib и манифест в"
  echo "#         /usr/share/vulkan/icd.d, а также обязан сам довезти те"
  echo "#         разделяемые библиотеки, которые драйвер тянет по NEEDED и"
  echo "#         которых нет в списке ниже."
  echo "# locale: скомпилированные локали лежат в /usr/lib/locale (список ниже),"
  echo "#         исходники (usr/share/i18n) сюда НЕ копируются - они нужны для"
  echo "#         компиляции, а не для использования. Языковые локали сверх"
  echo "#         базовой докладывает образ; LANG/LC_ALL тоже выставляет он,"
  echo "#         иначе процессы стартуют в C независимо от наличия данных."
  echo "# libgcc: libgcc_s.so.1 - шим из libunwind + compiler-rt, не gcc-шный."
  echo "#         Нужен рантайму glibc (pthread_exit, backtrace, раскрутка"
  echo "#         исключений через кадры libc), линковаться с ним не следует."
  echo "# tz:     glibc собрана --disable-timezone-tools и tzdata не ставится,"
  echo "#         поэтому TZ разрешается в UTC. Реальную зону должен положить образ."
  echo
  echo "== locales =="
  ( cd "$OUT/usr/lib/locale" 2>/dev/null && ls -1 ) || echo "(none)"
  echo
  echo "== libraries =="
  ( cd "$OUT/lib" && ls -1 )
  echo
  echo "== binaries =="
  find "$OUT/bin" "$OUT/sbin" "$OUT/usr/bin" "$OUT/usr/sbin" -type f -printf '%P\n' | sort
} > "$OUT/rootfs.manifest"

if [ "$missing" -ne 0 ]; then
  echo "[rootfs] FAILED: unresolved libraries (see MISSING above)" >&2
  exit 1
fi

say "ready -> $OUT ($(du -sh "$OUT" | cut -f1))"
