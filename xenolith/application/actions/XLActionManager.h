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

#ifndef XENOLITH_APPLICATION_ACTIONS_XLACTIONMANAGER_H_
#define XENOLITH_APPLICATION_ACTIONS_XLACTIONMANAGER_H_

#include "XLNodeInfo.h"
#include "XLAction.h"
#include "SPHashTable.h"
#include "SPRefContainer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Node;

struct SP_PUBLIC ActionContainer : RefContainer<Action, Interface> {
	Rc<Node> target;
	bool paused = false;

	virtual ~ActionContainer();
	ActionContainer(Node *);
};

struct SP_PUBLIC HashTraitActionContainer {
	static uint32_t hash(uint32_t salt, const ActionContainer &value) {
		auto target = value.target.get();
		return hash::hash32((const char *)&target, sizeof(Node *), salt);
	}

	static uint32_t hash(uint32_t salt, const Node *value) {
		return hash::hash32((const char *)&value, sizeof(Node *), salt);
	}

	static bool equal(const ActionContainer &l, const ActionContainer &r) {
		return l.target == r.target;
	}

	static bool equal(const ActionContainer &l, const Node *value) { return l.target == value; }
};

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler {

template <>
struct HashTraitDiscovery<xenolith::ActionContainer> {
	using type = xenolith::HashTraitActionContainer;
};

} // namespace STAPPLER_VERSIONIZED stappler

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SP_PUBLIC ActionManager : public Ref {
public:
	virtual ~ActionManager();

	ActionManager();

	bool init();

	// actions

	/** Adds an action with a target.
	 If the target is already present, then the action will be added to the existing target.
	 If the target is not present, a new instance of this target will be created either paused or not, and the action will be added to the newly created target.
	 When the target is paused, the queued actions won't be 'ticked'.
	 *
	 * @param action    A certain action.
	 * @param target    The target which need to be added an action.
	 * @param paused    Is the target paused or not.
	 */
	void addAction(Action *action, Node *target, bool paused);

	/** Removes all actions from all the targets. */
	void removeAllActions();

	/** Removes all actions from a certain target.
	 All the actions that belongs to the target will be removed.
	 *
	 * @param target    A certain target.
	 */
	void removeAllActionsFromTarget(Node *target);

	/** Removes an action given an action reference.
	 *
	 * @param action    A certain target.
	 */
	void removeAction(Action *action);

	/** Removes an action given its tag and the target.
	 *
	 * @param tag       The action's tag.
	 * @param target    A certain target.
	 */
	void removeActionByTag(uint32_t tag, Node *target);

	/** Removes all actions given its tag and the target.
	 *
	 * @param tag       The actions' tag.
	 * @param target    A certain target.
	 */
	void removeAllActionsByTag(uint32_t tag, Node *target);

	/** Gets an action given its tag an a target.
	 *
	 * @param tag       The action's tag.
	 * @param target    A certain target.
	 * @return  The Action the with the given tag.
	 */
	Action *getActionByTag(uint32_t tag, const Node *target) const;

	/** Returns the numbers of actions that are running in a certain target.
	 * Composable actions are counted as 1 action. Example:
	 * - If you are running 1 Sequence of 7 actions, it will return 1.
	 * - If you are running 7 Sequences of 2 actions, it will return 7.
	 *
	 * @param target    A certain target.
	 * @return  The numbers of actions that are running in a certain target.
	 */
	size_t getNumberOfRunningActionsInTarget(const Node *target) const;

	/** Pauses the target: all running actions and newly added actions will be paused.
	 *
	 * @param target    A certain target.
	 */
	void pauseTarget(Node *target);

	/** Resumes the target. All queued actions will be resumed.
	 *
	 * @param target    A certain target.
	 */
	void resumeTarget(Node *target);

	/** Pauses all running actions, returning a list of targets whose actions were paused.
	 *
	 * @return  A list of targets whose actions were paused.
	 */
	Vector<Node *> pauseAllRunningActions();

	/** Resume a set of targets (convenience function to reverse a pauseAllRunningActions call).
	 *
	 * @param targetsToResume   A set of targets need to be resumed.
	 */
	void resumeTargets(const Vector<Node *> &targetsToResume);

	/** Main loop of ActionManager. */
	void update(const UpdateTime &);

	bool empty() const;

protected:
	struct PendingAction {
		Rc<Action> action;
		Rc<Node> target;
		bool paused;
	};

	bool _inUpdate = false;
	ActionContainer *_current = nullptr;
	HashTable<ActionContainer> _actions;
	Vector<PendingAction> _pending;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_ACTIONS_XLACTIONMANAGER_H_ */
