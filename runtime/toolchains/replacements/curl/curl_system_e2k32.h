/* Forced-include shim for curl on e2k32 (ILP32), used by common/curl.mk.
 *
 * curl's include/curl/system.h has an explicit branch for the MCST lcc
 * (__LCC__/__MCST__) that unconditionally picks CURL_TYPEOF_CURL_OFF_T = long.
 * That is right for -m64 (lcc's default) but wrong for -m32, where long is
 * 4 bytes — lib/curl_setup.h then dies with "#error too small curl_off_t".
 *
 * Simply passing -U__LCC__ (which would route selection into the generic
 * __GNUC__ branch that reads __SIZEOF_LONG__) is not an option: glibc's
 * bits/floatn-common.h relies on __LCC__ to provide its `typedef float
 * _Float32`-style shims, and without them lcc cannot compile <stdlib.h>.
 *
 * So instead this header is force-included (-include) before everything
 * else. Its guard makes the real system.h a no-op, and the definitions
 * below replicate curl's generic __GNUC__/ILP32 branch for the e2k32 data
 * model (long=4, long long=8). e2k64 builds do not use this shim — the
 * MCST branch is correct there.
 */

#ifndef CURLINC_SYSTEM_H
#define CURLINC_SYSTEM_H

# define CURL_TYPEOF_CURL_OFF_T     long long
# define CURL_FORMAT_CURL_OFF_T     "lld"
# define CURL_FORMAT_CURL_OFF_TU    "llu"
# define CURL_SUFFIX_CURL_OFF_T     LL
# define CURL_SUFFIX_CURL_OFF_TU    ULL
# define CURL_TYPEOF_CURL_SOCKLEN_T socklen_t
# define CURL_PULL_SYS_TYPES_H      1
# define CURL_PULL_SYS_SOCKET_H     1

# ifdef CURL_PULL_SYS_TYPES_H
#  include <sys/types.h>
# endif
# ifdef CURL_PULL_SYS_SOCKET_H
#  include <sys/socket.h>
# endif

/* Data type definition of curl_socklen_t. */
typedef CURL_TYPEOF_CURL_SOCKLEN_T curl_socklen_t;

/* Data type definition of curl_off_t. */
typedef CURL_TYPEOF_CURL_OFF_T curl_off_t;

#endif /* CURLINC_SYSTEM_H */
