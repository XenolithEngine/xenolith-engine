__SPRT_ID(dev_t) st_dev;
__SPRT_ID(ino_t) st_ino;
__SPRT_ID(nlink_t) st_nlink;

__SPRT_ID(mode_t) st_mode;
__SPRT_ID(uid_t) st_uid;
__SPRT_ID(gid_t) st_gid;
__SPRT_ID(dev_t) st_rdev;
__SPRT_ID(off_t) st_size;
__SPRT_ID(blksize_t) st_blksize;
__SPRT_ID(blkcnt_t) st_blocks;

struct __SPRT_TIMESPEC_NAME st_atim;
struct __SPRT_TIMESPEC_NAME st_mtim;
struct __SPRT_TIMESPEC_NAME st_ctim;
