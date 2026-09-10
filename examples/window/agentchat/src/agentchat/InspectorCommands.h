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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_INSPECTORCOMMANDS_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_INSPECTORCOMMANDS_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Scene;

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/** The commands one node exposes to the inspector socket, and their lifetime.

Three owners here register commands - the layout, the chat panel, the saved panel - and each of them
has to take its own down when it leaves the scene: a command whose lambda captured a destroyed node
is a dangling call waiting on a socket. This is that bookkeeping, written once.

A handler is handed its arguments as a CONST reference on purpose. A script sends whatever it likes,
and the non-const data::Value getters assert on a key that is not there. */
class CommandScope {
public:
	using Handler = Function<void(const Value &, Function<void(Value &&)> &&)>;

	~CommandScope();

	// Kept so detach() can find the registry the commands went into.
	void attach(Scene *);
	void detach();

	void add(StringView name, StringView description, Handler &&);

protected:
	Scene *_scene = nullptr;
	Vector<String> _names;
};

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_INSPECTORCOMMANDS_H_
