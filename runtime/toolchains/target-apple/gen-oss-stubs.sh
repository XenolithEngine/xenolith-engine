#!/bin/sh
# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio> — MIT (see open-sysroot.mk)
#
# gen-oss-stubs.sh DYLIB NM OUTROOT ARCHTARGET [EXTRA_SYMS.txt ...]
#
# Derive minimal Text-Based Dylib (.tbd) link stubs for a "+open" macOS sysroot
# straight from the runtime's OWN external references, so the runtime (and
# runtime_window) can be linked WITHOUT the proprietary Xcode SDK.
#
#   DYLIB          built libsprt.dylib to read undefined (imported) symbols from
#   NM             llvm-nm to use
#   OUTROOT        target sysroot root (tbds are written under usr/lib + Frameworks)
#   ARCHTARGET     tbd `targets:` value, e.g. arm64-macos / x86_64-macos
#   EXTRA_SYMS.txt curated symbol lists (target-apple/functions_<arch>.txt): extra
#                  symbols the bundled DEPENDENCIES (curl, openssl, freetype, ...)
#                  pull from the system beyond what a bare libsprt.dylib imports.
#                  One "        _symbol," per line (the same format appended to
#                  libsprt.tbd); leading space and trailing comma are stripped.
#
# The union (libsprt imports + curated lists) is attributed symbol-by-symbol to
# exactly one system library by name/prefix, then each library gets a .tbd
# exporting precisely those symbols (and no more). install-name fields use the
# canonical on-device paths so the recorded LC_LOAD_DYLIB dependencies are
# correct at run time — this mirrors what Apple's real libSystem.tbd / framework
# tbds provide, distilled down to just the symbols this target actually needs.
#
# Bootstrap: needs a libsprt.dylib built once (with the Xcode SDK). The stubs it
# emits then make every subsequent runtime / app link for the +open target
# SDK-free. The imported-symbol set is stable across builds.

set -eu

DYLIB="$1"; NM="$2"; OUT="$3"; ARCH="$4"
shift 4   # remaining args (if any) are curated EXTRA_SYMS.txt files

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# 1) collect the symbol set = libsprt's undefined (imported) symbols UNION the
#    curated dependency symbol lists, one per line, deduped. DYLIB may be a
#    space-separated list of dylibs (e.g. the x86_64 + arm64 libsprt) so a
#    dual-target tbd carries BOTH arches' symbol names — x86_64's $INODE64 /
#    $UNIX2003 variants AND arm64's plain names (the extra names per arch are
#    unreferenced there, hence harmless).
{
  for d in $DYLIB; do
    [ -f "$d" ] && "$NM" --undefined-only "$d" | awk '{print $NF}'
  done
  for f in "$@"; do
    [ -f "$f" ] || { echo "gen-oss-stubs: missing symbol list $f" >&2; continue; }
    # "        _symbol,"  ->  "_symbol"   (also strip the optional YAML quoting
    # around $-suffixed names: "        '_fopen$DARWIN_EXTSN',")
    sed -E "s/^[[:space:]]+//; s/[[:space:]]+\$//; s/,\$//; s/^'//; s/'\$//" "$f" | grep -E '^[._A-Za-z$]'
  done
} | sort -u > "$tmp/undef.txt.all"

# 1b) drop symbols already provided by the list-baked SDK-export stubs
#     (exports/*.txt: libiconv, libicucore, libedit, ...) — in the real SDK they
#     live in those libraries, NOT in libSystem, so they must never fall through
#     into the classified stubs as duplicates.
EXPDIR="$(cd "$(dirname "$0")" && pwd)/exports"
if [ -d "$EXPDIR" ]; then
  cat "$EXPDIR"/*.txt | grep -v '^#' | grep -v '^[[:space:]]*$' | sort -u > "$tmp/listed.txt"
  grep -vxF -f "$tmp/listed.txt" "$tmp/undef.txt.all" > "$tmp/undef.txt"
else
  cp "$tmp/undef.txt.all" "$tmp/undef.txt"
fi

# 2) classify each symbol -> library bucket (first match wins). NS ObjC classes
#    are split Foundation vs AppKit by an explicit, SDK-free name table; every
#    other attribution is a pure prefix rule.
awk '
{
  s = $0
  if      (s ~ /^_OBJC_(META)?CLASS_\$_UTType/ || s ~ /^_UTType/ || s ~ /^_kUT/)           b = "UniformTypeIdentifiers"
  else if (s ~ /^_OBJC_(META)?CLASS_\$_CA/)                                                 b = "QuartzCore"
  else if (s ~ /^_OBJC_(META)?CLASS_\$_(NSAnimationContext|NSApplication|NSBitmapImageRep|NSColor|NSColorPanel|NSColorSpace|NSCursor|NSEvent|NSFont|NSFontManager|NSFontPanel|NSImage|NSImageRep|NSOpenPanel|NSPanel|NSPasteboard|NSPasteboardItem|NSSavePanel|NSScreen|NSTrackingArea|NSView|NSViewController|NSWindow|NSWorkspace|NSRunningApplication)$/) b = "AppKit"
  # NS class OWNERSHIP mirrors the real SDK: the toll-free-bridged base classes
  # are CORE FOUNDATION objc-classes, NSObject is libobjc own; the rest Foundation.
  else if (s ~ /^_OBJC_(META)?CLASS_\$_NS(Array|Data|Date|Dictionary|MutableArray|MutableDictionary|RunLoop|URL)$/) b = "CoreFoundation"
  else if (s ~ /^_OBJC_(META)?CLASS_\$_NSObject$/)                                          b = "libobjc"
  else if (s ~ /^_OBJC_(META)?CLASS_\$_NS/)                                                 b = "Foundation"
  # non-class AppKit data symbols (SDK: AppKit exports). The window notifications are
  # AppKit-owned despite the plain _NS prefix, so they must be named before the generic
  # _NS<Upper> -> Foundation rule below catches them.
  else if (s == "_NSApp" || s == "_NSDeviceRGBColorSpace" || s ~ /^_NSPasteboardType/ || s ~ /^_NSWindow.*Notification$/)    b = "AppKit"
  # SDK quirk: _NSRunLoopCommonModes is a CORE FOUNDATION export
  else if (s == "_NSRunLoopCommonModes")                                                    b = "CoreFoundation"
  # remaining _NS<Upper> functions/constants are Foundation exports
  # (NSLog, NSClassFromString, NSPointInRect, NSTemporaryDirectory, ...);
  # __NSGet* (double underscore: _NSGetExecutablePath etc.) stays libSystem.
  else if (s ~ /^_NS[A-Z]/)                                                                 b = "Foundation"
  else if (s ~ /^_Authorization/ || s == "_SessionGetInfo")                                 b = "Security"
  else if (s ~ /^_Sec/ || s ~ /^_kSec/)                                                     b = "Security"
  else if (s ~ /^_CF/ || s ~ /^_kCF/ || s == "___CFConstantStringClassReference")           b = "CoreFoundation"
  else if (s ~ /^_CT/ || s ~ /^_kCT/)                                                       b = "CoreText"
  else if (s ~ /^_CG/ || s ~ /^_kCG/)                                                       b = "CoreGraphics"
  else if (s ~ /^_nw_/)                                                                      b = "Network"
  else if (s ~ /^_IO/ || s ~ /^_kIO/)                                                        b = "IOKit"
  # LaunchServices / FSEvents / AppleEvents live in the CoreServices umbrella; without this
  # they fall through to libSystem and dyld cannot find them at run time
  else if (s ~ /^_FSEventStream/ || s ~ /^_LS[A-Z]/ || s ~ /^_AE[A-Z]/)                      b = "CoreServices"
  else if (s ~ /^_SC/ || s ~ /^_kSC/)                                                        b = "SystemConfiguration"
  else if (s ~ /^_objc_/ || s ~ /^_class_/ || s ~ /^_object_/ || s ~ /^_sel_/ || s ~ /^_method_/ || s ~ /^_ivar_/ || s ~ /^_protocol_/ || s ~ /^__objc/ || s ~ /^_property_/ || s ~ /^_imp_/) b = "libobjc"
  else if (s == "___cxa_atexit" || s == "___cxa_thread_atexit")                             b = "libSystem"
  # _Unwind_* are libSystem exports on macOS (the real libSystem reexports libunwind).
  else if (s ~ /^__Unwind/ || s ~ /^_Unwind/)                                               b = "libSystem"
  else if (s ~ /^___cxa/ || s ~ /^__ZT/ || s ~ /^__Zn/ || s ~ /^__Zd/ || s ~ /^__ZSt/ || s ~ /^___gxx_personality/ || s == "___dynamic_cast") b = "cxxabi-skip"
  else if (s == "___divtc3" || s == "___multc3")                                            b = "rtlib-skip"
  else                                                                                       b = "libSystem"
  print b "\t" s
}
' "$tmp/undef.txt" > "$tmp/classified.txt"

# 3) emit one .tbd per library that has at least one referenced symbol.
#    args: bucket  relative-output-path  install-name  [reexported-library-path]
# NOT baked here (curated/generated elsewhere, a bake must never clobber them):
#   libm/libiconv/libicucore/libedit/libncurses/libpanel tbds (SDK export lists),
#   libc++/libc++abi/libunwind tbds (llvm-readtapi in libcxx.mk), libpthread/libc/
#   libdl + versioned aliases (symlinks), CoreServices/Metal/IOSurface framework
#   tbds (hand-curated), Versions/A symlinks.
emit() {
  bucket="$1"; rel="$2"; iname="$3"; reexp="${4:-}"
  n="$(awk -F'\t' -v b="$bucket" '$1==b' "$tmp/classified.txt" | wc -l | tr -d ' ')"
  [ "$n" -gt 0 ] || { echo "  skip  $bucket (0 symbols)"; return 0; }
  out="$OUT/$rel"
  mkdir -p "$(dirname "$out")"
  {
    echo '--- !tapi-tbd'
    echo 'tbd-version:     4'
    echo "targets:         [ $ARCH ]"
    echo "install-name:    '$iname'"
    echo 'current-version: 0'
    echo 'compatibility-version: 0'
    if [ -n "$reexp" ]; then
      echo 'reexported-libraries:'
      echo "  - targets:         [ $ARCH ]"
      echo "    libraries:       [ $reexp ]"
    fi
    echo 'exports:'
    echo "  - targets:         [ $ARCH ]"
    echo '    symbols:         ['
    awk -F'\t' -v b="$bucket" '$1==b {print "        " $2 ","}' "$tmp/classified.txt"
    echo '    ]'
    echo '...'
  } > "$out"
  echo "  $bucket: $n symbols -> $rel"
}

FW='System/Library/Frameworks'
emit libSystem              usr/lib/libSystem.tbd    /usr/lib/libSystem.B.dylib
emit libobjc               usr/lib/libobjc.tbd      /usr/lib/libobjc.A.dylib
emit CoreFoundation         "$FW/CoreFoundation.framework/CoreFoundation.tbd"                 /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation
# Foundation reexports libobjc AND CoreFoundation (SDK parity): objc_msgSend & co
# and the toll-free-bridged CF-owned classes (NSArray/NSDictionary/...) resolve
# through `-framework Foundation` alone. The versioned libobjc.A.tbd alias and
# CoreFoundation.framework/Versions/A symlink make the recorded reexport paths
# resolvable inside the sysroot.
emit Foundation             "$FW/Foundation.framework/Foundation.tbd"                         /System/Library/Frameworks/Foundation.framework/Versions/C/Foundation \
  "'/usr/lib/libobjc.A.dylib', '/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation'"
emit AppKit                 "$FW/AppKit.framework/AppKit.tbd"                                 /System/Library/Frameworks/AppKit.framework/Versions/C/AppKit
emit QuartzCore             "$FW/QuartzCore.framework/QuartzCore.tbd"                         /System/Library/Frameworks/QuartzCore.framework/Versions/A/QuartzCore
emit Security               "$FW/Security.framework/Security.tbd"                             /System/Library/Frameworks/Security.framework/Versions/A/Security
emit UniformTypeIdentifiers "$FW/UniformTypeIdentifiers.framework/UniformTypeIdentifiers.tbd" /System/Library/Frameworks/UniformTypeIdentifiers.framework/Versions/A/UniformTypeIdentifiers
emit CoreGraphics           "$FW/CoreGraphics.framework/CoreGraphics.tbd"                     /System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics
emit CoreText               "$FW/CoreText.framework/CoreText.tbd"                             /System/Library/Frameworks/CoreText.framework/Versions/A/CoreText
emit Network                "$FW/Network.framework/Network.tbd"                              /System/Library/Frameworks/Network.framework/Versions/A/Network
emit IOKit                  "$FW/IOKit.framework/IOKit.tbd"                                   /System/Library/Frameworks/IOKit.framework/Versions/A/IOKit
emit SystemConfiguration    "$FW/SystemConfiguration.framework/SystemConfiguration.tbd"       /System/Library/Frameworks/SystemConfiguration.framework/Versions/A/SystemConfiguration

# 4) list-driven SDK-export stubs: each exports/<name>.txt describes ONE usr/lib
#    tbd — a `# tbd: install-name=... current-version=... compatibility-version=...`
#    directive followed by the export symbols (one per line, SDK-derived, order
#    preserved). These are libraries whose export sets do NOT come from libsprt
#    imports (libiconv/libicucore/libedit/libncurses/libpanel + the empty libm
#    `-lm` satisfier); the versioned-alias SYMLINKS beside them are git-tracked
#    and untouched here.
EXPDIR="$(cd "$(dirname "$0")" && pwd)/exports"
if [ -d "$EXPDIR" ]; then
  for lf in "$EXPDIR"/*.txt; do
    name="$(basename "$lf" .txt)"
    hdr="$(grep -m1 '^# tbd:' "$lf")" || { echo "gen-oss-stubs: $lf lacks '# tbd:' directive" >&2; exit 1; }
    iname="$(echo "$hdr"  | sed -E 's/.*install-name=([^ ]+).*/\1/')"
    curv="$(echo "$hdr"   | sed -E 's/.*current-version=([^ ]+).*/\1/')"
    compv="$(echo "$hdr"  | sed -E 's/.*compatibility-version=([^ ]+).*/\1/')"
    out="$OUT/usr/lib/$name.tbd"
    mkdir -p "$(dirname "$out")"
    nsym="$(grep -v '^#' "$lf" | grep -c -v '^[[:space:]]*$' || true)"
    {
      echo '--- !tapi-tbd'
      echo 'tbd-version:     4'
      echo "targets:         [ $ARCH ]"
      echo "install-name:    '$iname'"
      echo "current-version: $curv"
      echo "compatibility-version: $compv"
      if [ "$nsym" -gt 0 ]; then
        echo 'exports:'
        echo "  - targets:         [ $ARCH ]"
        echo '    symbols:         ['
        grep -v '^#' "$lf" | grep -v '^[[:space:]]*$' | sed -E "s/^(.*\\\$.*)\$/'\1'/; s/^/        /; s/\$/,/"
        echo '    ]'
      fi
      echo '...'
    } > "$out"
    echo "  $name: $nsym symbols -> usr/lib/$name.tbd (SDK export list)"
  done
fi

# 5) report any symbol we could not attribute to a shipped stub (should be none)
echo "  total imported: $(wc -l < "$tmp/undef.txt" | tr -d ' ')"
