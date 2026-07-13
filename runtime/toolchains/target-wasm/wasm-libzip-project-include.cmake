# Injected via -DCMAKE_PROJECT_libzip_INCLUDE (common/libzip.mk WASM branch) so it runs
# at the END of libzip's project() call - AFTER the toolchain file has set
# CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY, and BEFORE libzip's check_function_exists
# / check_type_size calls. A plain -DCMAKE_TRY_COMPILE_TARGET_TYPE cannot do this: the
# toolchain file's set() is a normal variable that shadows the -D cache entry.
#
# libzip (unlike curl) does not raise the probe target type itself, so its function
# probes never link and report every symbol as present. Switch them to EXECUTABLE here;
# configure.mk links each probe against the sprt libc archive with NO --allow-undefined,
# so an absent function is a hard link error -> detected absent (compat.h falls back).
# This replaces the hand-maintained -DHAVE_MEMCPY_S/STRNCPY_S/ARC4RANDOM/... =OFF list.
#
# check_type_size keeps working under EXECUTABLE: --export-if-defined=main (configure.mk)
# keeps the probe's main - and the info_size marker it references - alive through
# gc-sections, so the size is still recovered from the freestanding wasm exe.
set(CMAKE_TRY_COMPILE_TARGET_TYPE "EXECUTABLE")
