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

#include "agentchat/AgentTool.h"

#include "SPData.h"
#include "SPDataDecodeJson.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

namespace {

static constexpr auto s_systemPrompt = StringView(
		"You are the assistant of a desktop chat application. The application shows a column of "
		"saved notes beside this conversation, and you have one tool to put something in it: "
		"save_message. Call it when the user asks you to save, remember, note or write something "
		"down, passing the exact text they want kept and a short title for it. Do not call it for "
		"anything else, and answer normally when no saving was asked for.");

static Value makeStringProperty(StringView description) {
	Value property;
	property.setString("string", "type");
	property.setString(description, "description");
	return property;
}

// The truncated text a summary line shows, so a long note does not push everything else out.
static constexpr size_t s_maxSummaryText = 60;

static String makeSummary(StringView title, StringView text) {
	auto shown = text;
	if (shown.size() > s_maxSummaryText) {
		shown = StringView(shown.data(), s_maxSummaryText);
		return toString("saved '", title, "': ", shown, "...");
	}
	return toString("saved '", title, "': ", shown);
}

static ToolCallResult makeRefusal(StringView reason) {
	ToolCallResult result;
	result.ok = false;
	result.payload.setBool(false, "ok");
	result.payload.setString(reason, "error");
	result.summary = reason.str<Interface>();
	return result;
}

} // namespace

Value makeSaveMessageTool() {
	Value properties;
	properties.setValue(makeStringProperty("The exact text to keep."), "text");
	properties.setValue(makeStringProperty("A short title for the note, a few words at most."),
			"title");

	Value required;
	required.addString("text");

	Value parameters;
	parameters.setString("object", "type");
	parameters.setValue(sp::move(properties), "properties");
	parameters.setValue(sp::move(required), "required");

	Value function;
	function.setString(SaveMessageToolName, "name");
	function.setString("Save a note into the application's saved column. Use it when the user asks "
					   "to remember, save or write something down.",
			"description");
	function.setValue(sp::move(parameters), "parameters");

	Value tool;
	tool.setString("function", "type");
	tool.setValue(sp::move(function), "function");
	return tool;
}

Value makeToolDeclarations() {
	Value tools;
	tools.addValue(makeSaveMessageTool());
	return tools;
}

StringView getAgentSystemPrompt() { return s_systemPrompt; }

ToolCallResult invokeTool(SavedMessages &saved, StringView name, StringView argumentsJson) {
	if (name != SaveMessageToolName) {
		return makeRefusal(toString("unknown tool '", name, "'"));
	}

	/* The arguments are a STRING holding JSON, so this is a parse and not a lookup - and the result
	is read through a CONST reference. A non-const getter on a key that is not there returns the
	shared null container and asserts, and "the model left out an optional field" is the ordinary
	case here, not a bug. */
	const Value arguments = data::json::read<mem_std::Interface>(argumentsJson);
	if (!arguments.isDictionary()) {
		return makeRefusal("arguments were not a JSON object");
	}

	auto &text = arguments.getString("text");
	if (text.empty()) {
		return makeRefusal("'text' is required and was empty");
	}

	auto &title = arguments.getString("title");
	auto &entry = saved.add(title, text, "agent");

	ToolCallResult result;
	result.ok = true;
	result.payload.setBool(true, "ok");
	result.payload.setInteger(int64_t(saved.size()), "count");
	result.payload.setString(entry.text, "saved");
	result.summary = makeSummary(entry.title.empty() ? StringView("Note") : StringView(entry.title),
			entry.text);
	return result;
}

} // namespace stappler::xenolith::examples
