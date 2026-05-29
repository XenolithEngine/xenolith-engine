#ifndef __LINUX__AIO_ABI_H
#define __LINUX__AIO_ABI_H

#define ALG_SET_KEY     	    1
#define ALG_SET_IV      	    2
#define ALG_SET_OP      	    3
#define ALG_OP_DECRYPT          0
#define ALG_OP_ENCRYPT          1

struct sockaddr_alg {
	uint16_t   salg_family;
	uint8_t    salg_type[14];
	uint32_t   salg_feat;
	uint32_t   salg_mask;
	uint8_t    salg_name[64];
};

struct af_alg_iv {
	uint32_t ivlen;
	uint8_t iv[0];
};

typedef long long __kernel_long_t;
typedef unsigned long long aio_context_t;

enum {
	IOCB_CMD_PREAD = 0,
	IOCB_CMD_PWRITE = 1,
	IOCB_CMD_FSYNC = 2,
	IOCB_CMD_FDSYNC = 3,
	IOCB_CMD_POLL = 5,
	IOCB_CMD_NOOP = 6,
	IOCB_CMD_PREADV = 7,
	IOCB_CMD_PWRITEV = 8,
};

#define IOCB_FLAG_RESFD		(1 << 0)
#define IOCB_FLAG_IOPRIO	(1 << 1)

struct io_event {
	uint64_t data;
	uint64_t obj;
	int64_t res;
	int64_t res2;
};

struct iocb {
	uint64_t aio_data;
	uint32_t aio_key;
	int32_t aio_rw_flags;
	uint16_t aio_lio_opcode;
	int16_t	aio_reqprio;
	uint32_t aio_fildes;
	uint64_t aio_buf;
	uint64_t aio_nbytes;
	uint64_t aio_offset;
	uint64_t aio_reserved2;
	uint32_t aio_flags;
	uint32_t aio_resfd;
};

#endif
