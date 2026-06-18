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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTFILEFD_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTFILEFD_H_

#include "SPEventFile.h"

namespace sprt::dispatch {

struct URingData;

// Empty per-handle source; the io_uring file handle keeps all its state in
// FileState (userdata). This exists only to satisfy setupUringHandleClass, which
// placement-constructs a SourceType into Handle::_data and calls cancel() on it.
struct FileSource {
	void cancel() { }
};

// io_uring-native file handle: submits IORING_OP_READ/WRITE for each chunk and
// advances the shared FileState op-queue from notify(). Genuinely async — the
// loop is not blocked during disk I/O.
class SPRT_API FileURingHandle : public FileHandleImpl {
public:
	virtual ~FileURingHandle() = default;

	bool init(HandleClass *);

	Status rearm(URingData *, FileSource *);
	Status disarm(URingData *, FileSource *);
	void notify(URingData *, FileSource *, const NotifyData &);

private:
	void submitChunk(URingData *, FileState *);
	void pump(URingData *, FileState *);
};

// io_uring-strategy factory.
Rc<FileHandle> makeFileUringHandle(QueueData *, HandleClass *, Rc<FileState> &&);

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTFILEFD_H_ */
