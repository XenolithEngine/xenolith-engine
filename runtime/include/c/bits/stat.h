#ifndef CORE_RUNTIME_INCLUDE_C_BITS_STAT_H_
#define CORE_RUNTIME_INCLUDE_C_BITS_STAT_H_

#include <c/bits/__sprt_time_t.h>
#include <c/bits/__sprt_ssize_t.h>
#include <c/cross/__sprt_fstypes.h>
#include <c/cross/__sprt_sysid.h>


#ifdef __SPRT_BUILD
#define __SPRT_STAT_NAME __SPRT_ID(stat)
#else
#define __SPRT_STAT_NAME stat
#endif

struct __SPRT_STAT_NAME {
	__SPRT_ID(dev_t) st_dev;
	__SPRT_ID(ino_t) st_ino;
	__SPRT_ID(nlink_t) st_nlink;

	__SPRT_ID(mode_t) st_mode;
	__SPRT_ID(uid_t) st_uid;
	__SPRT_ID(gid_t) st_gid;
	unsigned int __pad0;
	__SPRT_ID(dev_t) st_rdev;
	__SPRT_ID(off_t) st_size;
	__SPRT_ID(blksize_t) st_blksize;
	__SPRT_ID(blkcnt_t) st_blocks;

	__SPRT_TIMESPEC_NAME st_atim;
	__SPRT_TIMESPEC_NAME st_mtim;
	__SPRT_TIMESPEC_NAME st_ctim;
	long unused[3];
};

#endif // CORE_RUNTIME_INCLUDE_C_BITS_STAT_H_
