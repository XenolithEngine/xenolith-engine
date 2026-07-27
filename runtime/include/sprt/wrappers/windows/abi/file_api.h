/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_FILE_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_FILE_API_H_


#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>

// clang-format off
#define __SPRT_CREATE_NEW          1
#define __SPRT_CREATE_ALWAYS       2
#define __SPRT_OPEN_EXISTING       3
#define __SPRT_OPEN_ALWAYS         4
#define __SPRT_TRUNCATE_EXISTING   5

#define __SPRT_INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#define __SPRT_INVALID_SET_FILE_POINTER ((DWORD)-1)
#define __SPRT_INVALID_FILE_ATTRIBUTES ((DWORD)-1)

#define __SPRT_FILE_READ_DATA            ( 0x0001 )    // file & pipe
#define __SPRT_FILE_LIST_DIRECTORY       ( 0x0001 )    // directory
#define __SPRT_FILE_WRITE_DATA           ( 0x0002 )    // file & pipe
#define __SPRT_FILE_ADD_FILE             ( 0x0002 )    // directory
#define __SPRT_FILE_APPEND_DATA          ( 0x0004 )    // file
#define __SPRT_FILE_ADD_SUBDIRECTORY     ( 0x0004 )    // directory
#define __SPRT_FILE_CREATE_PIPE_INSTANCE ( 0x0004 )    // named pipe
#define __SPRT_FILE_READ_EA              ( 0x0008 )    // file & directory
#define __SPRT_FILE_WRITE_EA             ( 0x0010 )    // file & directory
#define __SPRT_FILE_EXECUTE              ( 0x0020 )    // file
#define __SPRT_FILE_TRAVERSE             ( 0x0020 )    // directory
#define __SPRT_FILE_DELETE_CHILD         ( 0x0040 )    // directory
#define __SPRT_FILE_READ_ATTRIBUTES      ( 0x0080 )    // all
#define __SPRT_FILE_WRITE_ATTRIBUTES     ( 0x0100 )    // all

#define __SPRT_FILE_ALL_ACCESS (__SPRT_STANDARD_RIGHTS_REQUIRED | __SPRT_SYNCHRONIZE | 0x1FF)

#define __SPRT_FILE_GENERIC_READ   (__SPRT_STANDARD_RIGHTS_READ | __SPRT_FILE_READ_DATA | \
	__SPRT_FILE_READ_ATTRIBUTES | __SPRT_FILE_READ_EA | __SPRT_SYNCHRONIZE)

#define __SPRT_FILE_GENERIC_WRITE  (__SPRT_STANDARD_RIGHTS_WRITE | __SPRT_FILE_WRITE_DATA | \
	__SPRT_FILE_WRITE_ATTRIBUTES | __SPRT_FILE_WRITE_EA | __SPRT_FILE_APPEND_DATA | __SPRT_SYNCHRONIZE)

#define __SPRT_FILE_GENERIC_EXECUTE (__SPRT_STANDARD_RIGHTS_EXECUTE | __SPRT_FILE_READ_ATTRIBUTES | \
	__SPRT_FILE_EXECUTE | __SPRT_SYNCHRONIZE)

#define __SPRT_FILE_SHARE_READ                 0x00000001  
#define __SPRT_FILE_SHARE_WRITE                0x00000002  
#define __SPRT_FILE_SHARE_DELETE               0x00000004  

/* File attributes */
#define __SPRT_FILE_ATTRIBUTE_READONLY             0x00000001
#define __SPRT_FILE_ATTRIBUTE_HIDDEN               0x00000002
#define __SPRT_FILE_ATTRIBUTE_SYSTEM               0x00000004
#define __SPRT_FILE_ATTRIBUTE_DIRECTORY            0x00000010
#define __SPRT_FILE_ATTRIBUTE_ARCHIVE              0x00000020
#define __SPRT_FILE_ATTRIBUTE_DEVICE               0x00000040
#define __SPRT_FILE_ATTRIBUTE_NORMAL               0x00000080
#define __SPRT_FILE_ATTRIBUTE_TEMPORARY            0x00000100
#define __SPRT_FILE_ATTRIBUTE_SPARSE_FILE          0x00000200
#define __SPRT_FILE_ATTRIBUTE_REPARSE_POINT        0x00000400
#define __SPRT_FILE_ATTRIBUTE_COMPRESSED           0x00000800
#define __SPRT_FILE_ATTRIBUTE_OFFLINE              0x00001000
#define __SPRT_FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  0x00002000
#define __SPRT_FILE_ATTRIBUTE_ENCRYPTED            0x00004000
#define __SPRT_FILE_ATTRIBUTE_INTEGRITY_STREAM     0x00008000
#define __SPRT_FILE_ATTRIBUTE_VIRTUAL              0x00010000
#define __SPRT_FILE_ATTRIBUTE_NO_SCRUB_DATA        0x00020000
#define __SPRT_FILE_ATTRIBUTE_EA                   0x00040000
#define __SPRT_FILE_ATTRIBUTE_PINNED               0x00080000
#define __SPRT_FILE_ATTRIBUTE_UNPINNED             0x00100000
#define __SPRT_FILE_ATTRIBUTE_RECALL_ON_OPEN       0x00040000
#define __SPRT_FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000

#define __SPRT_FILE_VOLUME_IS_COMPRESSED           0x00008000  
#define __SPRT_FILE_SUPPORTS_OBJECT_IDS            0x00010000  
#define __SPRT_FILE_SUPPORTS_ENCRYPTION            0x00020000  
#define __SPRT_FILE_NAMED_STREAMS                  0x00040000  
#define __SPRT_FILE_READ_ONLY_VOLUME               0x00080000  
#define __SPRT_FILE_SEQUENTIAL_WRITE_ONCE          0x00100000  
#define __SPRT_FILE_SUPPORTS_TRANSACTIONS          0x00200000  
#define __SPRT_FILE_SUPPORTS_HARD_LINKS            0x00400000  
#define __SPRT_FILE_SUPPORTS_EXTENDED_ATTRIBUTES   0x00800000  
#define __SPRT_FILE_SUPPORTS_OPEN_BY_FILE_ID       0x01000000  
#define __SPRT_FILE_SUPPORTS_USN_JOURNAL           0x02000000  
#define __SPRT_FILE_SUPPORTS_INTEGRITY_STREAMS     0x04000000  
#define __SPRT_FILE_SUPPORTS_BLOCK_REFCOUNTING     0x08000000  
#define __SPRT_FILE_SUPPORTS_SPARSE_VDL            0x10000000  
#define __SPRT_FILE_DAX_VOLUME                     0x20000000  
#define __SPRT_FILE_SUPPORTS_GHOSTING              0x40000000 

/* File flags for CreateFileW (winbase.h) */
#define __SPRT_FILE_FLAG_WRITE_THROUGH         0x80000000
#define __SPRT_FILE_FLAG_OVERLAPPED            0x40000000
#define __SPRT_FILE_FLAG_NO_BUFFERING          0x20000000
#define __SPRT_FILE_FLAG_RANDOM_ACCESS         0x10000000
#define __SPRT_FILE_FLAG_SEQUENTIAL_SCAN       0x08000000
#define __SPRT_FILE_FLAG_DELETE_ON_CLOSE       0x04000000
#define __SPRT_FILE_FLAG_BACKUP_SEMANTICS      0x02000000
#define __SPRT_FILE_FLAG_POSIX_SEMANTICS       0x01000000
#define __SPRT_FILE_FLAG_SESSION_AWARE         0x00800000
#define __SPRT_FILE_FLAG_OPEN_REPARSE_POINT    0x00200000
#define __SPRT_FILE_FLAG_OPEN_NO_RECALL        0x00100000
#define __SPRT_FILE_FLAG_FIRST_PIPE_INSTANCE   0x00080000


#define __SPRT_IO_REPARSE_TAG_RESERVED_INVALID         (0xC0008000L)       
#define __SPRT_IO_REPARSE_TAG_MOUNT_POINT              (0xA0000003L)       
#define __SPRT_IO_REPARSE_TAG_HSM                      (0xC0000004L)       
#define __SPRT_IO_REPARSE_TAG_HSM2                     (0x80000006L)       
#define __SPRT_IO_REPARSE_TAG_SIS                      (0x80000007L)       
#define __SPRT_IO_REPARSE_TAG_WIM                      (0x80000008L)       
#define __SPRT_IO_REPARSE_TAG_CSV                      (0x80000009L)       
#define __SPRT_IO_REPARSE_TAG_DFS                      (0x8000000AL)       
#define __SPRT_IO_REPARSE_TAG_SYMLINK                  (0xA000000CL)       
#define __SPRT_IO_REPARSE_TAG_DFSR                     (0x80000012L)       
#define __SPRT_IO_REPARSE_TAG_DEDUP                    (0x80000013L)       
#define __SPRT_IO_REPARSE_TAG_NFS                      (0x80000014L)       
#define __SPRT_IO_REPARSE_TAG_FILE_PLACEHOLDER         (0x80000015L)       
#define __SPRT_IO_REPARSE_TAG_WOF                      (0x80000017L)       
#define __SPRT_IO_REPARSE_TAG_WCI                      (0x80000018L)       
#define __SPRT_IO_REPARSE_TAG_WCI_1                    (0x90001018L)       
#define __SPRT_IO_REPARSE_TAG_GLOBAL_REPARSE           (0xA0000019L)       
#define __SPRT_IO_REPARSE_TAG_CLOUD                    (0x9000001AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_1                  (0x9000101AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_2                  (0x9000201AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_3                  (0x9000301AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_4                  (0x9000401AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_5                  (0x9000501AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_6                  (0x9000601AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_7                  (0x9000701AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_8                  (0x9000801AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_9                  (0x9000901AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_A                  (0x9000A01AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_B                  (0x9000B01AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_C                  (0x9000C01AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_D                  (0x9000D01AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_E                  (0x9000E01AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_F                  (0x9000F01AL)       
#define __SPRT_IO_REPARSE_TAG_CLOUD_MASK               (0x0000F000L)       
#define __SPRT_IO_REPARSE_TAG_APPEXECLINK              (0x8000001BL)       
#define __SPRT_IO_REPARSE_TAG_PROJFS                   (0x9000001CL)       
#define __SPRT_IO_REPARSE_TAG_STORAGE_SYNC             (0x8000001EL)       
#define __SPRT_IO_REPARSE_TAG_WCI_TOMBSTONE            (0xA000001FL)       
#define __SPRT_IO_REPARSE_TAG_UNHANDLED                (0x80000020L)       
#define __SPRT_IO_REPARSE_TAG_ONEDRIVE                 (0x80000021L)       
#define __SPRT_IO_REPARSE_TAG_PROJFS_TOMBSTONE         (0xA0000022L)       
#define __SPRT_IO_REPARSE_TAG_AF_UNIX                  (0x80000023L)       
#define __SPRT_IO_REPARSE_TAG_STORAGE_SYNC_FOLDER      (0x90000027L)       
#define __SPRT_IO_REPARSE_TAG_WCI_LINK                 (0xA0000027L)       
#define __SPRT_IO_REPARSE_TAG_WCI_LINK_1               (0xA0001027L)       
#define __SPRT_IO_REPARSE_TAG_DATALESS_CIM             (0xA0000028L)       

#define __SPRT_MOVEFILE_REPLACE_EXISTING       0x00000001
#define __SPRT_MOVEFILE_COPY_ALLOWED           0x00000002
#define __SPRT_MOVEFILE_DELAY_UNTIL_REBOOT     0x00000004
#define __SPRT_MOVEFILE_WRITE_THROUGH          0x00000008
#define __SPRT_MOVEFILE_CREATE_HARDLINK        0x00000010
#define __SPRT_MOVEFILE_FAIL_IF_NOT_TRACKABLE  0x00000020

/* File move flags */
#define __SPRT_FILE_BEGIN                  0
#define __SPRT_FILE_CURRENT                1
#define __SPRT_FILE_END                    2

/* Final path name flags */
#define __SPRT_FILE_NAME_NORMALIZED        0x00000000
#define __SPRT_FILE_NAME_OPENED            0x00000008

#define __SPRT_FILE_MAP_WRITE            __SPRT_SECTION_MAP_WRITE
#define __SPRT_FILE_MAP_READ             __SPRT_SECTION_MAP_READ
#define __SPRT_FILE_MAP_ALL_ACCESS       __SPRT_SECTION_ALL_ACCESS

#define __SPRT_FILE_MAP_EXECUTE          __SPRT_SECTION_MAP_EXECUTE_EXPLICIT  // not included in FILE_MAP_ALL_ACCESS

#define __SPRT_FILE_MAP_COPY             0x00000001

#define __SPRT_FILE_MAP_RESERVE          0x80000000
#define __SPRT_FILE_MAP_TARGETS_INVALID  0x40000000
#define __SPRT_FILE_MAP_LARGE_PAGES      0x20000000

#define __SPRT_SYMBOLIC_LINK_FLAG_DIRECTORY                    (0x1)
#define __SPRT_SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE    (0x2)

#define __SPRT_SEC_HUGE_PAGES              0x00020000  
#define __SPRT_SEC_LARGE_PAGES             0x80000000

#define __SPRT_LOCKFILE_FAIL_IMMEDIATELY   0x00000001
#define __SPRT_LOCKFILE_EXCLUSIVE_LOCK     0x00000002

// Named-pipe open/mode flags (anonymous CreatePipe handles cannot do overlapped I/O, so an
// overlapped reader must build its pipe from CreateNamedPipe + CreateFile).
#define __SPRT_PIPE_ACCESS_INBOUND   0x00000001
#define __SPRT_PIPE_ACCESS_OUTBOUND  0x00000002
#define __SPRT_PIPE_ACCESS_DUPLEX    0x00000003
#define __SPRT_PIPE_TYPE_BYTE        0x00000000
#define __SPRT_PIPE_READMODE_BYTE    0x00000000
#define __SPRT_PIPE_WAIT             0x00000000

// ---- ReadDirectoryChangesW ([winbase] directory change notifications) --------
#define __SPRT_FILE_ACTION_ADDED 0x00000001
#define __SPRT_FILE_ACTION_REMOVED 0x00000002
#define __SPRT_FILE_ACTION_MODIFIED 0x00000003
#define __SPRT_FILE_ACTION_RENAMED_OLD_NAME 0x00000004
#define __SPRT_FILE_ACTION_RENAMED_NEW_NAME 0x00000005

#define __SPRT_FILE_NOTIFY_CHANGE_FILE_NAME 0x00000001
#define __SPRT_FILE_NOTIFY_CHANGE_DIR_NAME 0x00000002
#define __SPRT_FILE_NOTIFY_CHANGE_ATTRIBUTES 0x00000004
#define __SPRT_FILE_NOTIFY_CHANGE_SIZE 0x00000008
#define __SPRT_FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
#define __SPRT_FILE_NOTIFY_CHANGE_LAST_ACCESS 0x00000020
#define __SPRT_FILE_NOTIFY_CHANGE_CREATION 0x00000040
#define __SPRT_FILE_NOTIFY_CHANGE_SECURITY 0x00000100

// clang-format on

/* File information classes */
typedef enum _FILE_INFORMATION_CLASS {
	FileDirectoryInformation = 1,
	FileFullDirectoryInformation = 2,
	FileBothDirectoryInformation = 3,
	FileBasicInformation = 4,
	FileStandardInformation = 5,
	FileInternalInformation = 6,
	FileEaInformation = 7,
	FileAccessInformation = 8,
	FileNameInformation = 9,
	FileRenameInformation = 10,
	FileLinkInformation = 11,
	FileNamesInformation = 12,
	FileDispositionInformation = 13,
	FilePositionInformation = 14,
	FileFullEaInformation = 15,
	FileModeInformation = 16,
	FileAlignmentInformation = 17,
	FileAllInformation = 18,
	FileAllocationInformation = 19,
	FileEndOfFileInformation = 20,
	FileAlternateNameInformation = 21,
	FileStreamInformation = 22,
	FilePipeInformation = 23,
	FilePipeLocalInformation = 24,
	FilePipeRemoteInformation = 25,
	FileMailslotQueryInformation = 26,
	FileMailslotSetInformation = 27,
	FileCompressionInformation = 28,
	FileObjectIdInformation = 29,
	FileCompletionInformation = 30,
	FileMoveClusterInformation = 31,
	FileQuotaInformation = 32,
	FileReparsePointInformation = 33,
	FileNetworkOpenInformation = 34,
	FileAttributeTagInformation = 35,
	FileTrackingInformation = 36,
	FileIdBothDirectoryInformation = 37,
	FileIdFullDirectoryInformation = 38,
	FileValidDataLengthInformation = 39,
	FileShortNameInformation = 40,
	FileIoCompletionNotificationInformation = 41,
	FileIoStatusBlockRangeInformation = 42,
	FileIoPriorityHintInformation = 43,
	FileSfioReserveInformation = 44,
	FileSfioVolumeInformation = 45,
	FileHardLinkInformation = 46,
	FileProcessIdsUsingFileInformation = 47,
	FileNormalizedNameInformation = 48,
	FileNetworkPhysicalNameInformation = 49,
	FileIdGlobalTxDirectoryInformation = 50,
	FileIsRemoteDeviceInformation = 51,
	FileUnusedInformation = 52,
	FileNumaNodeInformation = 53,
	FileStandardLinkInformation = 54,
	FileRemoteProtocolInformation = 55,
	FileRenameInformationBypassAccessCheck = 56,
	FileLinkInformationBypassAccessCheck = 57,
	FileVolumeNameInformation = 58,
	FileIdInformation = 59,
	FileIdExtdDirectoryInformation = 60,
	FileReplaceCompletionInformation = 61,
	FileHardLinkFullIdInformation = 62,
	FileIdExtdBothDirectoryInformation = 63,
	FileDispositionInformationEx = 64,
	FileRenameInformationEx = 65,
	FileRenameInformationExBypassAccessCheck = 66,
	FileDesiredStorageClassInformation = 67,
	FileStatInformation = 68,
	FileMemoryPartitionInformation = 69,
	FileStatLxInformation = 70,
	FileCaseSensitiveInformation = 71,
	FileLinkInformationEx = 72,
	FileLinkInformationExBypassAccessCheck = 73,
	FileStorageReserveIdInformation = 74,
	FileCaseSensitiveInformationForceAccessCheck = 75,
	FileKnownFolderInformation = 76,
	FileStatBasicInformation = 77,
	FileId64ExtdDirectoryInformation = 78,
	FileId64ExtdBothDirectoryInformation = 79,
	FileIdAllExtdDirectoryInformation = 80,
	FileIdAllExtdBothDirectoryInformation = 81,
	FileStreamReservationInformation,
	FileMupProviderInfo,
	FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef enum _FILE_INFO_BY_HANDLE_CLASS {
	FileBasicInfo,
	FileStandardInfo,
	FileNameInfo,
	FileRenameInfo,
	FileDispositionInfo,
	FileAllocationInfo,
	FileEndOfFileInfo,
	FileStreamInfo,
	FileCompressionInfo,
	FileAttributeTagInfo,
	FileIdBothDirectoryInfo,
	FileIdBothDirectoryRestartInfo,
	FileIoPriorityHintInfo,
	FileRemoteProtocolInfo,
	FileFullDirectoryInfo,
	FileFullDirectoryRestartInfo,
	FileStorageInfo,
	FileAlignmentInfo,
	FileIdInfo,
	FileIdExtdDirectoryInfo,
	FileIdExtdDirectoryRestartInfo,
	FileDispositionInfoEx,
	FileRenameInfoEx,
	FileCaseSensitiveInfo,
	FileNormalizedNameInfo,
	MaximumFileInfoByHandleClass
} FILE_INFO_BY_HANDLE_CLASS, *PFILE_INFO_BY_HANDLE_CLASS;

typedef enum _GET_FILEEX_INFO_LEVELS {
	GetFileExInfoStandard,
	GetFileExMaxInfoLevel
} GET_FILEEX_INFO_LEVELS;

typedef struct _BY_HANDLE_FILE_INFORMATION {
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD dwVolumeSerialNumber;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD nNumberOfLinks;
	DWORD nFileIndexHigh;
	DWORD nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION, *LPBY_HANDLE_FILE_INFORMATION;

/* File information structures */
typedef struct _FILE_BASIC_INFO {
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	DWORD FileAttributes;
} FILE_BASIC_INFO, *PFILE_BASIC_INFO;

typedef struct _FILE_STANDARD_INFO {
	LARGE_INTEGER AllocationSize;
	LARGE_INTEGER EndOfFile;
	DWORD NumberOfLinks;
	BOOLEAN DeletePending;
	BOOLEAN Directory;
} FILE_STANDARD_INFO, *PFILE_STANDARD_INFO;

/* GetFileInformationByHandleEx(FileAttributeTagInfo): attributes + reparse tag */
typedef struct _FILE_ATTRIBUTE_TAG_INFO {
	DWORD FileAttributes;
	DWORD ReparseTag;
} FILE_ATTRIBUTE_TAG_INFO, *PFILE_ATTRIBUTE_TAG_INFO;

/* FILE_STORAGE_INFO structure for extended file info */
typedef struct _FILE_STORAGE_INFO {
	ULONG LogicalBytesPerSector;
	ULONG PhysicalBytesPerSectorForAtomicity;
	ULONG PhysicalBytesPerSectorForPerformance;
	ULONG FileSystemEffectivePhysicalBytesPerSectorForAtomicity;
	ULONG Flags;
	ULONG ByteOffsetForSectorAlignment;
	ULONG ByteOffsetForPartitionAlignment;
} FILE_STORAGE_INFO, *PFILE_STORAGE_INFO;

/**
 * File ID Both Directory Info structure used with FileIdBothDirectoryInfo and FileIdBothDirectoryRestartInfo.
 * Used by FindFirstFileEx/FindNextFile to enumerate directory contents.
 */
typedef struct _FILE_ID_BOTH_DIR_INFO {
	DWORD NextEntryOffset;
	DWORD FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	DWORD FileAttributes;
	DWORD FileNameLength;
	DWORD EaSize;
	CCHAR ShortNameLength;
	WCHAR ShortName[12];
	LARGE_INTEGER FileId;
	WCHAR FileName[1];
} FILE_ID_BOTH_DIR_INFO, *PFILE_ID_BOTH_DIR_INFO;

typedef union _FILE_SEGMENT_ELEMENT {
	PVOID Buffer;
	ULONGLONG Alignment;
} FILE_SEGMENT_ELEMENT, *PFILE_SEGMENT_ELEMENT;

typedef struct _WIN32_FILE_ATTRIBUTE_DATA {
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA, *LPWIN32_FILE_ATTRIBUTE_DATA;

typedef struct _WIN32_FIND_DATAA {
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;
	CHAR cFileName[__SPRT_MAX_PATH];
	CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

typedef struct _WIN32_FIND_DATAW {
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;
	WCHAR cFileName[__SPRT_MAX_PATH];
	WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

typedef struct _FILE_NOTIFY_INFORMATION {
	DWORD NextEntryOffset;
	DWORD Action;
	DWORD FileNameLength;
	WCHAR FileName[1];
} FILE_NOTIFY_INFORMATION, *PFILE_NOTIFY_INFORMATION;

typedef void(WINAPI *LPOVERLAPPED_COMPLETION_ROUTINE)(DWORD dwErrorCode,
		DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

// ReadDirectoryChangesW notification filter and record actions
#define __SPRT_FILE_NOTIFY_CHANGE_FILE_NAME    0x00000001
#define __SPRT_FILE_NOTIFY_CHANGE_DIR_NAME     0x00000002
#define __SPRT_FILE_NOTIFY_CHANGE_ATTRIBUTES   0x00000004
#define __SPRT_FILE_NOTIFY_CHANGE_SIZE         0x00000008
#define __SPRT_FILE_NOTIFY_CHANGE_LAST_WRITE   0x00000010
#define __SPRT_FILE_NOTIFY_CHANGE_LAST_ACCESS  0x00000020
#define __SPRT_FILE_NOTIFY_CHANGE_CREATION     0x00000040
#define __SPRT_FILE_NOTIFY_CHANGE_SECURITY     0x00000100

#define __SPRT_FILE_ACTION_ADDED               0x00000001
#define __SPRT_FILE_ACTION_REMOVED             0x00000002
#define __SPRT_FILE_ACTION_MODIFIED            0x00000003
#define __SPRT_FILE_ACTION_RENAMED_OLD_NAME    0x00000004
#define __SPRT_FILE_ACTION_RENAMED_NEW_NAME    0x00000005

#endif // SPRT_WRAPPERS_WINDOWS_ABI_FILE_API_H_
