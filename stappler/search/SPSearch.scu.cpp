/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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
#include "SPSearchConfiguration.cc"
#include "SPSearchDistance.cc"
#include "SPSearchDistanceEdLib.cc"
#include "SPSearchIndex.cc"
#include "SPSearchParser.cc"
#include "SPSearchQuery.cc"
#include "SPSearchUrl.cc"

namespace STAPPLER_VERSIONIZED stappler::search {

static StemmerEnv *Configuration_makeLocalConfig(StemmerEnv *orig) {
	auto p = memory::pool::acquire();

	char buf[24] = {0};
	snprintf(buf, 24, "%p", orig);

	StemmerEnv *ret = nullptr;
	memory::pool::userdata_get((void **)&ret, buf, p);

	if (ret) {
		return ret;
	}

	ret = (StemmerEnv *)memory::pool::palloc(p, sizeof(StemmerEnv));
	memset(ret, 0, sizeof(StemmerEnv));
	ret->memalloc = orig->memalloc;
	ret->memfree = orig->memfree;
	ret->userData = p;

	if (auto env = orig->mod->create(ret)) {
		env->stem = orig->mod->stem;
		env->stopwords = orig->stopwords;
		env->mod = orig->mod;
		memory::pool::userdata_set(env, buf, nullptr, p);
		return (StemmerEnv *)env;
	}
	return nullptr;
}

static void *StemmerEnv_getUserData(StemmerEnv *env) { return env->userData; }

} // namespace stappler::search
