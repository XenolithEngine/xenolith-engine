/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// Compile unit for stappler_vstore: the .cc files below are include-only subunits and are never
// compiled on their own.

#include "SPCommon.h"
#include "SPVStore.h"

#include "SPVStoreArena.cc"
#include "SPVStoreCodec.cc"
#include "SPVStoreJournal.cc"

namespace STAPPLER_VERSIONIZED stappler::vstore {

// adoptFrom is a member template, so instantiating ArenaT does not instantiate it: one line per pair
// of real kinds, diagonal included. ShadowArena is TrackedArena in a release build, so naming it there
// would instantiate the same specialization twice.
#if DEBUG
#define SP_VSTORE_ARENA_PAIRS(M) \
	M(PlainArena, PlainArena) M(PlainArena, TrackedArena) M(PlainArena, ShadowArena) \
	M(TrackedArena, PlainArena) M(TrackedArena, TrackedArena) M(TrackedArena, ShadowArena) \
	M(ShadowArena, PlainArena) M(ShadowArena, TrackedArena) M(ShadowArena, ShadowArena)
#else
#define SP_VSTORE_ARENA_PAIRS(M) \
	M(PlainArena, PlainArena) M(PlainArena, TrackedArena) \
	M(TrackedArena, PlainArena) M(TrackedArena, TrackedArena)
#endif

#define SP_VSTORE_INSTANTIATE_MOVE(D, S) template Status D::adoptFrom<S::TrackingType>(S &);
SP_VSTORE_ARENA_PAIRS(SP_VSTORE_INSTANTIATE_MOVE)

#undef SP_VSTORE_INSTANTIATE_MOVE
#undef SP_VSTORE_ARENA_PAIRS

} // namespace stappler::vstore
