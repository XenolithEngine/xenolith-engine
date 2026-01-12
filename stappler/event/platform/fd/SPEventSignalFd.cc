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

#include "SPEventSignalFd.h"
#include "../uring/SPEvent-uring.h"
#include "../epoll/SPEvent-epoll.h"
#include "../android/SPEvent-alooper.h"

#include <signal.h>
#include <unistd.h>

#if __SPRT_CONFIG_HAVE_SIGNALFD

namespace STAPPLER_VERSIONIZED stappler::event {

// see https://stackoverflow.com/questions/57299119/iterating-over-a-list-of-possible-signals
struct siginfo {
	int code;
	const char *name;
	const char *description;
};

static const siginfo siglist[] = {
#ifdef SIGABRT
	{SIGABRT, "SIGABRT", "Abort signal"},
#endif
#ifdef SIGALRM
	{SIGALRM, "SIGALRM", "Timer signal"},
#endif
#ifdef SIGBUS
	{SIGBUS, "SIGBUS", "Bus error (bad memory access)"},
#endif
#ifdef SIGCHLD
	{SIGCHLD, "SIGCHLD", "Child stopped or terminated"},
#endif
#ifdef SIGCLD
	{SIGCLD, "SIGCLD", "Child stopped or terminated"},
#endif
#ifdef SIGCONT
	{SIGCONT, "SIGCONT", "Continue if stopped"},
#endif
#ifdef SIGEMT
	{SIGEMT, "SIGEMT", "Emulator trap"},
#endif
#ifdef SIGFPE
	{SIGFPE, "SIGFPE", "Floating-point exception"},
#endif
#ifdef SIGHUP
	{SIGHUP, "SIGHUP", "Hangup detected on controlling terminal or death of controlling process"},
#endif
#ifdef SIGILL
	{SIGILL, "SIGILL", "Illegal Instruction"},
#endif
#ifdef SIGINFO
	{SIGINFO, "SIGINFO", "Power failure"},
#endif
#ifdef SIGINT
	{SIGINT, "SIGINT", "Interrupt from keyboard"},
#endif
#ifdef SIGIO
	{SIGIO, "SIGIO", "I/O now possible"},
#endif
#ifdef SIGIOT
	{SIGIOT, "SIGIOT", "IOT trap: Abort signal"},
#endif
#ifdef SIGKILL
	{SIGKILL, "SIGKILL", "Kill signal"},
#endif
#ifdef SIGLOST
	{SIGLOST, "SIGLOST", "File lock lost "},
#endif
#ifdef SIGPIPE
	{SIGPIPE, "SIGPIPE", "Broken pipe: write to pipe with no readers"},
#endif
#ifdef SIGPOLL
	{SIGPOLL, "SIGPOLL", "Pollable event: I/O now possible"},
#endif
#ifdef SIGPROF
	{SIGPROF, "SIGPROF", "Profiling timer expired"},
#endif
#ifdef SIGPWR
	{SIGPWR, "SIGPWR", "Power failure"},
#endif
#ifdef SIGQUIT
	{SIGQUIT, "SIGQUIT", "Quit from keyboard"},
#endif
#ifdef SIGSEGV
	{SIGSEGV, "SIGSEGV", "Invalid memory reference"},
#endif
#ifdef SIGSTKFLT
	{SIGSTKFLT, "SIGSTKFLT", "Stack fault on coprocessor"},
#endif
#ifdef SIGSTOP
	{SIGSTOP, "SIGSTOP", "Stop process"},
#endif
#ifdef SIGTSTP
	{SIGTSTP, "SIGTSTP", "Stop typed at terminal"},
#endif
#ifdef SIGSYS
	{SIGSYS, "SIGSYS", "Bad system call "},
#endif
#ifdef SIGTERM
	{SIGTERM, "SIGTERM", "Termination signal"},
#endif
#ifdef SIGTRAP
	{SIGTRAP, "SIGTRAP", "Trace/breakpoint trap"},
#endif
#ifdef SIGTTIN
	{SIGTTIN, "SIGTTIN", "Terminal input for background process"},
#endif
#ifdef SIGTTOU
	{SIGTTOU, "SIGTTOU", "Terminal output for background process"},
#endif
#ifdef SIGUNUSED
	{SIGUNUSED, "SIGUNUSED", "Bad system call "},
#endif
#ifdef SIGURG
	{SIGURG, "SIGURG", "Urgent condition on socket "},
#endif
#ifdef SIGUSR1
	{SIGUSR1, "SIGUSR1", "User-defined signal 1"},
#endif
#ifdef SIGUSR2
	{SIGUSR2, "SIGUSR2", "User-defined signal 2"},
#endif
#ifdef SIGVTALRM
	{SIGVTALRM, "SIGVTALRM", "Virtual alarm clock "},
#endif
#ifdef SIGXCPU
	{SIGXCPU, "SIGXCPU", "CPU time limit exceeded "},
#endif
#ifdef SIGXFSZ
	{SIGXFSZ, "SIGXFSZ", "File size limit exceeded"},
#endif
#ifdef SIGWINCH
	{SIGWINCH, "SIGWINCH", "Window resize signal"},
#endif
};

static const siginfo *getSignalInfo(int sig) {
	for (size_t i = 0; i < sizeof(siglist) / sizeof(siginfo); ++i) {
		if (siglist[i].code == sig) {
			return &siglist[i];
		}
	}
	return nullptr;
}

bool SignalFdSource::init(const __sprt_sigset_t *sig) {
	fd = ::__sprt_signalfd(-1, sig, __SPRT_SFD_CLOEXEC | __SPRT_SFD_NONBLOCK);
	if (fd < 0) {
		return false;
	}

	return true;
}

void SignalFdSource::cancel() {
	if (fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

bool SignalFdHandle::init(HandleClass *cl, SpanView<int> sigs) {
	static_assert(
			sizeof(SignalFdSource) <= DataSize && std::is_standard_layout<SignalFdSource>::value);

	if (!Handle::init(cl,
				CompletionHandle<SignalFdHandle>::create<SignalFdHandle>(this,
						[](SignalFdHandle *, SignalFdHandle *h, uint32_t value, Status st) {
		if (isSuccessful(st)) {
			h->process();
		}
	}))) {
		return false;
	}

	sigemptyset(&_sigset);
	sigemptyset(&_default);

	for (auto &it : sigs) { ::sigaddset(&_default, it); }

	auto source = reinterpret_cast<SignalFdSource *>(_data);

	return source->init(&_sigset);
}

bool SignalFdHandle::read() {
	auto source = reinterpret_cast<SignalFdSource *>(_data);

	auto s = ::read(source->fd, &_info, sizeof(signalfd_siginfo));
	if (s == sizeof(signalfd_siginfo)) {
		process();
		return true;
	}
	return false;
}

#if ANDROID
int sigisemptyset(const sigset_t *set) {
	sigset_t sigset;
	::sigemptyset(&sigset);

	return memcmp(set, &sigset, sizeof(sigset_t)) == 0;
}
#endif

bool SignalFdHandle::process() {
	if (_info.ssi_signo == 0) {
		return false;
	}

	auto sig = getSignalInfo(_info.ssi_signo);
	if (sig) {
		log::source().info("event::Queue", "Signal intercepted with signalfd: ", sig->name,
				", errno: ", _info.ssi_errno);
	} else {
		log::source().info("event::Queue",
				"Signal intercepted with signalfd: (unknown), errno: ", _info.ssi_errno);
	}

	sigset_t sigset;
	::sigemptyset(&sigset);
	::sigpending(&sigset);
	if (!sigisemptyset(&sigset) && ::sigismember(&sigset, _info.ssi_signo)) {
		::sigemptyset(&sigset);
		::sigaddset(&sigset, _info.ssi_signo);
		::sigsuspend(&sigset);
	}

	memset(&_info, 0, sizeof(signalfd_siginfo));

	return true;
}

void SignalFdHandle::enable() {
	sigset_t sigset;
	::sigemptyset(&sigset);
	::sigprocmask(SIG_UNBLOCK, nullptr, &sigset);

	enable(&sigset);
}

void SignalFdHandle::enable(const sigset_t *sigset) {
	auto source = reinterpret_cast<SignalFdSource *>(_data);

	::sigorset(&_sigset, sigset, &_default);

	mem_std::StringStream signals;
	for (size_t i = 0; i < sizeof(siglist) / sizeof(siginfo); i++) {
		if (::sigismember(&_sigset, siglist[i].code)) {
			signals << " " << siglist[i].name;
		}
	}

	log::source().debug("event::Queue", "signalfd enabled:", signals.str());

	::__sprt_signalfd(source->fd, &_sigset, __SPRT_SFD_NONBLOCK | __SPRT_SFD_CLOEXEC);
}

void SignalFdHandle::disable() {
	auto source = reinterpret_cast<SignalFdSource *>(_data);

	::sigemptyset(&_sigset);
	::__sprt_signalfd(source->fd, &_sigset, __SPRT_SFD_NONBLOCK | __SPRT_SFD_CLOEXEC);
}

const sigset_t *SignalFdHandle::getDefaultSigset() const { return &_default; }

const sigset_t *SignalFdHandle::getCurrentSigset() const { return &_sigset; }

Status SignalFdURingHandle::rearm(URingData *uring, SignalFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		status = uring->pushRead(source->fd, (uint8_t *)&_info, sizeof(signalfd_siginfo),
				reinterpret_cast<uintptr_t>(this) | (_timeline & URING_USERDATA_SERIAL_MASK));
		if (status == Status::Suspended) {
			status = uring->submit();
		}
	}
	return status;
}

Status SignalFdURingHandle::disarm(URingData *uring, SignalFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = uring->cancelOp(reinterpret_cast<uintptr_t>(this)
						| (_timeline & URING_USERDATA_SERIAL_MASK),
				URingCancelFlags::Suspend);
		++_timeline;
	}
	return status;
}

void SignalFdURingHandle::notify(URingData *uring, SignalFdSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	_status = Status::Suspended;

	if (data.result == sizeof(signalfd_siginfo)) {
		if (_status == Status::Suspended) {
			rearm(uring, source);
		}

		sendCompletion(0, Status::Ok);
		process();
	} else {
		cancel(URingData::getErrnoStatus(data.result));
	}
}

Status SignalFdEPollHandle::rearm(EPollData *epoll, SignalFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		source->event.data.ptr = this;
		source->event.events = __SPRT_EPOLLIN;

		status = epoll->add(source->fd, source->event);
	}
	return status;
}

Status SignalFdEPollHandle::disarm(EPollData *epoll, SignalFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = epoll->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void SignalFdEPollHandle::notify(EPollData *epoll, SignalFdSource *source, const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	bool notify = false;

	if (data.queueFlags & __SPRT_EPOLLIN) {
		while (read()) { notify = true; }
	}

	if ((data.queueFlags & __SPRT_EPOLLERR) || (data.queueFlags & __SPRT_EPOLLHUP)) {
		cancel();
	} else if (notify) {
		sendCompletion(0, Status::Ok);
	}
}

Status SignalFdALooperHandle::rearm(ALooperData *alooper, SignalFdSource *source) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		status = alooper->add(source->fd, __SPRT_ALOOPER_EVENT_INPUT, this);
	}
	return status;
}

Status SignalFdALooperHandle::disarm(ALooperData *alooper, SignalFdSource *source) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		status = alooper->remove(source->fd);
		++_timeline;
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void SignalFdALooperHandle::notify(ALooperData *alooper, SignalFdSource *source,
		const NotifyData &data) {
	if (_status != Status::Ok) {
		return;
	}

	bool notify = false;

	if (data.queueFlags & __SPRT_ALOOPER_EVENT_INPUT) {
		while (read()) { notify = true; }
	}

	if ((data.queueFlags & __SPRT_ALOOPER_EVENT_ERROR)
			|| (data.queueFlags & __SPRT_ALOOPER_EVENT_HANGUP)
			|| (data.queueFlags & __SPRT_ALOOPER_EVENT_INVALID)) {
		cancel();
	} else if (notify) {
		sendCompletion(0, Status::Ok);
	}
}

} // namespace stappler::event

#endif
