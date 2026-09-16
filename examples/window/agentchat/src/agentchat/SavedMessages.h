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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SAVEDMESSAGES_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SAVEDMESSAGES_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

struct SavedMessage {
	String title;
	String text;

	// Who put it here: the agent through its tool, or the inspector by hand. Shown under the text,
	// and it is what makes a headless check able to tell the two apart.
	String source;
};

/** What the left column holds. App thread only, and in memory only - the example is about the tool
protocol, not about a file format.

REF, AND NOT A MEMBER OF THE LAYOUT. The panels are held by the dock's panel registry, which is
owned by a system on a node - so they are destroyed with that node's subtree, AFTER the body of the
layout's destructor has already run. A list living as a plain member of the layout would be dead
while a panel could still reach it; a counted one cannot be.

The chat panel writes it (through the tool) and the saved panel reads it, so it is also the seam
between the two columns - neither knows about the other. */
class SavedMessages : public Ref {
public:
	using ChangeCallback = Function<void()>;

	virtual ~SavedMessages() = default;

	virtual bool init() { return true; }

	// `title` may be empty; the panel falls back to a generated one.
	const SavedMessage &add(StringView title, StringView text, StringView source);

	void clear();

	SpanView<SavedMessage> getEntries() const { return _entries; }
	size_t size() const { return _entries.size(); }
	bool empty() const { return _entries.empty(); }

	void setChangeCallback(ChangeCallback &&cb) { _onChange = sp::move(cb); }

	Value encode() const;

protected:
	void notify();

	Vector<SavedMessage> _entries;
	ChangeCallback _onChange;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_SAVEDMESSAGES_H_
