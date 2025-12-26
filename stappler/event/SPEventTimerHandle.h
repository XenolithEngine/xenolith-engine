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

#ifndef CORE_EVENT_SPEVENTTIMERHANDLE_H_
#define CORE_EVENT_SPEVENTTIMERHANDLE_H_

#include "SPEventHandle.h"

namespace STAPPLER_VERSIONIZED stappler::event {

class SP_PUBLIC TimerHandle : public Handle {
public:
	static constexpr uint32_t Infinite = TimerInfo::Infinite;

	virtual ~TimerHandle() = default;

	// Some timers can not be reset with some other params
	// like, URing-native timer can only be oneshot or infinite timer
	// If 'completion' is not specified - it stays the same
	// If 'completion' is specified - userdata will be cleared
	//
	// Be careful, when resetting timers with Function callback
	//  - this callback will become invalid when new 'completion' set
	virtual bool reset(TimerInfo &&) = 0;
};

} // namespace stappler::event

#endif /* CORE_EVENT_SPEVENTTIMERHANDLE_H_ */
