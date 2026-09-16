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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTTOOL_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTTOOL_H_

#include "agentchat/SavedMessages.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/** The one thing this agent can do besides talk: put a note in the left-hand column.

HOW A TOOL REACHES A MODEL. Not as prose in the prompt - as a declaration in the request's `tools`
field. The server expands it into whatever its model's template expects, and the model answers with
a structured call rather than with text that has to be fished out of a paragraph. That declaration
IS the instruction, and it travels with the very first question.

The system prompt beside it does the other half: a declaration says the tool exists, a sentence says
when calling it is the right move. Both are sent from the first request onwards.

WHAT COMES BACK. A call arrives as a name plus an arguments STRING holding JSON - not as an object,
which is why invoking one starts by parsing. A refusal is an ordinary result, not an error: the
model reads it and gets to try again. */

// The function name, as declared and as it comes back in a call.
constexpr auto SaveMessageToolName = StringView("save_message");

// The whole `tools` array, ready to be put in a request body.
Value makeToolDeclarations();

// The single declaration, for whoever wants to show it on its own.
Value makeSaveMessageTool();

// Goes into the history as the first message, before the first question.
StringView getAgentSystemPrompt();

struct ToolCallResult {
	// What goes back to the model as the content of the `tool` message.
	Value payload;

	// One line for the chat log, so a person watching sees what the agent just did.
	String summary;

	bool ok = false;
};

/* Runs a call. An unknown name, unparsable arguments and an empty text are all answered rather than
asserted: whatever went wrong, the model is the one who can fix it on the next turn. */
ToolCallResult invokeTool(SavedMessages &, StringView name, StringView argumentsJson);

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTTOOL_H_
