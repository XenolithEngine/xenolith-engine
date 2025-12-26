/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPSharedModule.h"
#include "SPLog.h"
#include "SPMemInterface.h"
#include "SPDso.h"
#include <typeinfo>

#if LINUX || ANDROID || MACOS
#include <cxxabi.h>
#endif

namespace STAPPLER_VERSIONIZED stappler {

struct SharedModuleManager {
	using ModuleVersionMap = std::map<uint32_t, SharedModule *>;

	static SharedModuleManager *getInstance();

	void addModule(SharedModule *);
	void removeModule(SharedModule *);

	const SharedModule *openModule(StringView, uint32_t version) const;

	const void *acquireSymbol(const char *module, uint32_t version, const char *symbol,
			const SourceLocation &loc) const;
	const void *acquireSymbol(const char *module, uint32_t version, const char *symbol,
			const std::type_info *, const SourceLocation &loc) const;

	void enumerateModules(void *userdata, void (*)(void *userdata, const char *name));

	bool enumerateSymbols(const char *module, uint32_t version, void *userdata,
			void (*)(void *userdata, const char *name, const void *symbol));

	std::unordered_map<StringView, ModuleVersionMap> _modules;
	mutable std::mutex _mutex;
};

static void printDemangled(std::ostream &stream, const std::type_info *t) {
#if LINUX || ANDROID || MACOS
	int status = 0;
	auto name = ::abi::__cxa_demangle(t->name(), nullptr, nullptr, &status);
	if (status == 0) {
		stream << name;
		::free(name);
	} else {
		stream << t->name();
	}
#else
	stream << t->name();
#endif
}

SharedModuleManager *SharedModuleManager::getInstance() {
	static std::mutex s_mutex;
	static SharedModuleManager *s_instance = nullptr;

	std::unique_lock lock(s_mutex);
	if (!s_instance) {
		s_instance = new SharedModuleManager();
	}
	return s_instance;
}

void SharedModuleManager::addModule(SharedModule *module) {
	std::unique_lock lock(_mutex);
	auto it = _modules.find(module->_name);
	if (it != _modules.end()) {
		auto vIt = it->second.find(module->_version);
		if (vIt != it->second.end()) {
			if (hasFlag(vIt->second->_flags, SharedModuleFlags::Extensible)) {
				module->_next = vIt->second;
				vIt->second = module;
			} else {
				log::source().error("SharedModule", "Module '", module->_name, "' redefined");
				abort();
			}
		} else {
			// add new version
			it->second.emplace(module->_version, module);
		}
	} else {
		auto it = _modules.emplace(module->_name, ModuleVersionMap()).first;
		it->second.emplace(module->_version, module);
	}
}

void SharedModuleManager::removeModule(SharedModule *module) {
	std::unique_lock lock(_mutex);
	auto it = _modules.find(module->_name);
	if (it == _modules.end()) {
		return;
	}

	auto vIt = it->second.find(module->_version);
	if (vIt == it->second.end()) {
		return;
	}

	bool erased = false;
	if (!hasFlag(vIt->second->_flags, SharedModuleFlags::Extensible)) {
		it->second.erase(vIt);
		erased = true;
	} else {
		auto target = &vIt->second;
		auto mod = vIt->second;
		while (mod && mod != module) {
			target = &mod->_next;
			mod = mod->_next;
		}

		if (mod == module) {
			*target = mod->_next;
		}

		if (!vIt->second) {
			it->second.erase(vIt);
			erased = true;
		}
	}

	if (erased) {
		if (it->second.empty()) {
			_modules.erase(it);
		}
	}
}

const SharedModule *SharedModuleManager::openModule(StringView module, uint32_t version) const {
	auto it = _modules.find(module);
	if (it != _modules.end()) {
		if (!it->second.empty()) {
			if (version == SharedModule::VersionLatest) {
				auto v = std::prev(it->second.end());
				return v->second;
			} else {
				auto vIt = it->second.find(version);
				if (vIt != it->second.end()) {
					return vIt->second;
				}
			}
		}
	}
	return nullptr;
}

const void *SharedModuleManager::acquireSymbol(const char *module, uint32_t version,
		const char *symbol, const SourceLocation &loc) const {
	std::unique_lock lock(_mutex);
	auto mod = openModule(module, version);
	if (!mod) {
		log::source(loc).error("SharedModule", "Module \"", module, "\" is not defined");
		return nullptr;
	}

	return mod->acquireSymbol(symbol, loc);
}

const void *SharedModuleManager::acquireSymbol(const char *module, uint32_t version,
		const char *symbol, const std::type_info *t, const SourceLocation &loc) const {
	std::unique_lock lock(_mutex);
	auto mod = openModule(module, version);
	if (!mod) {
		log::source(loc).error("SharedModule", "Module \"", module, "\" is not defined");
		return nullptr;
	}

	return mod->acquireSymbol(symbol, *t, loc);
}

void SharedModuleManager::enumerateModules(void *userdata,
		void (*cb)(void *userdata, const char *name)) {
	if (!cb) {
		return;
	}
	for (auto &it : _modules) { cb(userdata, it.first.data()); }
}

bool SharedModuleManager::enumerateSymbols(const char *module, uint32_t version, void *userdata,
		void (*cb)(void *userdata, const char *name, const void *symbol)) {
	if (!cb) {
		return false;
	}

	auto mod = openModule(module, version);
	if (!mod) {
		return false;
	}

	SharedSymbol *s = nullptr;
	size_t c = 0;

	while (mod) {
		s = mod->_symbols;
		c = mod->_symbolsCount;
		while (c) {
			cb(userdata, s->name, s->ptr);
			++s;
			--c;
		}
		mod = mod->_next;
	}

	return true;
}

const SharedModule *SharedModule::openModule(const char *module, uint32_t version) {
	auto manager = SharedModuleManager::getInstance();
	return manager->openModule(module, version);
}

const SharedModule *SharedModule::openModule(const char *module) {
	auto manager = SharedModuleManager::getInstance();
	return manager->openModule(module, VersionLatest);
}

const void *SharedModule::acquireSymbol(const char *module, uint32_t version, const char *symbol,
		const SourceLocation &loc) {
	auto manager = SharedModuleManager::getInstance();
	return manager->acquireSymbol(module, version, symbol, loc);
}

const void *SharedModule::acquireSymbol(const char *module, uint32_t version, const char *symbol,
		const std::type_info &info, const SourceLocation &loc) {
	auto manager = SharedModuleManager::getInstance();
	return manager->acquireSymbol(module, version, symbol, &info, loc);
}

const void *SharedModule::acquireSymbol(const char *module, const char *symbol,
		const SourceLocation &loc) {
	auto manager = SharedModuleManager::getInstance();
	return manager->acquireSymbol(module, VersionLatest, symbol, loc);
}

const void *SharedModule::acquireSymbol(const char *module, const char *symbol,
		const std::type_info &info, const SourceLocation &loc) {
	auto manager = SharedModuleManager::getInstance();
	return manager->acquireSymbol(module, VersionLatest, symbol, &info, loc);
}

void SharedModule::enumerateModules(void *userdata, void (*cb)(void *userdata, const char *name)) {
	auto manager = SharedModuleManager::getInstance();
	manager->enumerateModules(userdata, cb);
}

bool SharedModule::enumerateSymbols(const char *module, void *userdata,
		void (*cb)(void *userdata, const char *name, const void *symbol)) {
	auto manager = SharedModuleManager::getInstance();
	return manager->enumerateSymbols(module, VersionLatest, userdata, cb);
}

bool SharedModule::enumerateSymbols(const char *module, uint32_t version, void *userdata,
		void (*cb)(void *userdata, const char *name, const void *symbol)) {
	auto manager = SharedModuleManager::getInstance();
	return manager->enumerateSymbols(module, version, userdata, cb);
}

SharedModule::SharedModule(const char *n, SharedSymbol *s, size_t count, SharedModuleFlags f)
: _name(n), _symbols(s), _symbolsCount(count), _flags(f), _version(Dso::GetCurrentVersion()) {
	_typeId = TypeId;
	SharedModuleManager::getInstance()->addModule(this);
}

SharedModule::~SharedModule() { SharedModuleManager::getInstance()->removeModule(this); }

const void *SharedModule::acquireSymbol(const char *symbol, const SourceLocation &loc) const {
	StringView symbolView(symbol);

	SharedSymbol *s = nullptr;
	size_t c = 0;

	auto mod = this;
	while (mod) {
		s = mod->_symbols;
		c = mod->_symbolsCount;
		while (c) {
			if (s->name == symbolView) {
				return s->ptr;
			}
			++s;
			--c;
		}
		mod = mod->_next;
	}
	return nullptr;
}

const void *SharedModule::acquireSymbol(const char *symbol, const std::type_info &t,
		const SourceLocation &loc) const {
	StringView symbolView(symbol);
	bool found = false;

	SharedSymbol *s = nullptr;
	size_t c = 0;

	auto mod = this;
	while (mod) {
		s = mod->_symbols;
		c = mod->_symbolsCount;
		while (c) {
			if (s->name == symbolView) {
				if (*s->type == t) {
					return s->ptr;
				}
				found = true;
			}
			++s;
			--c;
		}
		mod = mod->_next;
	}

	if (found) {
		memory::StandartInterface::StringStreamType err;
		err << "Module \"" << _name << "\": Symbol \"" << symbol << "\" not found for: '";
		printDemangled(err, &t);
		err << "'\n";

		mod = this;
		while (mod) {
			s = mod->_symbols;
			c = mod->_symbolsCount;
			while (c) {
				if (s->name == symbolView) {
					err << "\tFound: '";
					printDemangled(err, s->type);
					err << "'\n";
				}
				++s;
				--c;
			}
			mod = mod->_next;
		}
		log::source(loc).error("SharedModule", err.str());
	}
	return nullptr;
}

} // namespace STAPPLER_VERSIONIZED stappler
