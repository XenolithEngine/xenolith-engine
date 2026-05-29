// clang-format off
#define __SPRT_O_NONBLOCK      0x00000004      /* no delay */
#define __SPRT_O_APPEND        0x00000008      /* set append mode */

#define __SPRT_O_SYNC                  0x0080

#define __SPRT_O_SHLOCK        0x00000010      /* open with shared file lock */
#define __SPRT_O_EXLOCK        0x00000020      /* open with exclusive file lock */
#define __SPRT_O_ASYNC         0x00000040      /* signal pgrp when data ready */
#define __SPRT_O_FSYNC         __SPRT_O_SYNC          /* source compatibility: do not use */
#define __SPRT_O_NOFOLLOW      0x00000100      /* don't follow symlinks */
#define __SPRT_O_CREAT         0x00000200      /* create if nonexistant */
#define __SPRT_O_TRUNC         0x00000400      /* truncate to zero length */
#define __SPRT_O_EXCL          0x00000800      /* error if already exists */
#define __SPRT_O_RESOLVE_BENEATH 0x00001000    /* only for open(2), same value as FMARK */
#define __SPRT_O_UNIQUE        0x00002000      /* only for open(2), same value as FDEFER */
#define __SPRT_O_EVTONLY       0x00008000      /* descriptor requested for event notifications only */
#define __SPRT_O_NOCTTY        0x00020000      /* don't assign controlling terminal */
#define __SPRT_O_DIRECTORY     0x00100000
#define __SPRT_O_SYMLINK       0x00200000      /* allow open of a symlink */
#define __SPRT_O_DSYNC                 0x400000
#define __SPRT_O_CLOEXEC       0x01000000      /* implicitly set FD_CLOEXEC */
#define __SPRT_O_EXEC          0x40000000               /* open file for execute only */
#define __SPRT_O_SEARCH        (__SPRT_O_EXEC | __SPRT_O_DIRECTORY)   /* open directory for search only */
#define __SPRT_O_NDELAY __SPRT_O_NONBLOCK

#define __SPRT_O_ACCMODE       0x0003
#define __SPRT_O_RDONLY        0x0000          /* open for reading only */
#define __SPRT_O_WRONLY        0x0001          /* open for writing only */
#define __SPRT_O_RDWR          0x0002          /* open for reading and writing */
// clang-format on

#define __SPRT_F_DUPFD         0               /* duplicate file descriptor */
#define __SPRT_F_GETFD         1               /* get file descriptor flags */
#define __SPRT_F_SETFD         2               /* set file descriptor flags */
#define __SPRT_F_GETFL         3               /* get file status flags */
#define __SPRT_F_SETFL         4               /* set file status flags */
#define __SPRT_F_GETOWN        5               /* get SIGIO/SIGURG proc/pgrp */
#define __SPRT_F_SETOWN        6               /* set SIGIO/SIGURG proc/pgrp */
#define __SPRT_F_GETLK         7               /* get record locking information */
#define __SPRT_F_SETLK         8               /* set record locking information */
#define __SPRT_F_SETLKW        9               /* __SPRT_F_SETLK; wait if blocked */

#define __SPRT_F_DUPFD_CLOEXEC         67      /* mark the dup with FD_CLOEXEC */

#define __SPRT_F_OFD_SETLK             90      /* Acquire or release open file description lock */
#define __SPRT_F_OFD_SETLKW            91      /* (as F_OFD_SETLK but blocking if conflicting lock) */
#define __SPRT_F_OFD_GETLK             92      /* Examine OFD lock */

#define __SPRT_F_RDLCK         1               /* shared or read lock */
#define __SPRT_F_UNLCK         2               /* unlock */
#define __SPRT_F_WRLCK         3               /* exclusive or write lock */
#define __SPRT_F_WAIT          0x010           /* Wait until lock is granted */
#define __SPRT_F_FLOCK         0x020           /* Use flock(2) semantics for lock */
#define __SPRT_F_POSIX         0x040           /* Use POSIX semantics for lock */
#define __SPRT_F_PROV          0x080           /* Non-coalesced provisional lock */
#define __SPRT_F_WAKE1_SAFE    0x100           /* its safe to only wake one waiter */
#define __SPRT_F_ABORT         0x200           /* lock attempt aborted (force umount) */
#define __SPRT_F_OFD_LOCK      0x400           /* Use "OFD" semantics for lock */
#define __SPRT_F_TRANSFER      0x800           /* Transfer the lock to new proc */
#define __SPRT_F_CONFINED      0x1000          /* fileglob cannot leave curproc */

#define __SPRT_FD_CLOEXEC      1               /* close-on-exec flag */

#define __SPRT_AT_FDCWD        -2
#define __SPRT_AT_EACCESS              0x0010  /* Use effective ids in access check */
#define __SPRT_AT_SYMLINK_NOFOLLOW     0x0020  /* Act on the symlink itself not the target */
#define __SPRT_AT_SYMLINK_FOLLOW       0x0040  /* Act on target of symlink */
#define __SPRT_AT_REMOVEDIR            0x0080  /* Path refers to directory */
#define __SPRT_AT_REALDEV              0x0200  /* Return real device inodes resides on for fstatat(2) */
#define __SPRT_AT_FDONLY               0x0400  /* Use only the fd and Ignore the path for fstatat(2) */
#define __SPRT_AT_SYMLINK_NOFOLLOW_ANY 0x0800  /* Path should not contain any symlinks */
#define __SPRT_AT_RESOLVE_BENEATH      0x2000  /* Path must reside in the hierarchy beneath the starting directory */
#define __SPRT_AT_NODELETEBUSY         0x4000  /* Don't delete busy files */
#define __SPRT_AT_UNIQUE               0x8000  /* prevent a path lookup from succeeding on a vnode with multiple links */
