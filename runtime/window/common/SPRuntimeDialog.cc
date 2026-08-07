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

#include <sprt/runtime/window/dialog.h>
#include <sprt/runtime/window/controller.h>
#include <sprt/runtime/window/native_window.h>

namespace sprt::window {

bool DialogHandle::init(NotNull<ContextController> controller, NotNull<dispatch::Looper> target,
		Rc<DialogRequest> &&req, NativeWindow *parent) {
	if (!req || !req->callback) {
		return false;
	}
	_controller = controller;
	_target = target;
	_request = sprt::move(req);
	_parent = parent;
	_active = true;
	return true;
}

Status DialogHandle::cancel(Status st) {
	if (!_active) {
		return Status::ErrorAlreadyPerformed;
	}
	finalize(st);
	return Status::Ok;
}

void DialogHandle::finalize(Status st) {
	DialogResult result;
	result.status = st;
	finalize(sprt::move(result));
}

void DialogHandle::finalize(DialogResult &&result) {
	if (!_active) {
		return;
	}
	_active = false;
	result.type = _request->type;

	// Unregistering drops the registry's reference — which may well be the last one — and is also
	// what releases the modal block, so hold ourselves across it.
	Rc<DialogHandle> hold(this);
	if (_controller) {
		_controller->unregisterDialog(this);
	}

	// The looper retains `this` for the lifetime of the queued task — the same keep-alive strategy
	// used everywhere else in the engine. Note that when the target looper IS the calling thread,
	// performOnThread runs the callback in place; that is harmless, because `_active` is already
	// cleared and a re-entrant finalize() therefore returns above.
	_target->performOnThread([this, result = sprt::move(result)]() mutable {
		if (_request->callback) {
			_request->callback(result);
		}
		// Drop the caller's captures on the thread that owns them.
		_request->callback = nullptr;
		_request->target = nullptr;
	}, this, false, "DialogHandle::finalize");
}

Rc<DialogRequest> DialogHandle::abandon() {
	if (!_active) {
		return nullptr;
	}
	_active = false;

	// Same ordering as finalize(): unregistering releases the modal block and may drop the last
	// reference to us, so hold across it. The block is immediately re-taken by whichever backend
	// picks the request up, and nothing can observe the gap — no input is dispatched between here
	// and the caller's next statement.
	Rc<DialogHandle> hold(this);
	if (_controller) {
		_controller->unregisterDialog(this);
	}

	auto req = sprt::move(_request);
	_request = nullptr;
	return req;
}

Status ContextController::declineDialog(NotNull<dispatch::Looper> target, Rc<DialogRequest> &&req,
		Status st) {
	if (!req || !req->callback) {
		return Status::ErrorInvalidArguemnt; // there is nobody to answer
	}

	// Answer through the same path a real dialog would, so the caller never has to special-case
	// "the callback will not come". Deliberately not registered: it finishes on the spot and never
	// takes a modal block.
	auto handle = Rc<DialogHandle>::create(this, target, sprt::move(req), nullptr);
	if (!handle) {
		return Status::ErrorInvalidArguemnt;
	}
	handle->cancel(st);
	return st;
}

Status ContextController::openDialog(NotNull<dispatch::Looper> target, Rc<DialogRequest> &&req) {
	// No backend on this platform at all.
	return declineDialog(target, sprt::move(req), Status::ErrorNotImplemented);
}

Status ContextController::cancelDialog(NotNull<DialogRequest> req) {
	for (auto &it : _dialogs) {
		for (auto &handle : it.second) {
			if (handle && handle->getRequest() == req) {
				return handle->cancel(Status::ErrorCancelled);
			}
		}
	}
	return Status::ErrorNotFound;
}

void ContextController::registerDialog(NotNull<DialogHandle> handle) {
	auto parent = handle->getParent();

	// `map[key]` is a lazy proxy that traps when read on a missing key, so every insert-or-modify
	// here goes through find/emplace instead.
	auto it = _dialogs.find(parent);
	if (it == _dialogs.end()) {
		it = _dialogs.emplace(parent, Vector<Rc<DialogHandle>>()).first;
	}
	it->second.emplace_back(handle);

	if (parent && hasFlag(handle->getRequest()->flags, DialogFlags::Modal)) {
		retainModalBlock(parent);
	}
}

void ContextController::unregisterDialog(NotNull<DialogHandle> handle) {
	auto parent = handle->getParent();
	auto it = _dialogs.find(parent);
	if (it == _dialogs.end()) {
		return;
	}

	for (auto iit = it->second.begin(); iit != it->second.end(); ++iit) {
		if (*iit == handle) {
			it->second.erase(iit);
			break;
		}
	}
	if (it->second.empty()) {
		_dialogs.erase(it);
	}

	if (parent && hasFlag(handle->getRequest()->flags, DialogFlags::Modal)) {
		releaseModalBlock(parent);
	}
}

void ContextController::cancelWindowDialogs(NotNull<NativeWindow> w, Status st) {
	auto it = _dialogs.find(w.get());
	if (it == _dialogs.end()) {
		return;
	}

	// Copy first: cancel() unregisters, which mutates the very vector being walked.
	auto handles = it->second;
	for (auto &handle : handles) {
		if (handle) {
			handle->cancel(st);
		}
	}
	_dialogs.erase(w.get());
	_modalBlocks.erase(w.get());
}

void ContextController::raiseWindowDialogs(NotNull<NativeWindow> w) {
	auto it = _dialogs.find(w.get());
	if (it != _dialogs.end()) {
		for (auto &handle : it->second) {
			if (handle && handle->isActive()) {
				handle->raise();
			}
		}
	}

	// Modal Dialog WINDOWS block their parent through the same counter, so a click on the blocked
	// parent has to point at them too — otherwise the user gets a window that ignores them with
	// nothing visible to explain why.
	//
	// DemandsAttention rather than a raise primitive: it is the portable "look at me" every desktop
	// backend already implements, and it leaves the decision to the WM, which is what the user's
	// focus-stealing settings are for. A linear scan is fine; the live window count is tiny.
	auto info = w->getInfo();
	if (!info) {
		return;
	}
	for (auto *child : _allWindows) {
		auto ci = child->getInfo();
		if (ci && ci->type == WindowType::Dialog && hasFlag(ci->flags, WindowCreationFlags::Modal)
				&& ci->parent == info->id
				&& hasFlag(ci->capabilities, WindowCapabilities::DemandsAttentionState)) {
			child->enableState(WindowState::DemandsAttention);
		}
	}
}

void ContextController::trackHeldInput(NotNull<NativeWindow> w, const Vector<InputEventData> &ev) {
	// Matching is by `id` for pointers and by keycode for keys; the two never collide because the
	// event name distinguishes them.
	auto isSameInput = [](const InputEventData &a, const InputEventData &b) {
		if (a.isKeyEvent() != b.isKeyEvent()) {
			return false;
		}
		return a.isKeyEvent() ? a.key.keycode == b.key.keycode : a.id == b.id;
	};

	Vector<InputEventData> *held = nullptr;
	auto ensureHeld = [&]() -> Vector<InputEventData> & {
		if (!held) {
			auto it = _heldInput.find(w.get());
			if (it == _heldInput.end()) {
				it = _heldInput.emplace(w.get(), Vector<InputEventData>()).first;
			}
			held = &it->second;
		}
		return *held;
	};

	for (auto &it : ev) {
		switch (it.event) {
		case InputEventName::Begin:
		case InputEventName::KeyPressed: {
			auto &list = ensureHeld();
			for (auto iit = list.begin(); iit != list.end(); ++iit) {
				if (isSameInput(*iit, it)) {
					list.erase(iit);
					break;
				}
			}
			list.emplace_back(it);
			break;
		}
		case InputEventName::End:
		case InputEventName::Cancel:
		case InputEventName::KeyReleased:
		case InputEventName::KeyCanceled: {
			auto mapIt = _heldInput.find(w.get());
			if (mapIt == _heldInput.end()) {
				break;
			}
			auto &list = mapIt->second;
			for (auto iit = list.begin(); iit != list.end(); ++iit) {
				if (isSameInput(*iit, it)) {
					list.erase(iit);
					break;
				}
			}
			if (list.empty()) {
				_heldInput.erase(mapIt);
				held = nullptr;
			}
			break;
		}
		default:
			// Move / MouseMove / Scroll / KeyRepeated / WindowState carry no press transition.
			break;
		}
	}
}

void ContextController::cancelWindowInput(NotNull<NativeWindow> w) {
	auto it = _heldInput.find(w.get());
	if (it == _heldInput.end()) {
		return;
	}

	// Echo each held press back with its cancelling counterpart, so every InputListener sees the
	// gesture terminate instead of hanging.
	auto events = sprt::move(it->second);
	_heldInput.erase(it);

	for (auto &event : events) {
		event.event = event.isKeyEvent() ? InputEventName::KeyCanceled : InputEventName::Cancel;
	}
	_context->handleNativeWindowInputEvents(w, sprt::move(events));
}

bool ContextController::isModalBlocked(NotNull<NativeWindow> w) const {
	auto it = _modalBlocks.find(w.get());
	return it != _modalBlocks.end() && it->second > 0;
}

void ContextController::retainModalBlock(NotNull<NativeWindow> w) {
	auto it = _modalBlocks.find(w.get());
	if (it == _modalBlocks.end()) {
		it = _modalBlocks.emplace(w.get(), uint32_t(0)).first;
	}
	++it->second;

	if (it->second == 1) {
		// Entering the block. Anything held down in the scene must be released now, or an
		// InputListener keeps a phantom press for as long as the dialog is up — and past it.
		cancelWindowInput(w);

		// Clears WindowState::Enabled, so the application can see it is blocked, and adds the
		// platform's advisory hint.
		w->setModalBlocked(true);
	}
}

void ContextController::releaseModalBlock(NotNull<NativeWindow> w) {
	auto it = _modalBlocks.find(w.get());
	if (it == _modalBlocks.end()) {
		return;
	}
	if (it->second > 0) {
		--it->second;
	}
	if (it->second == 0) {
		_modalBlocks.erase(it);
		w->setModalBlocked(false);
	}
}

} // namespace sprt::window
