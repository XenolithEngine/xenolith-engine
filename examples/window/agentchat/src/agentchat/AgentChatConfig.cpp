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

#include "XLCommon.h" // IWYU pragma: keep
#include "XLContext.h"
#include "XLContextInfo.h"
#include "XLEntryPoint.h" // IWYU pragma: keep

#include "agentchat/AgentChatConfig.h"

#include <stdlib.h> // getenv: neither sprt::platform nor sprt::filesystem wraps it

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

// The agent this example talks to when nothing says otherwise: an OpenAI-compatible server on
// this machine, which is where LM Studio, llama.cpp and Ollama all put one. It is a default and
// only a default - the endpoint in use is printed in the window's status line, so what the app is
// talking to is never a guess.
static constexpr auto s_defaultEndpoint = StringView("http://localhost:1234/v1");

/* Captured from the command line by parseConfigCmd below, read by the getters. File-local statics:
there is exactly one app instance, and the parse seam runs before anything else in this file. */
static String s_endpointArg;
static String s_modelArg;
static String s_apiKeyArg;

// Resolved once on first use, from the argument, then the environment, then the default.
static String s_endpoint;
static String s_model;
static String s_apiKey;
static bool s_resolved = false;

static String readEnv(const char *name) {
	if (auto value = ::getenv(name)) {
		return String(value);
	}
	return String();
}

static void resolve() {
	if (s_resolved) {
		return;
	}
	s_resolved = true;

	s_endpoint = s_endpointArg;
	if (s_endpoint.empty()) {
		s_endpoint = readEnv("XL_AGENT_URL");
	}
	if (s_endpoint.empty()) {
		s_endpoint = readEnv("OPENAI_BASE_URL");
	}
	if (s_endpoint.empty()) {
		s_endpoint = s_defaultEndpoint.str<Interface>();
	}

	// One trailing slash here turns every request path into `//something`. llama.cpp forgives it,
	// other servers answer 404 - drop it once, here, rather than at each call site.
	while (!s_endpoint.empty() && s_endpoint.back() == '/') {
		s_endpoint.pop_back();
	}

	s_model = s_modelArg;
	if (s_model.empty()) {
		s_model = readEnv("XL_AGENT_MODEL");
	}

	s_apiKey = s_apiKeyArg;
	if (s_apiKey.empty()) {
		s_apiKey = readEnv("XL_AGENT_API_KEY");
	}
	if (s_apiKey.empty()) {
		s_apiKey = readEnv("OPENAI_API_KEY");
	}
}

/* Takes `--flag value` and `--flag=value` for the example's own three options and hands everything
else to the engine's ContextConfig parser, which would reject an option it does not know. */
static bool takeOption(StringView arg, StringView name, int argc, const char **argv, int &i,
		String &target) {
	if (arg == name) {
		if (i + 1 < argc) {
			target = String(argv[++i]);
		}
		return true;
	}

	auto prefix = toString(name, "=");
	if (arg.starts_with(StringView(prefix))) {
		target.assign(arg.data() + prefix.size(), arg.size() - prefix.size());
		return true;
	}

	return false;
}

static ContextConfig parseConfigCmd(int argc, const char **argv) {
	Vector<const char *> filtered;
	filtered.reserve(size_t(argc));

	for (int i = 0; i < argc; ++i) {
		StringView arg(argv[i]);
		if (takeOption(arg, "--agent-url", argc, argv, i, s_endpointArg)
				|| takeOption(arg, "--agent-model", argc, argv, i, s_modelArg)
				|| takeOption(arg, "--agent-key", argc, argv, i, s_apiKeyArg)) {
			continue;
		}
		filtered.emplace_back(argv[i]);
	}

	return ContextConfig(int(filtered.size()), filtered.data());
}

} // namespace

SP_USED static SharedExtension s_parseConfigCmdSymbol(buildconfig::MODULE_APPCOMMON_NAME,
		Context::SymbolParseConfigCmdName, &parseConfigCmd);

// Printed by `--help`, after the engine's own options.
SP_USED static SharedExtension s_helpStringSymbol(buildconfig::MODULE_APPCOMMON_NAME,
		Context::SymbolHelpStringName,
		Context::SymbolHelpStringSignature("Agent chat example options:\n"
										   "\t--agent-url <url> - OpenAI-compatible endpoint, `/v1` "
										   "included (env: XL_AGENT_URL)\n"
										   "\t--agent-model <id> - model to ask for (env: "
										   "XL_AGENT_MODEL)\n"
										   "\t--agent-key <key> - bearer token, if the agent wants "
										   "one (env: XL_AGENT_API_KEY)\n"));

StringView getAgentEndpoint() {
	resolve();
	return StringView(s_endpoint);
}

StringView getAgentModelHint() {
	resolve();
	return StringView(s_model);
}

StringView getAgentApiKey() {
	resolve();
	return StringView(s_apiKey);
}

namespace {

// Long enough that a model name fits, short enough to leave the status line room.
static constexpr size_t s_maxModelTitleSize = 42;

} // namespace

String makeModelTitle(StringView id) {
	auto title = id;

	auto slash = title.rfind('/');
	if (slash != maxOf<size_t>()) {
		title = StringView(title.data() + slash + 1, title.size() - slash - 1);
	}

	if (title.size() > s_maxModelTitleSize) {
		title = StringView(title.data(), s_maxModelTitleSize);
		return toString(title, "...");
	}

	return title.str<Interface>();
}

String makeAgentUrl(StringView path) {
	auto base = getAgentEndpoint();
	if (path.starts_with("/")) {
		return toString(base, path);
	}
	return toString(base, "/", path);
}

} // namespace stappler::xenolith::examples
