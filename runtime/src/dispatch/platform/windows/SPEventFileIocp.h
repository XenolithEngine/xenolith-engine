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

#ifndef CORE_EVENT_PLATFORM_WINDOWS_SPEVENTFILEIOCP_H_
#define CORE_EVENT_PLATFORM_WINDOWS_SPEVENTFILEIOCP_H_

#include "../fd/SPEventFile.h"

namespace sprt::dispatch {

struct IocpData;
struct FileIocpIO; // OVERLAPPED holder; defined in the .cc (avoids <windows.h> here)

// Per-handle overlapped-I/O state. The HANDLE is borrowed from the libc fd (the
// fd, opened with O_OVERLAPPED, owns the HANDLE and closes it via ~FileState), so
// it is NOT closed here. The OVERLAPPED lives in a pool-allocated FileIocpIO.
struct FileIocpSource {
	void *hFile = nullptr;
	FileIocpIO *io = nullptr; // pool-owned, non-owning here
	bool associated = false;

	void cancel(Handle *);
};

// Windows IOCP-native file handle: drives each chunk with an overlapped
// ReadFile/WriteFile on the file HANDLE, completing via the IOCP. True async
// regular-file I/O without a thread (the Windows counterpart of io_uring).
class SPRT_API FileIocpHandle : public FileHandleImpl {
public:
	virtual ~FileIocpHandle() = default;

	bool init(HandleClass *, void *hFile, FileIocpIO *io);

	Status rearm(IocpData *, FileIocpSource *);
	Status disarm(IocpData *, FileIocpSource *);
	void notify(IocpData *, FileIocpSource *, const NotifyData &);

private:
	void submitChunk(FileState *, FileIocpSource *);
	void pump(FileState *, FileIocpSource *);

	uint64_t _opRefId = 0;
	bool _opPending = false;
};

// IOCP-strategy factory. The HANDLE is fetched from the (overlapped-capable) fd.
Rc<FileHandle> makeFileIocpHandle(QueueData *, HandleClass *, Rc<FileState> &&);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_WINDOWS_SPEVENTFILEIOCP_H_ */
