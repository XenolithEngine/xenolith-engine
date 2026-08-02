/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLPugSystem.h"
#include "SPFilesystem.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::pugui {

TemplateSystem::~TemplateSystem() {
	if (_context) {
		_context->setParentContext(nullptr);
		_context = nullptr;
	}
	_template = nullptr;
	if (_pool) {
		memory::pool::destroy(_pool);
		_pool = nullptr;
	}
}

bool TemplateSystem::initContext(BuilderConfig &&config) {
	if (!System::init()) {
		return false;
	}

	_config = move(config);
	_pool = memory::pool::create((memory::pool_t *)nullptr);
	if (!_pool) {
		return false;
	}

	// build the Context (and everything set into it) under the persistent pool
	memory::perform([&] {
		_context = new (sprt::nothrow) spug::Context();
		if (_context) {
			_context->loadDefaults();
		}
	}, _pool);

	return _context != nullptr;
}

bool TemplateSystem::init(StringView inlineTemplate, BuilderConfig &&config) {
	if (!initContext(move(config))) {
		return false;
	}
	setTemplate(inlineTemplate);
	return true;
}

bool TemplateSystem::init(const FileInfo &file, BuilderConfig &&config) {
	if (!initContext(move(config))) {
		return false;
	}
	setTemplateFile(file);
	return true;
}

void TemplateSystem::setTemplate(StringView inlineTemplate) {
	_source = inlineTemplate.str<mem_std::Interface>();
	_isFile = false;
	_category = FileCategory::Custom;
	_templateDirty = true;
}

void TemplateSystem::setTemplateFile(const FileInfo &file) {
	_source = file.path.str<mem_std::Interface>();
	_isFile = true;
	_category = file.category;
	_templateDirty = true;
}

void TemplateSystem::setVariable(StringView name, Value &&value) {
	if (!_context) {
		return;
	}
	memory::perform([&] { _context->set(name, spug::Value(value)); }, _pool);
}

void TemplateSystem::setVariable(StringView name, const Value &value) {
	if (!_context) {
		return;
	}
	memory::perform([&] { _context->set(name, spug::Value(value)); }, _pool);
}

void TemplateSystem::setFunction(StringView name, spug::VarClass::Callback &&fn) {
	if (!_context) {
		return;
	}
	memory::perform([&] { _context->set(name, sp::move(fn)); }, _pool);
}

void TemplateSystem::updateContext(const Callback<void(spug::Context &)> &cb) {
	if (!_context || !cb) {
		return;
	}
	memory::perform([&] { cb(*_context); }, _pool);
}

void TemplateSystem::setBuildCallback(Function<void(TemplateSystem *, SpanView<Rc<Node>>)> &&cb) {
	_buildCallback = move(cb);
}

TemplateSystem *TemplateSystem::findAncestor() const {
	for (Node *n = _owner ? _owner->getParent() : nullptr; n; n = n->getParent()) {
		if (auto s = n->getSystemByType<TemplateSystem>()) {
			return s;
		}
	}
	return nullptr;
}

void TemplateSystem::ensureTemplate() {
	if (_template && !_templateDirty) {
		return;
	}

	Callback<void(StringView)> errCb = [this](StringView err) {
		if (_config.onError) {
			_config.onError(err);
		} else {
			log::source().warn("pugui::TemplateSystem", err);
		}
	};

	memory::perform([&] {
		if (_isFile) {
			auto bytes =
					filesystem::readIntoMemory<mem_std::Interface>(FileInfo{_source, _category});
			StringView content((const char *)bytes.data(), bytes.size());
			_template = spug::Template::read(_pool, content, spug::Template::Options::getNodes(),
					errCb);
		} else {
			_template = spug::Template::read(_pool, _source, spug::Template::Options::getNodes(),
					errCb);
		}
	}, _pool);

	_templateDirty = false;
}

void TemplateSystem::build() {
	if (!_owner) {
		return;
	}

	teardown();
	ensureTemplate();
	if (!_template) {
		_built = false;
		return;
	}

	// cascade: names unresolved here fall through to the nearest ancestor system's Context
	auto ancestor = findAncestor();
	_context->setParentContext(ancestor ? ancestor->getContext() : nullptr);

	// a stylesheet is attached to the owner once (NodeBuilder would re-add it every build)
	if (_config.styleSheet && !_styleSheetAttached) {
		_owner->addSystem(Rc<StyleSheetSystem>::create(Rc<StyleSheet>(_config.styleSheet)));
		_styleSheetAttached = true;
	}

	Vector<Node *> before;
	for (auto &c : _owner->getChildren()) { before.emplace_back(c.get()); }

	// run the persistent Context; run-time temporaries go to a throwaway scratch pool so
	// the persistent pool does not grow across rebuilds
	auto scratch = memory::pool::create(_pool);
	memory::perform([&] {
		BuilderConfig cfg = _config;
		if (cfg.styleSheet) {
			// keep per-node StyleApplier, but do not let NodeBuilder re-attach the sheet system
			cfg.enableStyles = true;
			cfg.styleSheet = nullptr;
		}
		NodeBuilder builder(_owner, move(cfg));
		_built = _template->run(*_context, builder) && builder.isValid();
	}, scratch);
	memory::pool::destroy(scratch);

	_builtNodes.clear();
	for (auto &c : _owner->getChildren()) {
		bool existed = false;
		for (auto b : before) {
			if (b == c.get()) {
				existed = true;
				break;
			}
		}
		if (!existed) {
			_builtNodes.emplace_back(c);
		}
	}

	if (_buildCallback) {
		_buildCallback(this, _builtNodes);
	}
}

void TemplateSystem::teardown() {
	for (auto &n : _builtNodes) {
		if (n->getParent() == _owner) {
			n->removeFromParent();
		}
	}
	_builtNodes.clear();
	_built = false;
}

void TemplateSystem::rebuild() {
	if (_owner) {
		build();
	}
}

void TemplateSystem::handleEnter(Scene *scene) {
	System::handleEnter(scene);
	if (!_built) {
		build();
	}
}

void TemplateSystem::handleRemoved() {
	teardown();
	if (_context) {
		_context->setParentContext(nullptr);
	}
	System::handleRemoved();
}

} // namespace stappler::xenolith::pugui
