/**
 Copyright (c) 2026 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SAVEDPANEL_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SAVEDPANEL_H_

#include "agentchat/ChatBubble.h"
#include "agentchat/InspectorCommands.h"
#include "agentchat/SavedMessages.h"
#include "XLUiButton.h"
#include "XLUiScrollSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

// The rules for the saved column, added to the sheet the scene content carries.
StringView getSavedPanelStylesheet();

/** The left-hand column: what the agent was asked to remember.

It owns no data - the list is counted and shared with the chat column, which is the only thing the
two have in common. What it does own is the view of that list, and it rebuilds the whole view when
the list changes: a column of notes is short, and rebuilding it is both correct and shorter to read
than keeping cards in step one at a time. A list long enough for that to matter wants ui::TreeView,
not a column of live nodes.

The panel is built lazily by the dock, so entries can already be there when it appears. Reading the
whole list on arrival is what makes the order not matter. */
class SavedPanel : public Node {
public:
	virtual ~SavedPanel() = default;

	virtual bool init(Rc<SavedMessages> &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;
	virtual void update(const UpdateTime &) override;

	// Drops every card and builds them again from the list.
	void rebuild();

protected:
	using Node::init;

	void buildBar();
	void buildList();
	void applyWrapWidth();

	void registerCommands();

	Rc<SavedMessages> _saved;

	Node *_bar = nullptr;
	basic2d::Label *_countLabel = nullptr;
	ui::Button *_clearButton = nullptr;

	Node *_list = nullptr;
	basic2d::Label *_emptyLabel = nullptr;

	Vector<ChatBubble *> _cards;

	float _wrapWidth = 0.0f;

	CommandScope _commands;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SAVEDPANEL_H_
