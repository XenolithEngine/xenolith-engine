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

#ifndef SPRT_WRAPPERS_WINDOWS_FILE_API_H_
#define SPRT_WRAPPERS_WINDOWS_FILE_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>

// clang-format off
#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5

#define FILE_READ_DATA            ( 0x0001 )    // file & pipe
#define FILE_LIST_DIRECTORY       ( 0x0001 )    // directory
#define FILE_WRITE_DATA           ( 0x0002 )    // file & pipe
#define FILE_ADD_FILE             ( 0x0002 )    // directory
#define FILE_APPEND_DATA          ( 0x0004 )    // file
#define FILE_ADD_SUBDIRECTORY     ( 0x0004 )    // directory
#define FILE_CREATE_PIPE_INSTANCE ( 0x0004 )    // named pipe
#define FILE_READ_EA              ( 0x0008 )    // file & directory
#define FILE_WRITE_EA             ( 0x0010 )    // file & directory
#define FILE_EXECUTE              ( 0x0020 )    // file
#define FILE_TRAVERSE             ( 0x0020 )    // directory
#define FILE_DELETE_CHILD         ( 0x0040 )    // directory
#define FILE_READ_ATTRIBUTES      ( 0x0080 )    // all
#define FILE_WRITE_ATTRIBUTES     ( 0x0100 )    // all

#define FILE_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x1FF)

#define FILE_GENERIC_READ   (STANDARD_RIGHTS_READ | FILE_READ_DATA | \
	FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE)

#define FILE_GENERIC_WRITE  (STANDARD_RIGHTS_WRITE | FILE_WRITE_DATA | \
	FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_APPEND_DATA | SYNCHRONIZE)

#define FILE_GENERIC_EXECUTE (STANDARD_RIGHTS_EXECUTE | FILE_READ_ATTRIBUTES | \
	FILE_EXECUTE | SYNCHRONIZE)

#define FILE_SHARE_READ                 0x00000001  
#define FILE_SHARE_WRITE                0x00000002  
#define FILE_SHARE_DELETE               0x00000004  

/* File attributes */
#define FILE_ATTRIBUTE_READONLY             0x00000001
#define FILE_ATTRIBUTE_HIDDEN               0x00000002
#define FILE_ATTRIBUTE_SYSTEM               0x00000004
#define FILE_ATTRIBUTE_DIRECTORY            0x00000010
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020
#define FILE_ATTRIBUTE_DEVICE               0x00000040
#define FILE_ATTRIBUTE_NORMAL               0x00000080
#define FILE_ATTRIBUTE_TEMPORARY            0x00000100
#define FILE_ATTRIBUTE_SPARSE_FILE          0x00000200
#define FILE_ATTRIBUTE_REPARSE_POINT        0x00000400
#define FILE_ATTRIBUTE_COMPRESSED           0x00000800
#define FILE_ATTRIBUTE_OFFLINE              0x00001000
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  0x00002000
#define FILE_ATTRIBUTE_ENCRYPTED            0x00004000
#define FILE_ATTRIBUTE_INTEGRITY_STREAM     0x00008000
#define FILE_ATTRIBUTE_VIRTUAL              0x00010000
#define FILE_ATTRIBUTE_NO_SCRUB_DATA        0x00020000
#define FILE_ATTRIBUTE_EA                   0x00040000
#define FILE_ATTRIBUTE_PINNED               0x00080000
#define FILE_ATTRIBUTE_UNPINNED             0x00100000
#define FILE_ATTRIBUTE_RECALL_ON_OPEN       0x00040000
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000

#define FILE_VOLUME_IS_COMPRESSED           0x00008000  
#define FILE_SUPPORTS_OBJECT_IDS            0x00010000  
#define FILE_SUPPORTS_ENCRYPTION            0x00020000  
#define FILE_NAMED_STREAMS                  0x00040000  
#define FILE_READ_ONLY_VOLUME               0x00080000  
#define FILE_SEQUENTIAL_WRITE_ONCE          0x00100000  
#define FILE_SUPPORTS_TRANSACTIONS          0x00200000  
#define FILE_SUPPORTS_HARD_LINKS            0x00400000  
#define FILE_SUPPORTS_EXTENDED_ATTRIBUTES   0x00800000  
#define FILE_SUPPORTS_OPEN_BY_FILE_ID       0x01000000  
#define FILE_SUPPORTS_USN_JOURNAL           0x02000000  
#define FILE_SUPPORTS_INTEGRITY_STREAMS     0x04000000  
#define FILE_SUPPORTS_BLOCK_REFCOUNTING     0x08000000  
#define FILE_SUPPORTS_SPARSE_VDL            0x10000000  
#define FILE_DAX_VOLUME                     0x20000000  
#define FILE_SUPPORTS_GHOSTING              0x40000000 

/* File flags for CreateFileW (winbase.h) */
#define FILE_FLAG_WRITE_THROUGH         0x80000000
#define FILE_FLAG_OVERLAPPED            0x40000000
#define FILE_FLAG_NO_BUFFERING          0x20000000
#define FILE_FLAG_RANDOM_ACCESS         0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN       0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE       0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS      0x02000000
#define FILE_FLAG_POSIX_SEMANTICS       0x01000000
#define FILE_FLAG_SESSION_AWARE         0x00800000
#define FILE_FLAG_OPEN_REPARSE_POINT    0x00200000
#define FILE_FLAG_OPEN_NO_RECALL        0x00100000
#define FILE_FLAG_FIRST_PIPE_INSTANCE   0x00080000

#define MOVEFILE_REPLACE_EXISTING       0x00000001
#define MOVEFILE_COPY_ALLOWED           0x00000002
#define MOVEFILE_DELAY_UNTIL_REBOOT     0x00000004
#define MOVEFILE_WRITE_THROUGH          0x00000008
#define MOVEFILE_CREATE_HARDLINK        0x00000010
#define MOVEFILE_FAIL_IF_NOT_TRACKABLE  0x00000020

/* File move flags */
#define FILE_BEGIN                  0
#define FILE_CURRENT                1
#define FILE_END                    2

/* Final path name flags */
#define FILE_NAME_NORMALIZED        0x00000000
#define FILE_NAME_OPENED            0x00000008

#define FILE_MAP_WRITE            SECTION_MAP_WRITE
#define FILE_MAP_READ             SECTION_MAP_READ
#define FILE_MAP_ALL_ACCESS       SECTION_ALL_ACCESS

#define FILE_MAP_EXECUTE          SECTION_MAP_EXECUTE_EXPLICIT  // not included in FILE_MAP_ALL_ACCESS

#define FILE_MAP_COPY             0x00000001

#define FILE_MAP_RESERVE          0x80000000
#define FILE_MAP_TARGETS_INVALID  0x40000000
#define FILE_MAP_LARGE_PAGES      0x20000000

#define SYMBOLIC_LINK_FLAG_DIRECTORY                    (0x1)
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE    (0x2)

#define SEC_HUGE_PAGES              0x00020000  
#define SEC_LARGE_PAGES             0x80000000

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

__SPRT_BEGIN_DECL

/* ---- File I/O (handleapi.h, fileapi.h) ---- */
/**
 * Creates or opens a file or I/O device.
 * @param lpFileName Name of the file/device to create/open.
 * @param dwDesiredAccess Access mode (read/write).
 * @param dwShareMode Share mode.
 * @param lpSecurityAttributes Security attributes (NULL = default).
 * @param dwCreationDisposition Creation action.
 * @param dwFlagsAndAttributes File attributes and flags.
 * @param hTemplateFile Template file handle (usually NULL).
 * @return Handle to the file/device, or INVALID_HANDLE_VALUE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
 */
WINAPI HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
		DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

/**
 * Reads data from a file.
 * @param hFile Handle to the file.
 * @param lpBuffer Buffer to receive the read data.
 * @param nNumberOfBytesToRead Number of bytes to read.
 * @param lpNumberOfBytesRead Pointer to receive actual bytes read.
 * @param lpOverlapped Optional overlapped structure for async I/O.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
 */
WINAPI BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

/**
 * Writes data to a file.
 * @param hFile Handle to the file.
 * @param lpBuffer Buffer containing data to write.
 * @param nNumberOfBytesToWrite Number of bytes to write.
 * @param lpNumberOfBytesWritten Pointer to receive actual bytes written.
 * @param lpOverlapped Optional overlapped structure for async I/O.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile
 */
WINAPI BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);

/**
 * Sets the file pointer for a given file handle.
 * @param hFile Handle to the file.
 * @param liDistanceToMove Structure specifying distance to move.
 * @param lpNewFilePointer Pointer to receive new file position (may be NULL).
 * @param dwMoveMethod Method for moving the pointer.
 * @return New file position, or INVALID_SET_FILE_POINTER on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointerex
 */
WINAPI BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove,
		PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod);

/**
 * Sets the end-of-file for a file.
 * @param hFile Handle to the file.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setendoffile
 */
WINAPI BOOL SetEndOfFile(HANDLE hFile);

/**
 * Flushes the buffers for a specified file and also causes all file data to be written to the disk.
 * @param hFile Handle to an open file.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers
 */
WINAPI BOOL FlushFileBuffers(HANDLE hFile);

/**
 * Retrieves the path of a file or directory given its handle.
 * @param hHandle Handle to an open file object.
 * @param lpFilePath Buffer for the output path string.
 * @param nSize Size of the buffer in characters.
 * @param dwFlags Flags controlling the output format.
 * @return Length of the string copied to the buffer, or zero on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew
 */
WINAPI DWORD GetFinalPathNameByHandleW(HANDLE hHandle, LPWSTR lpFilePath, DWORD nSize,
		DWORD dwFlags);

/**
 * Sets the attributes of a file or directory.
 * @param lpFileName Path to the file or directory.
 * @param dwFileAttributes New file attributes.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileattributesw
 */
WINAPI BOOL SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes);

/**
 * Sets file information for an open file or directory handle.
 * @param hFile Handle to the file or directory.
 * @param FileInformationClass Type of file information to set.
 * @param lpFileInformation Pointer to file information structure.
 * @param dwSize Size of the information in bytes.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle
 */
WINAPI BOOL SetFileInformationByHandle(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
		LPVOID lpFileInformation, DWORD dwBufferSize);

/**
 * Creates a new directory.
 * @param lpPathName Path of the directory to create.
 * @param lpSecurityAttributes Security attributes (NULL = default).
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectoryw
 */
WINAPI BOOL CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

/**
 * Creates a temporary file and returns its name.
 * @param lpPathName Path for the temp file directory.
 * @param lpTemplateFile Template filename prefix (up to 3 chars).
 * @param uUnique Value appended to create unique filename.
 * @param lpTempFileName Buffer for the generated filename.
 * @return Length of the generated filename, or zero on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettempfilenamew
 */
WINAPI DWORD GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpTemplateFile, UINT uUnique,
		LPWSTR lpTempFileName);

WINAPI BOOL GetFileInformationByHandleEx(HANDLE hFile,
		FILE_INFO_BY_HANDLE_CLASS FileInformationClass, LPVOID lpFileInformation,
		DWORD dwBufferSize);

WINAPI DWORD GetLogicalDrives();

/**
 * Determines whether a path is relative.
 * @param pszPath Pointer to null-terminated string containing the path to check.
 * @return TRUE if the path is relative, FALSE otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-pathisrelativew
 */
WINAPI BOOL PathIsRelativeW(LPCWSTR pszPath);

/**
 * Retrieves the current directory for the current process.
 * @param nBufferLength Length of the buffer in characters.
 * @param lpBuffer Pointer to a buffer that receives the current directory path.
 * @return Length of the string copied to the buffer, or zero if the buffer length is less than or equal to zero.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getcurrentdirectoryw
 */
WINAPI DWORD GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer);

/**
 * Moves the file pointer of an open file.
 * @param hFile Handle to the file.
 * @param lDistanceToMove Number of bytes to move the file pointer.
 * @param lpDistanceToMoveHigh Pointer to the high-order word of the distance to move (can be NULL).
 * @param dwMoveMethod Method for moving the file pointer (FILE_BEGIN, FILE_CURRENT, or FILE_END).
 * @return Low-order portion of the new file-pointer position, or INVALID_SET_FILE_POINTER on error.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointer
 */
WINAPI DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG *lpDistanceToMoveHigh,
		DWORD dwMoveMethod);

/**
 * Retrieves file information for the specified file handle.
 * @param hFile Handle to the file.
 * @param lpFileInformation Pointer to a BY_HANDLE_FILE_INFORMATION structure that receives file information.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandle
 */
WINAPI BOOL GetFileInformationByHandle(HANDLE hFile,
		LPBY_HANDLE_FILE_INFORMATION lpFileInformation);

/**
 * Reopens an existing file with different access rights.
 * @param hOriginalFile Handle to the original file.
 * @param dwDesiredAccess Access mode for the new handle.
 * @param dwShareMode Share mode for the new handle.
 * @param dwFlagsAndAttributes File attributes and flags.
 * @return New file handle, or INVALID_HANDLE_VALUE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-reopenfile
 */
WINAPI HANDLE ReOpenFile(HANDLE hOriginalFile, DWORD dwDesiredAccess, DWORD dwShareMode,
		DWORD dwFlagsAndAttributes);
/**
 * Creates a file mapping object for use with memory-mapped files.
 * @param lpFileAttributes Security attributes (NULL = default).
 * @param dwMaximumSizeHigh High-order 32 bits of maximum size.
 * @param dwMaximumSizeLow Low-order 32 bits of maximum size.
 * @param lpName Name of the file mapping object.
 * @param flProtect Protection for the mapping.
 * @param dwDesiredAccess Access mode for the mapping.
 * @return Handle to the file mapping object, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-createfilemappingw
 */
WINAPI HANDLE CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
		DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName);

/**
 * Creates a view of a file mapping object in the address space of the calling process.
 * @param hFileMappingObject Handle to the file mapping object.
 * @param dwDesiredAccess Access mode for the view.
 * @param dwFileOffsetHigh High-order 32 bits of offset.
 * @param dwFileOffsetLow Low-order 32 bits of offset.
 * @param dwNumberOfBytesToMap Number of bytes to map.
 * @return Pointer to the base address of the view, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile
 */
WINAPI LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
		DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);

/**
 * Unmaps a mapped view of a file from the calling processs address space.
 * @param lpBaseAddress Base address of the view to unmap.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-unmapviewoffile
 */
WINAPI BOOL UnmapViewOfFile(LPCVOID lpBaseAddress);

/**
 * Flushes modified pages of a mapped view of a file to disk.
 * @param lpBaseAddress Base address of the view to flush.
 * @param dwNumberOfBytesToFlush Number of bytes to flush (0 = all).
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-flushviewoffile
 */
WINAPI BOOL FlushViewOfFile(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush);

/**
 * Deletes an existing file.
 * @param lpFileName Name of the file to delete.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilew
 */
WINAPI BOOL DeleteFileW(LPCWSTR lpFileName);

/**
 * Removes an empty directory.
 * @param lpPathName Name of the directory to remove.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-removedirectoryw
 */
WINAPI BOOL RemoveDirectoryW(LPCWSTR lpPathName);

WINAPI DWORD GetFileAttributesW(LPCWSTR lpFileName);

WINAPI BOOL MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags);

WINAPI BOOL SetCurrentDirectoryW(LPCWSTR lpPathName);

WINAPI BOOL GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer,
		DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
		LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);

WINAPI BOOL GetDiskFreeSpaceExW(LPCWSTR lpDirectoryName,
		PULARGE_INTEGER lpFreeBytesAvailableToCaller, PULARGE_INTEGER lpTotalNumberOfBytes,
		PULARGE_INTEGER lpTotalNumberOfFreeBytes);

WINAPI BOOL CreateHardLinkW(LPCWSTR lpFileName, LPCWSTR lpExistingFileName,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes);

WINAPI BOOLEAN CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName,
		DWORD dwFlags);

WINAPI BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe,
		LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize);

WINAPI DWORD GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer);

WINAPI HRESULT PathAllocCanonicalize(PCWSTR pszPathIn, ULONG dwFlags, PWSTR *ppszPathOut);

WINAPI BOOL ReadFileScatter(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToRead, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped);

WINAPI BOOL WriteFileGather(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToWrite, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped);

__SPRT_END_DECL

#endif
