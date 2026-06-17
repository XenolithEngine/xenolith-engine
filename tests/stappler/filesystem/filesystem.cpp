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

#include "SPCommon.h"
#include "SPFilesystem.h"
#include "SPFilepath.h"
#include "SPMemInterface.h"

#include "../tests.h"

#include <stdio.h> // __sprt_fpath_to_native + SPRT_WINDOWS (Windows-native path round-trip)

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

// Verifies the filesystem layer accepts both posix and Windows-native paths. A probe file written
// through a posix path must be found again through posix and (on Windows) native path forms — which
// exercises filesystem::toPosixPath(). On POSIX hosts the native checks are compiled out (there is
// no native form distinct from posix); the posix checks still validate the normalization no-op.
void performFilesystemTests() {
	sprt::cout << "\n== stappler filesystem tests (path normalization) ==\n";

	// A probe file in the current directory (Custom category == cwd-relative). Using a plain file
	// (not a subtree) keeps the test idempotent across platforms.
	FileInfo file("xlfs_probe.txt", LocationCategory::Custom);
	filesystem::remove(file); // drop any leftover from a previous run

	check(filesystem::write(file, (const uint8_t *)"probe", 5), "fs: write (posix relative)");
	check(filesystem::exists(file), "fs: exists() via posix relative path");

	// stat must succeed and report the file's size and type (Windows stat regressed when an
	// advisory FileStorageInfo query is unavailable, e.g. under wine).
	filesystem::Stat fst;
	check(filesystem::stat(file, fst), "fs: stat() succeeds");
	check(fst.size == 5, "fs: stat() reports correct size");
	check(fst.type == FileType::File, "fs: stat() reports a regular file");

	// absolute posix path to the same file
	auto absPosix =
			filesystem::currentDir<memory::StandartInterface>(StringView("xlfs_probe.txt"), false);
	check(!absPosix.empty()
					&& filesystem::exists(FileInfo(StringView(absPosix.data(), absPosix.size()))),
			"fs: exists() via absolute posix path");

#if SPRT_WINDOWS
	// Windows-native absolute path: convert the posix absolute path to native form (drive letter +
	// backslashes, e.g. C:\dir\file or Z:\... under wine) and confirm the filesystem layer still
	// resolves it — i.e. toPosixPath() converted it back to posix internally.
	if (!absPosix.empty()) {
		memory::StandartInterface::StringType nativeBuf;
		nativeBuf.resize(absPosix.size() + 8);
		auto n = __sprt_fpath_to_native(absPosix.data(), absPosix.size(), nativeBuf.data(),
				nativeBuf.size());
		if (n > 0) {
			nativeBuf.resize(n);
			StringView native(nativeBuf.data(), nativeBuf.size());
			sprt::cout << "    (native form: " << native << ")\n";
			check(filesystem::exists(FileInfo(native)),
					"fs: exists() via Windows-native absolute path (drive + backslashes)");
		} else {
			check(false, "fs: fpath_to_native produced a native path");
		}
	}
#endif

	// --- removal: a regular file (must report success and actually delete it) ---
	check(filesystem::remove(file), "fs: remove (regular file)");
	check(!filesystem::exists(file), "fs: removed file is gone");

	// --- removal: a directory TREE (a file plus a nested subdirectory), recursively ---
	FileInfo dir("xlfs_rmdir", LocationCategory::Custom);
	filesystem::remove(dir, true); // drop any leftover from a previous run
	filesystem::mkdir(dir);
	filesystem::mkdir(FileInfo("xlfs_rmdir/sub", LocationCategory::Custom));
	filesystem::write(FileInfo("xlfs_rmdir/a.txt", LocationCategory::Custom),
			(const uint8_t *)"a", 1);
	filesystem::write(FileInfo("xlfs_rmdir/sub/b.txt", LocationCategory::Custom),
			(const uint8_t *)"b", 1);
	check(filesystem::remove(dir, true), "fs: remove (recursive directory tree)");
	check(!filesystem::exists(dir), "fs: removed directory tree is gone");
}

} // namespace stappler
