typedef struct __SPRT_DIR __SPRT_ID(DIR);

// should match dirent64 for target systems
struct __SPRT_DIRENT_NAME {
	__SPRT_ID(uint64_t) d_ino;
	__SPRT_ID(uint64_t) d_seekoff;
	__SPRT_ID(uint16_t) d_reclen;
	__SPRT_ID(uint16_t) d_namlen;
	__SPRT_ID(uint8_t) d_type;
	char d_name[1'024];
};
