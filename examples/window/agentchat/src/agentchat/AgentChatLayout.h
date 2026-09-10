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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCHATLAYOUT_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCHATLAYOUT_H_

#include "agentchat/ChatPanel.h"
#include "agentchat/InspectorCommands.h"
#include "agentchat/SavedMessages.h"
#include "agentchat/SavedPanel.h"
#include "XL2dSceneLayout.h"
#include "XLUiDockSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/* The example's stylesheet, for whoever installs it.

IT GOES ON THE SCENE CONTENT, not on this layout. The model picker opens its list as an in-scene
overlay pushed onto the SceneContent, which makes it a SIBLING of this layout rather than a
descendant - a resolver installed here would never reach it, and the list would come up as an
unstyled white box. Same reasoning, and the same placement, as the form example.

The text is assembled from pieces that live beside the code they paint: the frame and the dock here,
the chat column in ChatPanel.cpp, the saved column in SavedPanel.cpp. One sheet, because a node
carries one StyleSystem and a second one would never be found. */
StringView getAgentChatStylesheet();

/** Two columns over a dock: what the agent was asked to remember on the left, the conversation on
the right.

WHAT THIS CLASS IS, after the conversation moved into its own panel: the host of the dock and
nothing else. It owns the split, the two panels and the list they share, and it exposes the dock
itself to the inspector. Everything about talking to an agent is in ChatPanel.

WHY THE COLUMNS DO NOT MOVE. The dock is a real one - the divider is live and the layout is a tree -
but the panels are declared without Closable and Movable and the frames without AllowSplit, AllowDrop
and AllowClose. There is nothing to drag, nothing to close and nowhere to drop, so the arrangement
that opens is the arrangement that stays.

THE ORDER THINGS ARE BUILT IN matters once: the panels are constructed HERE, in init(), and the
dock's lazy builders only hand them over. A builder that constructed them on first show would leave
every command and every early save reaching for a panel that does not exist yet. */
class AgentChatLayout : public basic2d::SceneLayout2d {
public:
	virtual ~AgentChatLayout() = default;

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	SavedMessages &getSavedMessages() const { return *_saved; }

protected:
	void registerPanels();
	void applyLayout();

	void registerCommands();
	Value encodeTree() const;

	Node *_background = nullptr;

	/* The dock's owner. It carries NO layout system and must never be `display: flex`: DockSystem
	writes every frame's geometry itself, and a second writer beside it is a fight rather than a
	layout - which the dock asserts about when it is added. Its size is set by hand here, because
	nothing above it lays it out either. */
	Node *_dockRoot = nullptr;
	ui::DockSystem *_dock = nullptr;

	Rc<SavedMessages> _saved;
	Rc<ChatPanel> _chatPanel;
	Rc<SavedPanel> _savedPanel;

	CommandScope _commands;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCHATLAYOUT_H_
