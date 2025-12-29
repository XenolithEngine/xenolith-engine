/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "private/SPRTMemStruct.h"

namespace sprt::memory::apr {

bool AprInterface::load(Dso &&handle) {
	if (handle) {
		SPRT_LOAD_PROTO(handle, apr_allocator_create);
		SPRT_LOAD_PROTO(handle, apr_allocator_destroy);
		SPRT_LOAD_PROTO(handle, apr_allocator_mutex_set);
		SPRT_LOAD_PROTO(handle, apr_allocator_owner_set);
		SPRT_LOAD_PROTO(handle, apr_allocator_owner_get);
		SPRT_LOAD_PROTO(handle, apr_allocator_max_free_set);
		SPRT_LOAD_PROTO(handle, apr_pool_initialize);
		SPRT_LOAD_PROTO(handle, apr_pool_terminate);
		SPRT_LOAD_PROTO(handle, apr_pool_create_unmanaged_ex);
		SPRT_LOAD_PROTO(handle, apr_pool_create_ex);
		SPRT_LOAD_PROTO(handle, apr_pool_tag);
		SPRT_LOAD_PROTO(handle, apr_pool_destroy);
		SPRT_LOAD_PROTO(handle, apr_pool_clear);
		SPRT_LOAD_PROTO(handle, apr_palloc);
		SPRT_LOAD_PROTO(handle, apr_pool_cleanup_kill);
		SPRT_LOAD_PROTO(handle, apr_pool_cleanup_null);
		SPRT_LOAD_PROTO(handle, apr_pool_cleanup_register);
		SPRT_LOAD_PROTO(handle, apr_pool_pre_cleanup_register);
		SPRT_LOAD_PROTO(handle, apr_pool_userdata_set);
		SPRT_LOAD_PROTO(handle, apr_pool_userdata_setn);
		SPRT_LOAD_PROTO(handle, apr_pool_userdata_get);
		SPRT_LOAD_PROTO(handle, apr_pool_allocator_get);
		SPRT_LOAD_PROTO(handle, apr_pmemdup);
		SPRT_LOAD_PROTO(handle, apr_pstrdup);

		if (validateFunctionList(&apr_first_fn, &apr_last_fn)) {
			_handle = sprt::move(handle);
			return true;
		}
	}
	return false;
}

AprInterface::operator bool() const { return _handle.isValid(); }

} // namespace sprt::memory::apr
