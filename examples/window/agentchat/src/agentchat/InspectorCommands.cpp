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

#include "agentchat/InspectorCommands.h"

#include "XLScene.h"
#include "XLSceneInspector.h"
#include "XL2dSceneContent.h" // the definition that makes a SceneContent a Node here

namespace STAPPLER_VERSIONIZED stappler::xenolith::examples {

CommandScope::~CommandScope() { detach(); }

void CommandScope::attach(Scene *scene) {
	detach();
	_scene = scene;
}

void CommandScope::detach() {
	if (_scene && !_names.empty()) {
		if (auto insp = inspector::get(_scene->getContent())) {
			for (auto &it : _names) { insp->removeCommand(it); }
		}
	}

	_names.clear();
	_scene = nullptr;
}

void CommandScope::add(StringView name, StringView description, Handler &&handler) {
	if (!_scene || !handler) {
		return;
	}

	auto added = inspector::addCommand(_scene->getContent(), name, description,
			[handler = sp::move(handler)](Value &&args, Function<void(Value &&)> &&done) {
		handler(args, sp::move(done));
	});

	if (added) {
		_names.emplace_back(name.str<Interface>());
	}
}

} // namespace stappler::xenolith::examples
