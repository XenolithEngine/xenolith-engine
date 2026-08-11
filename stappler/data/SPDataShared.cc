/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPSharedModule.h"
#include "SPData.h"

namespace STAPPLER_VERSIONIZED stappler::data {

static SharedSymbol s_dataSharedSymbols[] = {
	// stappler_filesystem's embedded (BundleFS) reader decompresses through this symbol: a hard
	// dependency the other way round would be a cycle, since a shared stappler_data links against
	// stappler_filesystem. See SPFilesystemEmbedded.cc.
	SharedSymbol{"decompress",
		static_cast<size_t (*)(const uint8_t *, size_t, uint8_t *, size_t)>(decompress)},
	SharedSymbol{"readUrlencoded",
		static_cast<ValueTemplate<memory::PoolInterface> (*)(StringView, size_t)>(
				readUrlencoded<memory::PoolInterface>)},
	SharedSymbol{"readUrlencoded",
		static_cast<ValueTemplate<mem_std::Interface> (*)(StringView, size_t)>(
				readUrlencoded<mem_std::Interface>)},
};

SP_USED static SharedModule s_dataSharedModule(buildconfig::MODULE_STAPPLER_DATA_NAME,
		s_dataSharedSymbols, sizeof(s_dataSharedSymbols) / sizeof(SharedSymbol));

} // namespace stappler::data
