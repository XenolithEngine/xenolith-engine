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

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

namespace sprt {

using dispatch::WatchFlags;

static void writeFileContent(const char *path, const char *data) {
	auto fp = ::fopen(path, "w");
	if (fp) {
		::fputs(data, fp);
		::fclose(fp);
	}
}

// Count open inotify *instances* for this process by scanning /proc/self/fd:
// each inotify fd is a symlink to "anon_inode:inotify". The whole point of the
// shared-reader design is that N watched files use exactly ONE instance.
static int countInotifyInstances() {
	auto dir = ::opendir("/proc/self/fd");
	if (!dir) {
		return -1;
	}
	int count = 0;
	char linkpath[64];
	char target[256];
	while (auto ent = ::readdir(dir)) {
		::snprintf(linkpath, sizeof(linkpath), "/proc/self/fd/%s", ent->d_name);
		auto n = ::readlink(linkpath, target, sizeof(target) - 1);
		if (n > 0) {
			target[n] = '\0';
			if (::strstr(target, "anon_inode:inotify") != nullptr) {
				++count;
			}
		}
	}
	::closedir(dir);
	return count;
}

void performWatchFileTests() {
	sprt::cout << "\n== runtime watchFile tests ==\n";

	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		sprt::cout << "watchFile: no looper\n";
		return;
	}

	int failures = 0;
	auto check = [&](bool cond, StringView msg) {
		sprt::cout << (cond ? "  PASS: " : "  FAIL: ") << msg << "\n";
		if (!cond) {
			++failures;
		}
	};

	// dedicated temp dir so events cannot cross-talk with other /tmp activity
	char dir[256];
	::snprintf(dir, sizeof(dir), "/tmp/sprt_watch_%d", (int)::getpid());
	::mkdir(dir, 0755);

	char path[512];
	char tmp[512];
	::snprintf(path, sizeof(path), "%s/probe.txt", dir);
	::snprintf(tmp, sizeof(tmp), "%s/probe.tmp", dir);
	::remove(path);
	::remove(tmp);

	WatchFlags observed = WatchFlags::None;

	auto h = looper->watchFile(StringView(path), WatchFlags::Any,
			[&observed](WatchFlags f) -> Status {
		observed |= f;
		sprt::cout << "  event flags: " << toInt(f) << "\n";
		return Status::Ok;
	});

	check(h != nullptr, "watchFile returns a handle (inotify backend present)");
	if (!h) {
		return;
	}

	sprt::cout << "  engine: " << toInt(looper->getQueue()->getEngine())
			   << ", watching: " << h->getPath() << "\n";

	// Drain inotify events for a short window (loop runs until the timeout,
	// firing the callback for whatever the kernel delivered in between).
	auto pump = [&] { looper->run(dispatch::TimeInterval::milliseconds(300)); };

	// 1. create-in-place: expect Created (fopen "w" on a missing file) and
	//    typically Modified/close-write for the initial content.
	observed = WatchFlags::None;
	writeFileContent(path, "one");
	pump();
	check(hasFlag(observed, WatchFlags::Created) || hasFlag(observed, WatchFlags::Modified),
			"create detected");

	// 2. modify existing file: expect Modified.
	observed = WatchFlags::None;
	writeFileContent(path, "two-longer-content");
	pump();
	check(hasFlag(observed, WatchFlags::Modified), "modify detected");

	// 3. atomic replace (write temp + rename over the name) — the key
	//    name-based robustness case: expect MovedTo for the watched name.
	observed = WatchFlags::None;
	writeFileContent(tmp, "replacement");
	::rename(tmp, path);
	pump();
	check(hasFlag(observed, WatchFlags::MovedTo), "atomic-replace (rename-over) detected as MovedTo");

	// 4. delete: expect Deleted.
	observed = WatchFlags::None;
	::remove(path);
	pump();
	check(hasFlag(observed, WatchFlags::Deleted), "delete detected");

	// 5. re-create after delete (watch still armed on the parent dir): expect Created.
	observed = WatchFlags::None;
	writeFileContent(path, "again");
	pump();
	check(hasFlag(observed, WatchFlags::Created), "re-create after delete detected");

	// 6. shared-instance invariant: a second watch on another file must NOT open a
	//    second inotify instance — both watches ride the one per-queue reader.
	char path2[512];
	::snprintf(path2, sizeof(path2), "%s/probe2.txt", dir);
	::remove(path2);

	WatchFlags observed2 = WatchFlags::None;
	auto h2 = looper->watchFile(StringView(path2), WatchFlags::Any,
			[&observed2](WatchFlags f) -> Status {
		observed2 |= f;
		return Status::Ok;
	});
	check(h2 != nullptr, "second watchFile returns a handle");

	auto instances = countInotifyInstances();
	sprt::cout << "  inotify instances with 2 watches: " << instances << "\n";
	check(instances == 1, "two watches share a single inotify instance");

	// both watches fire independently for their own file
	observed = WatchFlags::None;
	observed2 = WatchFlags::None;
	writeFileContent(path, "p1");
	writeFileContent(path2, "p2");
	pump();
	check(hasFlag(observed, WatchFlags::Modified) || hasFlag(observed, WatchFlags::Created),
			"first watch still fires for its file");
	check(hasFlag(observed2, WatchFlags::Modified) || hasFlag(observed2, WatchFlags::Created),
			"second watch fires for its file");

	// cross-talk check: an event for path2 must not be reported to the path watch
	observed = WatchFlags::None;
	writeFileContent(path2, "p2-again");
	pump();
	check(observed == WatchFlags::None, "no cross-talk between watches in the same directory");

	h2->cancel();
	h->cancel();
	looper->run(dispatch::TimeInterval::milliseconds(100));

	::remove(path);
	::remove(path2);
	::remove(tmp);
	::rmdir(dir);

	sprt::cout << "watchFile tests completed: " << failures << " failures\n";
}

} // namespace sprt
