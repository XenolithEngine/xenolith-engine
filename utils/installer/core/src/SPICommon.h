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

#ifndef UTILS_INSTALLER_CORE_SRC_SPICOMMON_H_
#define UTILS_INSTALLER_CORE_SRC_SPICOMMON_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPString.h"
#include "SPData.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// The core is consumed by the CLI (its own thread) and by the GUI (worker pool → app thread), so
// every owned value uses the malloc-backed interface: it can cross threads without carrying a pool.
using mem_std::String;
using mem_std::Bytes;
using mem_std::Vector;
using mem_std::Function;

// Variadic concatenation: toString("no host for ", arch, "-", os). The single-argument form
// replaces the StringView → String conversion (mem_std::String has no implicit one).
using mem_std::toString;

using Value = data::ValueTemplate<mem_std::Interface>;

// Path building — the only path concatenation the core is allowed to do.
inline String mergePath(StringView root, StringView path) {
	return filepath::merge<mem_std::Interface>(root, path);
}

inline String mergePath(StringView root, StringView path, StringView sub) {
	return filepath::merge<mem_std::Interface>(root, path, sub);
}

// Existence predicates. `filesystem::stat` follows symlinks (the layer exposes no lstat), which is
// what every caller here wants: a symlinked toolchain dir must read as a directory.
inline bool isDirectory(StringView path) {
	filesystem::Stat stat;
	return filesystem::stat(FileInfo(path), stat) && stat.type == FileType::Dir;
}

inline bool isFile(StringView path) {
	filesystem::Stat stat;
	return filesystem::stat(FileInfo(path), stat) && stat.type == FileType::File;
}

// Common outcome of a fallible operation: a Status plus a human-readable reason. Operations that
// produce data derive from it and add their payload, so every call site tests the same way
// (`if (!result)`) and builds its message the same way (`setError(...)`).
struct SP_PUBLIC OperationResult {
	Status status = Status::Ok;
	String error; // empty on success

	bool valid() const { return isSuccessful(status) && error.empty(); }

	explicit operator bool() const { return valid(); }

	template <typename... Args>
	void setError(Status st, Args &&...args) {
		status = st;
		error = toString(sprt::forward<Args>(args)...);
	}
};

} // namespace stappler::xenolith::installer

#endif // UTILS_INSTALLER_CORE_SRC_SPICOMMON_H_
