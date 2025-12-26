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

#ifndef CORE_EVENT_PLATFORM_FD_SPEVENTDIRFD_H_
#define CORE_EVENT_PLATFORM_FD_SPEVENTDIRFD_H_

#include "SPEventFd.h"

namespace STAPPLER_VERSIONIZED stappler::event {

/*class SP_PUBLIC DirFdSource : public FdSource {
public:
	bool init();
};

class SP_PUBLIC DirFdHandle : public DirHandle {
public:
	virtual ~DirFdHandle();

	bool init(QueueRef *, QueueData *, OpenDirInfo &&);

	virtual Status scan(const Callback<void(FileType, StringView)> &) override;
};

class SP_PUBLIC DirFdURingHandle : public DirFdHandle {
public:
	virtual ~DirFdURingHandle() = default;

	bool init(URingData *, OpenDirInfo &&);

	Status run(DirFdSource *);

	void notify(DirFdSource *, int32_t res, uint32_t flags, URingUserFlags uflags);
};*/

}

#endif /* CORE_EVENT_PLATFORM_FD_SPEVENTDIRFD_H_ */
