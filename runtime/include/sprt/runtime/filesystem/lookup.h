/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_FILESYSTEM_LOOKUP_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_FILESYSTEM_LOOKUP_H_

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/mem/forward_list.h>
#include <sprt/runtime/mutex.h>
#include <sprt/c/sys/__sprt_stat.h>

namespace sprt::filesystem {

/**
	Flags for searching for special categories of files in the file system.

	The application is assumed to be running in a container or sandbox where access to the
	file system is restricted. In this case, it is assumed that all file locations can be
	divided into three classes:

	Private files belong to the application itself and can only be used by the application
	itself. Other applications generally do not have access to them.

	Public files are owned by the application itself, but other applications can access, read,
	and modify them with or without permission.

	Shared files are not owned by any application and can be accessed by any application
	without notice.

	In regular desktop application mode without containerization, all files are effectively
	public or shared, but the App<X> and Bundled categories are considered private.
	
	For most resources, it is worth using a Public location, access to it is the fastest and
	cheapest, and it is available for cleaning by the operating system.

	To export public files, use Shared locations. However, you must remember that other
	applications can also write files there.

	For private and sensitive data that should be hidden from other applications, use Private
	locations.

	Если базовые флаги из PathMask не указаны - применяются правила поиска по умолчанию.

	PrivateFirst, PublicFirst и SharedFirst не фильтруют местоположение, но управляют порядком
	поиска. Только одно из этих значений может быть указано при поиске.

	The Writable flag causes a location to be found where the file can be written or overwritten.
	If this flag is passed without MakeDir, the directory at the found location may not exist
	and must be created manually.

	The Exclusive flag checks that the file at the found location does not exist, and writing
	to it will create the file. Only makes sense in combination with Writable

	The MakeDir flag ensures that before returning the location, all necessary directories are
	created to create a file directly at the found location. Only makes sense in combination
	with Writable.
*/
enum class LookupFlags : uint32_t {
	None = 0,

	Public = 1 << 0,
	Private = 1 << 1,
	Shared = 1 << 2,

	PathMask = Public | Private | Shared,

	PrivateFirst = 1 << 4,
	PublicFirst = 2 << 4,
	SharedFirst = 3 << 4,

	OrderMask = SharedFirst,

	Writable = 1 << 7,
	Exclusive = 1 << 8,
	MakeDir = 1 << 9,

	WritableMask = Writable | Exclusive | MakeDir,
};

SPRT_DEFINE_ENUM_AS_MASK(LookupFlags)

/*
	Standard file categories for lookup.
	Search locations in these categories are provided by SPRT.

	Based on XDG recommendations:
	https://www.freedesktop.org/wiki/Software/xdg-user-dirs/
	https://specifications.freedesktop.org/basedir-spec/latest/
*/
enum class LocationCategory {
	Exec, // user or system executable from PATH
	Library, // user or system dynamic shared library

	Fonts,

	UserHome,
	UserDesktop,
	UserDownload,
	UserDocuments,
	UserMusic,
	UserPictures,
	UserVideos,

	// Common<X> is a base dirs, as defined directly by XDG spec or similar OS spec
	// Runtime assumes, that this dirs are read-only
	CommonData,
	CommonConfig,
	CommonState,
	CommonCache,
	CommonRuntime,

	// App<X> targets app-specific dir within specific common location
	// It can match Common<X> on some OS with sandboxing
	// Runtime assumes, that this dirs are read-write
	AppData,
	AppConfig,
	AppState,
	AppCache,
	AppRuntime,

	Bundled, // some files, bundled with app executable

	Custom, // can be absolute or cwd-relative path
	Absolute = Custom,
	Max = Custom,
};

/**
	File access check flags. Extended version of POSIX access.

	The special Empty flag cannot be specified together with others,
	and is used to check that there is no file at the given path.
*/
enum class Access : uint32_t {
	None,
	Exists = 1 << 0,
	Read = 1 << 1,
	Write = 1 << 2,
	Execute = 1 << 3,
	AccessMask = Exists | Read | Write | Execute,

	// Used to find a place to write a new file without overwriting an existing one
	Empty = 1 << 4,
};

SPRT_DEFINE_ENUM_AS_MASK(Access)


enum class LocationFlags : uint32_t {
	None = 0,

	// File in this category can be reverse-located via `detectResourceCategory` and 'filepath::canonical'
	Locateable = 1 << 0,

	// Location's root directory can not be unplugged/unmounted when app is still active
	Unpluggable = 1 << 1,

	// Writing files to this location is possible
	Writable = 1 << 2,

	// Files are accessed through a non-standard interface
	Specific = 1 << 3,
};

SPRT_DEFINE_ENUM_AS_MASK(LocationFlags)

/**
	Posix-compatible file types
*/
enum class FileType : uint16_t {
	File, // S_IFREG
	Dir, // S_IFDIR
	BlockDevice, // S_IFBLK
	CharDevice, // S_IFCHR
	Pipe, // S_IFIFO
	Socket, // S_IFSOCK
	Link, // S_IFLNK
	Unknown
};

enum class OpenFlags : uint32_t {
	None,
	Read = 1 << 0,
	Write = 1 << 1,
	Create = 1 << 2,
	Append = 1 << 3,
	Truncate = 1 << 4,
	CreateExclusive = 1 << 5,
	DelOnClose = 1 << 6,

	// Optional hints for prefetching, use one of:
	ReadHintRandom = 1 << 28, // Random access (default it no other hints)
	ReadHintStreaming = 1 << 29, // Sequential read (possibly with seek)
	ReadHintOneshotBuffer = 1 << 30, // Oneshot read into preallocated buffer

	Override = Write | Create | Truncate,
	ReadHints = ReadHintRandom | ReadHintStreaming | ReadHintOneshotBuffer
};

SPRT_DEFINE_ENUM_AS_MASK(OpenFlags)

enum class PollFlags : uint16_t {
	None = 0,
	In = 0x001, /* There is data to read.  */
	Pri = 0x002, /* There is urgent data to read.  */
	Out = 0x004, /* Writing now will not block.  */
	Err = 0X008, /* ERROR CONDITION.  */
	HungUp = 0X010, /* HUNG UP.  */
	Invalid = 0X020, /* INVALID POLLING REQUEST.  */

	PollMask = 0x3FFF,
	CloseFd = 0x4000,
	AllowMulti = 0x8000, // Allow edge-triggered multishot setups
};

SPRT_DEFINE_ENUM_AS_MASK(PollFlags)

enum class MappingType {
	Private,
	Shared
};

struct LocationInfo;

/**
	Interface for accessing files at a specific location. Depending on the flags, not all functions can be defined.

	When an action is executed, LocationInfo is used to construct absolute paths from relative paths.
*/
struct LocationInterface {
	static constexpr __sprt_mode_t DirMode = __SPRT_S_IRUSR | __SPRT_S_IWUSR | __SPRT_S_IXUSR
			| __SPRT_S_IRGRP | __SPRT_S_IXGRP | __SPRT_S_IROTH | __SPRT_S_IXOTH;
	static constexpr __sprt_mode_t FileMode =
			__SPRT_S_IRUSR | __SPRT_S_IWUSR | __SPRT_S_IRGRP | __SPRT_S_IROTH;

	Status (*_access)(const LocationInfo &, StringView path, Access) = nullptr;
	Status (*_stat)(const LocationInfo &, StringView path, struct __SPRT_STAT_NAME *) = nullptr;

	void *(*_open)(const LocationInfo &, StringView, OpenFlags, Status *) = nullptr;
	size_t (*_read)(void *, uint8_t *buf, size_t nbytes, Status *) = nullptr;
	size_t (*_write)(void *, const uint8_t *buf, size_t nbytes, Status *) = nullptr;
	size_t (*_seek)(void *, int64_t offset, int, Status *) = nullptr;
	size_t (*_tell)(void *, Status *) = nullptr;
	void (*_flush)(void *, Status *) = nullptr;
	void (*_close)(void *, Status *) = nullptr;

	Status (*_unlink)(const LocationInfo &, StringView) = nullptr;
	Status (*_remove)(const LocationInfo &, StringView) = nullptr;
	Status (*_touch)(const LocationInfo &, StringView) = nullptr;
	Status (*_mkdir)(const LocationInfo &, StringView, __sprt_mode_t) = nullptr;
	Status (*_rename)(const LocationInfo &, StringView from, const LocationInfo &,
			StringView to) = nullptr;
	Status (*_copy)(const LocationInfo &, StringView from, const LocationInfo &,
			StringView to) = nullptr;

	Status (*_ftw)(const LocationInfo &, StringView path,
			const callback<bool(StringView, FileType)> &, int depth, bool dirFirst) = nullptr;

	Status (*_write_oneshot)(const LocationInfo &, StringView path, const uint8_t *buf,
			size_t nbytes, __sprt_mode_t mode, bool override) = nullptr;

	uint8_t *(*_mmap)(uint8_t storage[16], const LocationInfo &, StringView path, MappingType type,
			__sprt_mode_t prot, size_t offset, size_t len, Status *) = nullptr;
	Status (*_munmap)(uint8_t *region, uint8_t storage[16]) = nullptr;
	Status (*_msync)(uint8_t *region, uint8_t storage[16]) = nullptr;

	__sprt_FILE *(*_fdopen)(void *, const char *, Status *) = nullptr;
};

struct LocationInfo {
	StringView path;
	LookupFlags lookupType = LookupFlags::None;
	LocationFlags locationFlags = LocationFlags::None;
	const LocationInterface *interface = nullptr;
};

struct LookupInfo {
	LocationCategory category = LocationCategory::Custom;
	StringView prefix;
	memory::forward_list<LocationInfo> paths;
	mutable qmutex mutex;
	bool init = false;
	LookupFlags defaultLookupFlags = LookupFlags::None;
	memory::pool_t *_pool = nullptr;
};

SPRT_API const LocationInterface *getDefaultInterface();

// Returns the location info for the current directory
SPRT_API const LocationInfo &getCurrentLocation();

SPRT_API const LookupInfo *getLookupInfo(LocationCategory);

// Paths above root is allowed
SPRT_API bool getCurrentDir(const callback<void(StringView)> &, StringView = StringView());

/*
	Reads an environment variable taking into account the rules for accessing paths in the file system.
*/
SPRT_API StringView readVariable(memory::pool_t *pool, StringView key);

SPRT_API LocationCategory getResourceCategoryByPrefix(StringView prefix);

SPRT_API void enumeratePaths(const LookupInfo &res, StringView ifilename, LookupFlags flags,
		Access a, const callback<bool(const LocationInfo &, StringView)> &cb);

SPRT_API void enumeratePaths(LocationCategory t, LookupFlags flags,
		const callback<bool(const LocationInfo &, StringView)> &cb);

} // namespace sprt::filesystem

#endif
