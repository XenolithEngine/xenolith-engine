/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_EVENT_SPEVENTFILEHANDLE_H_
#define CORE_EVENT_SPEVENTFILEHANDLE_H_

#include "SPEventHandle.h"

namespace STAPPLER_VERSIONIZED stappler::event {

/*class SP_PUBLIC FileOpHandle : public Handle {
public:
	virtual ~FileOpHandle() = default;

	bool init(QueueRef *, QueueData *, FileOpInfo &&);

	StringView getPath() const { return _pathname; }

protected:
	Rc<DirHandle> _root; // exists only until performed
	mem_std::String _pathname;
};

class SP_PUBLIC StatHandle : public FileOpHandle {
public:
	virtual ~StatHandle() = default;

	bool init(QueueRef *, QueueData *, StatOpInfo &&);

	const Stat &getStat() const { return _stat; }

protected:
	Stat _stat;
};

class SP_PUBLIC DirHandle : public FileOpHandle {
public:
	virtual ~DirHandle() = default;

	bool init(QueueRef *, QueueData *, OpenDirInfo &&);

	// Synchronously scan filenames in dir
	virtual Status scan(const Callback<void(FileType, StringView)> &) { return Status::ErrorNotImplemented; }
};

class SP_PUBLIC InputOutputHandle : public Handle {

};

class SP_PUBLIC FileHandle : public InputOutputHandle {

};*/

}

#endif /* CORE_EVENT_SPEVENTFILEHANDLE_H_ */
