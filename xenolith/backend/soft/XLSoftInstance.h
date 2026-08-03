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

#ifndef XENOLITH_BACKEND_SOFT_XLSOFTINSTANCE_H_
#define XENOLITH_BACKEND_SOFT_XLSOFTINSTANCE_H_

#include "XLSoft.h"
#include "XLCoreInstance.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::soft {

class Device;

// There is exactly one "device" and it is the CPU this process runs on, so device enumeration
// is a constant: no probing, no driver load, nothing that can fail.
class SP_PUBLIC Instance final : public core::Instance {
public:
	virtual ~Instance() = default;

	Instance(core::InstanceFlags, sprt::Dso &&);

	virtual size_t getDeviceCount() const override;
	virtual bool readDeviceProperties(size_t, sprt::window::gapi::DeviceProperties &) override;

	virtual Rc<core::Loop> makeLoop(NotNull<sprt::dispatch::Looper>,
			Rc<core::LoopInfo> &&) const override;

	Rc<Device> makeDevice(const core::LoopInfo &) const;
};

} // namespace stappler::xenolith::soft

#endif /* XENOLITH_BACKEND_SOFT_XLSOFTINSTANCE_H_ */
