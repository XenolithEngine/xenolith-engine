/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "SPFilepath.h" // IWYU pragma: keep
#include <unistd.h>

#if LINUX

#include "SPFilesystem.h"
#include "SPMemInterface.h"
#include "SPSharedModule.h"
#include "detail/SPFilesystemResourceData.h"

namespace STAPPLER_VERSIONIZED stappler::filesystem::platform {

static char s_execPath[PATH_MAX] = {0};
static char s_homePath[PATH_MAX] = {0};

// No PlatformSpecific categories defined for now
void _enumerateObjects(const FilesystemResourceData &data, FileCategory, StringView path, FileFlags,
		Access, const Callback<bool(StringView, FileFlags)> &) { }

bool _access(FileCategory cat, StringView path, Access) { return false; }

bool _stat(FileCategory cat, StringView path, Stat &stat) { return false; }

File _openForReading(FileCategory cat, StringView path) { return File(); }

size_t _read(void *, uint8_t *buf, size_t nbytes) { return 0; }
size_t _seek(void *, int64_t offset, io::Seek s) { return maxOf<size_t>(); }
size_t _tell(void *) { return 0; }
bool _eof(void *) { return true; }
void _close(void *) { }

Status _ftw(FileCategory cat, StringView path,
		const Callback<bool(StringView path, FileType t)> &cb, int depth, bool dirFirst) {
	return Status::Declined;
}

} // namespace stappler::filesystem::platform

#endif
