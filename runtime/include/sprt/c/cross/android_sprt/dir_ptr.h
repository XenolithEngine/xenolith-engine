typedef struct DIR __SPRT_ID(DIR);

// should match dirent64 for target systems
struct __SPRT_DIRENT_NAME {
	__SPRT_ID(ino_t) d_ino;
	__SPRT_ID(off_t) d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[256];
};
