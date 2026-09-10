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

#ifndef EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCHATCONFIG_H_
#define EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCHATCONFIG_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

/** Where the agent lives and what to ask it.

Three sources, in falling priority: the command line, the environment, a built-in default. The
command line half cannot be read where the rest of the application config is assembled -
DEFINE_CONFIG_FUNCTION is handed a ContextConfig and nothing else - so AgentChatConfig.cpp registers
its own `parseConfigCmd` symbol, peels its three flags off argv and hands the rest to the engine's
parser. That is the same seam tests/window uses for `--watch` (tests/window/src/app/AppSetup.cpp).

The values are resolved on the first call and kept: nothing they read can change while the app runs.
The returned views point at file-local storage that outlives every caller. */

// Base URL of an OpenAI-compatible agent, `/v1` included, no trailing slash. Default is the test
// stand on the local network; any such endpoint works - llama.cpp server, LM Studio, Ollama, or a
// cloud provider once a key is set.
StringView getAgentEndpoint();

// The model to ask for, when the user named one. Empty means "take the first one /models offers".
StringView getAgentModelHint();

// Sent as `authorization: Bearer ...`. Empty means the header is not sent at all, which is what a
// local agent expects.
StringView getAgentApiKey();

// getAgentEndpoint() + path, with exactly one slash between them.
String makeAgentUrl(StringView path);

/** A model id from a local server is usually a filesystem path to a gguf file, which is a hundred
characters of directory nobody reads. This keeps the tail, which is the part that names the model,
and shortens what is left. */
String makeModelTitle(StringView id);

} // namespace stappler::xenolith::examples

#endif // EXAMPLES_WINDOW_AGENTCHAT_SRC_AGENTCHAT_AGENTCHATCONFIG_H_
