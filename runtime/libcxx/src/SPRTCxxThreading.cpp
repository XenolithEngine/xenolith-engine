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

// Out-of-line TU for vendored libc++ threading. SPRT_STD_THREADING_SPRT selects the
// whole-stack ABI configuration (the flag must match between this build and every
// consumer TU):
//
//  - default (flag off): upstream classes; compile mutex/condition_variable/thread
//    verbatim. The overlay passes <mutex>/<thread> through to upstream libc++.
//  - flag on: the include_libc/cxx overlay backs std::mutex/condition_variable/thread
//    with sprt primitives, fully header-inline, so those upstream TUs must NOT be
//    compiled (their out-of-line member definitions do not match the overlay classes).
//    Only the residual out-of-line pieces are provided below, compiled against the
//    overlay types: the __thread_struct at-thread-exit machinery, notify_all_at_thread_exit
//    and this_thread::sleep_for (adapted from upstream thread.cpp / condition_variable.cpp).
//
// shared_mutex.cpp and call_once.cpp compile in BOTH modes: __shared_mutex_base builds
// over whatever std::mutex/condition_variable the headers provide (consistent within a
// mode), and __call_once uses raw __libcpp_mutex_t, independent of the std::mutex ABI.
// The legacy ~mutex/~condition_variable live in SPRTCxxThreadDtors.cpp (flag-off only).

// Derive the threading mode from the single source of truth that the include_libc/cxx
// overlay headers also key on: <sprt/c/bits/__sprt_config.h> defines SPRT_STD_THREADING_
// SPRT (default on). Without this include the gate below would see the macro only when a
// consumer passes -DSPRT_STD_THREADING_SPRT (tests/libcxx run.sh does; the plain
// tests/libc build does not), and would then compile the vendored mutex.cpp/condition_
// variable.cpp/thread.cpp against an overlay that is already sprt-backed -> redefinition.
// <__config> MUST come first: it defines _LIBCPP_VERSION, which __sprt_config.h reads at
// its first inclusion to lock __SPRT_STD_EXTERNAL=1 (project the std-owned types —
// compare/nothrow_t/initializer_list — onto libc++ instead of hand-defining them). Pulling
// __sprt_config.h before any libc++ header would freeze __SPRT_STD_EXTERNAL=0 and double-
// define those types.

#define _LIBCPP_BUILDING_LIBRARY

#include <__config>
#include <sprt/c/bits/__sprt_config.h>

#ifndef SPRT_STD_THREADING_SPRT

#include "libcxx/mutex.cpp"
#include "libcxx/condition_variable.cpp"
#include "libcxx/thread.cpp"

#else // SPRT_STD_THREADING_SPRT

#include <__config>
#include <__thread/support.h>
#include <__utility/pair.h>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

_LIBCPP_BEGIN_NAMESPACE_STD

// __thread_local_data + __thread_struct machinery, adapted verbatim from upstream
// thread.cpp -- here they compile against the sprt-backed overlay condition_variable
// and mutex, so the at-thread-exit notify/make-ready calls use the overlay ABI.

__thread_specific_ptr<__thread_struct> &__thread_local_data() {
	// See upstream: never destroyed, threads may outlive the static.
	alignas(__thread_specific_ptr<__thread_struct>) static char
			__b[sizeof(__thread_specific_ptr<__thread_struct>)];
	static __thread_specific_ptr<__thread_struct> *__p =
			new (__b) __thread_specific_ptr<__thread_struct>();
	return *__p;
}

template <class T>
class _LIBCPP_HIDDEN __hidden_allocator {
public:
	typedef T value_type;

	T *allocate(size_t __n) { return static_cast<T *>(::operator new(__n * sizeof(T))); }
	void deallocate(T *__p, size_t) { ::operator delete(static_cast<void *>(__p)); }

	size_t max_size() const { return size_t(~0) / sizeof(T); }
};

class _LIBCPP_HIDDEN __thread_struct_imp {
	typedef vector<__assoc_sub_state *, __hidden_allocator<__assoc_sub_state *> > _AsyncStates;
	typedef vector<pair<condition_variable *, mutex *>,
			__hidden_allocator<pair<condition_variable *, mutex *> > >
			_Notify;

	_AsyncStates async_states_;
	_Notify notify_;

	__thread_struct_imp(const __thread_struct_imp &);
	__thread_struct_imp &operator=(const __thread_struct_imp &);

public:
	__thread_struct_imp() { }
	~__thread_struct_imp();

	void notify_all_at_thread_exit(condition_variable *cv, mutex *m);
	void __make_ready_at_thread_exit(__assoc_sub_state *__s);
};

__thread_struct_imp::~__thread_struct_imp() {
	for (_Notify::iterator i = notify_.begin(), e = notify_.end(); i != e; ++i) {
		i->first->notify_all();
		i->second->unlock();
	}
	for (_AsyncStates::iterator i = async_states_.begin(), e = async_states_.end(); i != e; ++i) {
		(*i)->__make_ready();
		(*i)->__release_shared();
	}
}

void __thread_struct_imp::notify_all_at_thread_exit(condition_variable *cv, mutex *m) {
	notify_.push_back(pair<condition_variable *, mutex *>(cv, m));
}

void __thread_struct_imp::__make_ready_at_thread_exit(__assoc_sub_state *__s) {
	async_states_.push_back(__s);
	__s->__add_shared();
}

__thread_struct::__thread_struct() : __p_(new __thread_struct_imp) { }

__thread_struct::~__thread_struct() { delete __p_; }

void __thread_struct::notify_all_at_thread_exit(condition_variable *cv, mutex *m) {
	__p_->notify_all_at_thread_exit(cv, m);
}

void __thread_struct::__make_ready_at_thread_exit(__assoc_sub_state *__s) {
	__p_->__make_ready_at_thread_exit(__s);
}

namespace this_thread {

void sleep_for(const chrono::nanoseconds &ns) {
	if (ns > chrono::nanoseconds::zero()) {
		__libcpp_thread_sleep_for(ns);
	}
}

} // namespace this_thread

// from upstream condition_variable.cpp -- against the overlay cv/mutex/unique_lock.
void notify_all_at_thread_exit(condition_variable &cond, unique_lock<mutex> lk) {
	auto &tl_ptr = __thread_local_data();
	// If this thread was not created using std::thread then it will not have
	// previously allocated.
	if (tl_ptr.get() == nullptr) {
		tl_ptr.set_pointer(new __thread_struct);
	}
	__thread_local_data()->notify_all_at_thread_exit(&cond, lk.release());
}

_LIBCPP_END_NAMESPACE_STD

#endif // SPRT_STD_THREADING_SPRT

#include "libcxx/shared_mutex.cpp"
#include "libcxx/call_once.cpp"
