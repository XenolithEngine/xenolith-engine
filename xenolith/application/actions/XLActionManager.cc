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

#include "XLActionManager.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ActionContainer::~ActionContainer() { }

ActionContainer::ActionContainer(Node *t) : target(t) { }

ActionManager::~ActionManager() { removeAllActions(); }

ActionManager::ActionManager() { }

bool ActionManager::init() { return true; }

void ActionManager::addAction(Action *action, Node *target, bool paused) {
	if (_inUpdate) {
		_pending.emplace_back(PendingAction{action, target, paused});
		return;
	}

	XLASSERT(action != nullptr, "");
	XLASSERT(target != nullptr, "");

	auto it = _actions.find(target);
	if (it == _actions.end()) {
		it = _actions.emplace(target).first;
		it->paused = paused;
	}

	action->setContainer(target);
	it->addItem(action);
	action->startWithTarget(target);
}

void ActionManager::removeAllActions() {
	if (_inUpdate) {
		auto it = _actions.begin();
		while (it != _actions.end()) {
			it->foreach ([&](Action *a) {
				a->invalidate();
				return true;
			});
			++it;
		}
	} else {
		_actions.clear();
	}
}

void ActionManager::removeAllActionsFromTarget(Node *target) {
	if (target == nullptr) {
		return;
	}

	if (_current && _current->target == target) {
		_current->foreach ([&](Action *a) {
			a->invalidate();
			return true;
		});
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			if (_inUpdate) {
				it->foreach ([&](Action *a) {
					a->invalidate();
					return true;
				});
			} else {
				_actions.erase(it);
			}
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target) {
			it = _pending.erase(it);
		} else {
			++it;
		}
	}
}

void ActionManager::removeAction(Action *action) {
	auto target = action->getContainer();
	if (_current && _current->target == target) {
		action->invalidate();
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			it->removeItem(action);
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->action == action) {
			it = _pending.erase(it);
			break;
		} else {
			++it;
		}
	}
}

void ActionManager::removeActionByTag(uint32_t tag, Node *target) {
	if (_current && _current->target == target) {
		if (_current->invalidateItemByTag(tag)) {
			return;
		}
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			if (it->removeItemByTag(tag)) {
				return;
			}
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target && it->action->getTag() == tag) {
			it = _pending.erase(it);
			break;
		} else {
			++it;
		}
	}
}

void ActionManager::removeAllActionsByTag(uint32_t tag, Node *target) {
	if (_current && _current->target == target) {
		_current->invalidateAllItemsByTag(tag);
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			it->removeAllItemsByTag(tag);
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target && it->action->getTag() == tag) {
			it = _pending.erase(it);
		} else {
			++it;
		}
	}
}

Action *ActionManager::getActionByTag(uint32_t tag, const Node *target) const {
	if (_current && _current->target == target) {
		return _current->getItemByTag(tag);
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			return it->getItemByTag(tag);
		}
	}

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target && it->action->getTag() == tag) {
			return it->action;
		} else {
			++it;
		}
	}

	return nullptr;
}

size_t ActionManager::getNumberOfRunningActionsInTarget(const Node *target) const {
	size_t pending = 0;

	auto it = _pending.begin();
	while (it != _pending.end()) {
		if (it->target == target) {
			++pending;
		}
		++it;
	}

	if (_current && _current->target == target) {
		return _current->size() + pending;
	} else {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			return it->size() + pending;
		}
	}
	return 0;
}

void ActionManager::pauseTarget(Node *target) {
	auto it = _actions.find(target);
	if (it != _actions.end()) {
		it->paused = true;
	}
}

void ActionManager::resumeTarget(Node *target) {
	auto it = _actions.find(target);
	if (it != _actions.end()) {
		it->paused = false;
	}
}

Vector<Node *> ActionManager::pauseAllRunningActions() {
	Vector<Node *> ret;
	for (auto &it : _actions) {
		it.paused = true;
		ret.emplace_back(it.target);
	}
	return ret;
}

void ActionManager::resumeTargets(const Vector<Node *> &targetsToResume) {
	for (auto &target : targetsToResume) {
		auto it = _actions.find(target);
		if (it != _actions.end()) {
			it->paused = false;
		}
	}
}

/** Main loop of ActionManager. */
void ActionManager::update(const UpdateTime &time) {
	float dt = float(time.delta) / 1'000'000.0f;
	_inUpdate = true;
	auto it = _actions.begin();
	while (it != _actions.end()) {
		_current = &(*it);
		_current->foreach ([&](Action *a) {
			if (a->getTarget()) {
				a->step(dt);
				if (a->isDone()) {
					a->stop();
				}
			} else {
				a->stop();
			}
			return true;
		});
		_current = nullptr;
		++it;
	}
	_inUpdate = false;

	it = _actions.begin();
	while (it != _actions.end()) {
		if (it->cleanup()) {
			auto iit = _actions.find(it->target);
			if (iit != _actions.end()) {
				it = _actions.erase(it);
			} else {
				log::source().debug("ActionManager", "update: ", time.app,
						" erase: ", (void *)it->target.get());
			}
		} else {
			++it;
		}
	}

	for (auto &it : _pending) { addAction(it.action, it.target, it.paused); }
	_pending.clear();
}

bool ActionManager::empty() const { return _actions.empty() && _pending.empty(); }

} // namespace stappler::xenolith
