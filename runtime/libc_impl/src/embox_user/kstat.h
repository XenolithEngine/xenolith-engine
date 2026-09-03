
// The wire form of stat, and the conversion into the libc's own.
//
// Three struct-stat layouts meet at this boundary and none of them agree
// (ABI doc section 4.1): Embox's, the ABI's `struct kstat`, and sprt's
// `struct __sprt_stat`. The kernel converts the first into the second; this
// converts the second into the third. Nothing may pass through untranslated --
// st_size sits at a different offset in all three.

#ifndef RUNTIME_LIBC_IMPL_SRC_EMBOX_USER_KSTAT_H_
#define RUNTIME_LIBC_IMPL_SRC_EMBOX_USER_KSTAT_H_

#include <sprt/c/sys/__sprt_stat.h>

namespace sprt {

// Mirrors `struct xl_kstat` in xenolith-os
// board/embox-qemu/drivers/xlsyscall/xl_abi.h. Fixed-width throughout: this is a
// wire format, not a C struct that happens to be shared.
struct __el0_kstat {
	__SPRT_ID(uint64_t) st_dev;
	__SPRT_ID(uint64_t) st_ino;
	__SPRT_ID(uint32_t) st_mode;
	__SPRT_ID(uint32_t) st_nlink;
	__SPRT_ID(uint32_t) st_uid;
	__SPRT_ID(uint32_t) st_gid;
	__SPRT_ID(uint64_t) st_rdev;
	__SPRT_ID(uint64_t) __pad1;
	__SPRT_ID(int64_t) st_size;
	__SPRT_ID(int32_t) st_blksize;
	__SPRT_ID(int32_t) __pad2;
	__SPRT_ID(int64_t) st_blocks;
	__SPRT_ID(int64_t) st_atime_sec;
	__SPRT_ID(uint64_t) st_atime_nsec;
	__SPRT_ID(int64_t) st_mtime_sec;
	__SPRT_ID(uint64_t) st_mtime_nsec;
	__SPRT_ID(int64_t) st_ctime_sec;
	__SPRT_ID(uint64_t) st_ctime_nsec;
	__SPRT_ID(uint32_t) __unused4;
	__SPRT_ID(uint32_t) __unused5;
};

// The same three assertions the kernel makes about its own copy. If one side is
// edited without the other, this is where it stops.
static_assert(sizeof(__el0_kstat) == 128, "struct stat is 128 bytes on the wire");
static_assert(__builtin_offsetof(__el0_kstat, st_size) == 48, "st_size at 48");
static_assert(__builtin_offsetof(__el0_kstat, st_atime_sec) == 72, "st_atime at 72");

inline void __el0_kstat_to_stat(const __el0_kstat &src, struct __SPRT_STAT_NAME *dst) {
	dst->st_dev = (__SPRT_ID(dev_t))src.st_dev;
	dst->st_ino = (__SPRT_ID(ino_t))src.st_ino;
	dst->st_nlink = (__SPRT_ID(nlink_t))src.st_nlink;
	dst->st_mode = (__SPRT_ID(mode_t))src.st_mode;
	dst->st_uid = (__SPRT_ID(uid_t))src.st_uid;
	dst->st_gid = (__SPRT_ID(gid_t))src.st_gid;
	dst->st_rdev = (__SPRT_ID(dev_t))src.st_rdev;
	dst->st_size = (__SPRT_ID(off_t))src.st_size;
	dst->st_blksize = (__SPRT_ID(blksize_t))src.st_blksize;
	dst->st_blocks = (__SPRT_ID(blkcnt_t))src.st_blocks;
	dst->st_atim.tv_sec = (__SPRT_ID(time_t))src.st_atime_sec;
	dst->st_atim.tv_nsec = (long)src.st_atime_nsec;
	dst->st_mtim.tv_sec = (__SPRT_ID(time_t))src.st_mtime_sec;
	dst->st_mtim.tv_nsec = (long)src.st_mtime_nsec;
	dst->st_ctim.tv_sec = (__SPRT_ID(time_t))src.st_ctime_sec;
	dst->st_ctim.tv_nsec = (long)src.st_ctime_nsec;
}

} // namespace sprt

#endif // RUNTIME_LIBC_IMPL_SRC_EMBOX_USER_KSTAT_H_
