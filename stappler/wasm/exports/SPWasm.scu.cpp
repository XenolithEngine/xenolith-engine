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

#include "SPWasm.h"

#include "SPWasm-core.cc"
#include "SPWasm-filesystem.cc"
#include "SPWasm-data.cc"

namespace stappler::wasm {

static std::mutex s_loadMutex;
static Runtime *s_instance = nullptr;

struct Runtime::Data {
	bool enabled = false;
};

static void *runtime_malloc(size_t size) { return ::malloc(size); }

static void runtime_free(void *ptr) { ::free(ptr); }

static void *runtime_realloc(void *ptr, size_t size) { return ::realloc(ptr, size); }

static void StapplerWasmDebugPrint(wasm_exec_env_t exec_env, uint32_t ptr, uint32_t size) {
	auto mod = wasm_runtime_get_module_inst(exec_env);
	auto sptr = (const char *)wasm_runtime_addr_app_to_native(mod, ptr);

	log::source().debug("wasm::Runtime", StringView(sptr, size));
}

static NativeSymbol stapper_wasm_symbols[] = {
	NativeSymbol{"debug-print", (void *)&StapplerWasmDebugPrint, "(ii)", NULL},
};

static NativeModule s_wasmModule("stappler:wasm/wasm", stapper_wasm_symbols,
		sizeof(stapper_wasm_symbols) / sizeof(NativeSymbol));

Runtime *Runtime::getInstance() {
	std::unique_lock lock(s_loadMutex);
	if (!s_instance) {
		s_instance = new Runtime;
	}
	return s_instance;
}

Runtime::~Runtime() {
	if (_data->enabled) {
		wasm_runtime_destroy();
		_data->enabled = false;
	}
	if (_data) {
		delete _data;
		_data = nullptr;
	}
}

struct RuntimeNativeStorage {
	static RuntimeNativeStorage *getInstance() {
		static RuntimeNativeStorage s_instance;
		return &s_instance;
	}

	Set<NativeModule *> s_nativeModules;
};


Runtime::Runtime() {
	_data = new Data;

	RuntimeInitArgs init_args;
	memset(&init_args, 0, sizeof(RuntimeInitArgs));

	init_args.mem_alloc_type = Alloc_With_Allocator;
	init_args.mem_alloc_option.allocator.malloc_func = (void *)&runtime_malloc;
	init_args.mem_alloc_option.allocator.realloc_func = (void *)&runtime_realloc;
	init_args.mem_alloc_option.allocator.free_func = (void *)&runtime_free;
	// init_args.mem_alloc_option.allocator.user_data = this;

	init_args.n_native_symbols = 0;
	init_args.native_module_name = "env";
	init_args.native_symbols = nullptr;

#ifdef WASM_DEBUG
	strcpy(init_args.ip_addr, "127.0.0.1");
	init_args.instance_port = 0;
#endif

	/* initialize runtime environment */
	if (wasm_runtime_full_init(&init_args)) {
		_data->enabled = true;

		for (auto &it : RuntimeNativeStorage::getInstance()->s_nativeModules) {
			wasm_runtime_register_natives(it->name.data(), it->symbols, it->symbolsCount);
		}
	}
}

NativeModule::NativeModule(StringView n, NativeSymbol *s, uint32_t count)
: name(n.str<Interface>()), symbols(s), symbolsCount(count) {
	RuntimeNativeStorage::getInstance()->s_nativeModules.emplace(this);
}

NativeModule::~NativeModule() { RuntimeNativeStorage::getInstance()->s_nativeModules.erase(this); }

Module::~Module() {
	if (_module) {
		wasm_runtime_unload(_module);
		_module = nullptr;
	}
}

bool Module::init(StringView name, BytesView data) {
	char errorBuf[128] = {0};

	_runtime = Runtime::getInstance();

	_data = data.bytes<Interface>();

	auto mod = wasm_runtime_load(const_cast<uint8_t *>(_data.data()),
			static_cast<uint32_t>(_data.size()), errorBuf, sizeof(errorBuf));
	if (!mod) {
		log::source().error("wasm::Module", "Fail to load module: ", errorBuf);
		return false;
	}

	_name = name.str<Interface>();
	_module = mod;

	if (!wasm_runtime_register_module(_name.data(), _module, errorBuf, sizeof(errorBuf))) {
		log::source().error("wasm::Module", "Fail to register module '", name, "': ", errorBuf);
		return false;
	}

	return true;
}

bool Module::init(StringView name, Bytes &&data) {
	char errorBuf[128] = {0};

	_runtime = Runtime::getInstance();

	_data = sp::move(data);

	auto mod = wasm_runtime_load(const_cast<uint8_t *>(_data.data()),
			static_cast<uint32_t>(_data.size()), errorBuf, sizeof(errorBuf));
	if (!mod) {
		log::source().error("wasm::Module", "Fail to load module: ", errorBuf);
		return false;
	}

	_name = name.str<Interface>();
	_module = mod;

	if (!wasm_runtime_register_module(_name.data(), _module, errorBuf, sizeof(errorBuf))) {
		log::source().error("wasm::Module", "Fail to register module '", name, "': ", errorBuf);
		return false;
	}

	return true;
}

bool Module::init(StringView name, const FileInfo &path) {
	char errorBuf[128] = {0};
	_data = filesystem::readIntoMemory<Interface>(path);

	if (_data.empty()) {
		log::source().error("wasm::Module", "Fail to open file: ", path);
		return false;
	}

	_runtime = Runtime::getInstance();

	auto mod = wasm_runtime_load(const_cast<uint8_t *>(_data.data()),
			static_cast<uint32_t>(_data.size()), errorBuf, sizeof(errorBuf));
	if (!mod) {
		log::source().error("wasm::Module", "Fail to load module '", path, "': ", errorBuf);
		return false;
	}

	_name = name.str<Interface>();
	_module = mod;

	if (!wasm_runtime_register_module(_name.data(), _module, errorBuf, sizeof(errorBuf))) {
		log::source().error("wasm::Module", "Fail to register module '", path, "': ", errorBuf);
		return false;
	}

	return true;
}

ModuleInstance::~ModuleInstance() {
	if (_inst) {
		for (auto &it : _handles) {
			if (it.destructor) {
				it.destructor();
			}
			it.destructor = nullptr;
		}

		_handles.clear();
		_objects.clear();

		auto senv = wasm_runtime_get_exec_env_singleton(_inst);
		if (senv && _finalize) {
			wasm_runtime_call_wasm(senv, _finalize, 0, nullptr);
			_finalize = nullptr;
		}
		wasm_runtime_deinstantiate(_inst);
		_inst = nullptr;
	}
}

bool ModuleInstance::init(Module *mod, uint32_t stackSize, uint32_t heapSize) {
	char errorBuf[128] = {0};
	auto inst = wasm_runtime_instantiate(mod->getModule(), stackSize, heapSize, errorBuf,
			sizeof(errorBuf));
	/* instantiate the module */
	if (!inst) {
		log::source().error("wasm::Module", "Fail to instantiate module '", mod->getName(),
				"': ", errorBuf);
		return false;
	}

	_inst = inst;
	_module = mod;

	auto senv = wasm_runtime_get_exec_env_singleton(_inst);
	auto env = Rc<ExecEnv>::create(this, senv);

	auto realloc = Rc<ExecFunction>::create(this, "realloc");
	if (realloc && realloc->getNumArgs() == 2 && realloc->getNumResults() == 1) {
		_realloc = realloc->getFunc();
	}

	_selfHandle = addHandle(this, [] { });

	// search for _start
	auto initialize = Rc<ExecFunction>::create(this, "_initialize");
	// search for _initialize
	if (initialize) {
		if (initialize->getName() != "_initialize") {
			// component-named _initialize, try stappler_initialize first
			auto stapplerInitialize = Rc<ExecFunction>::create(this, "initialize");
			if (stapplerInitialize) {
				stapplerInitialize->call(env);
			} else {
				initialize->call(env);
			}
		}
	} else {
		auto start = Rc<ExecFunction>::create(this, "_start");
		if (start) {
			// assume WASI command module
			return true;
		}
	}

	auto fin = Rc<ExecFunction>::create(this, "_finalize");
	if (fin && fin->getNumArgs() == 0 && fin->getNumResults() == 0) {
		_finalize = fin->getFunc();
	}

	_handles.reserve(16);

	return true;
}

void *ModuleInstance::appToNative(uint32_t offset) const {
	return wasm_runtime_addr_app_to_native(_inst, offset);
}

uint32_t ModuleInstance::nativeToApp(void *ptr) const {
	return uint32_t(wasm_runtime_addr_native_to_app(_inst, ptr));
}

uint32_t ModuleInstance::allocate(uint32_t size, void **ptr) {
	return uint32_t(wasm_runtime_module_malloc(_inst, size, ptr));
}

uint32_t ModuleInstance::reallocate(uint32_t offset, uint32_t size, void **ptr) {
	if (_realloc) {
		uint32_t args[] = {offset, size};

		auto senv = wasm_runtime_get_exec_env_singleton(_inst);

		if (wasm_runtime_call_wasm(senv, _realloc, 2, args)) {
			if (ptr) {
				*ptr = appToNative(args[0]);
			}
			return args[0];
		}
	}

	wasm_runtime_module_free(_inst, offset);
	return uint32_t(wasm_runtime_module_malloc(_inst, size, ptr));
}

void ModuleInstance::free(uint32_t ptr) { wasm_runtime_module_free(_inst, ptr); }

Rc<ExecFunction> ModuleInstance::lookup(StringView name) {
	return Rc<ExecFunction>::create(this, name);
}

uint32_t ModuleInstance::getHandle(void *obj) const {
	auto it = _objects.find(obj);
	if (it != _objects.end()) {
		return it->second;
	}
	return InvalidHandle;
}

void ModuleInstance::removeHandle(uint32_t idx) {
	if (idx >= _handles.size()) {
		return;
	}

	auto &slot = _handles.at(idx);
	if (slot.object) {
		_objects.erase(slot.object);
		if (slot.destructor) {
			slot.destructor();
			slot.destructor = nullptr;
		}
		slot.nextIndex = _freeHandleSlot;
		slot.object = nullptr;

		_freeHandleSlot = slot.index;
	}
}

void ModuleInstance::removeObject(void *obj) {
	auto it = _objects.find(obj);
	if (it != _objects.end()) {
		removeHandle(it->second);
	}
}

uint32_t ModuleInstance::addHandleObject(void *obj, std::type_index &&idx, Function<void()> &&cb) {
	if (!obj) {
		return InvalidHandle;
	}

	HandleSlot *slot = nullptr;
	if (_freeHandleSlot != InvalidHandle) {
		slot = &_handles.at(_freeHandleSlot);
		_freeHandleSlot = slot->nextIndex;
		slot->nextIndex = InvalidHandle;
	} else {
		slot = &_handles.emplace_back(HandleSlot());
		slot->index = uint32_t(_handles.size() - 1);
	}

	slot->object = obj;
	slot->type = sp::move(idx);
	slot->destructor = sp::move(cb);

	_objects.emplace(obj, slot->index);
	return slot->index;
}

uint32_t ModuleInstance::getHandleObject(void *obj, const std::type_index &type) const {
	auto it = _objects.find(obj);
	if (it != _objects.end()) {
		uint32_t idx = it->second;
		if (_handles[idx].type == type) {
			return idx;
		}
	}
	return InvalidHandle;
}

void *ModuleInstance::getObjectHandle(uint32_t idx, const std::type_index &type) const {
	if (idx >= _handles.size()) {
		return nullptr;
	}

	auto &slot = _handles.at(idx);
	if (slot.type == type) {
		return slot.object;
	}

	return nullptr;
}

ExecEnv *ExecEnv::get(wasm_exec_env_t env) {
	return reinterpret_cast<ExecEnv *>(wasm_runtime_get_user_data(env));
}

ExecEnv::~ExecEnv() {
	if (_env) {
		if (_isSingleton) {
			wasm_runtime_set_user_data(_env, nullptr);
		} else {
			wasm_runtime_destroy_exec_env(_env);
		}
		_env = nullptr;
	}
}

bool ExecEnv::init(ModuleInstance *inst, uint32_t stackSize) {
	auto env = wasm_runtime_create_exec_env(inst->getInstance(), stackSize);
	/* create an execution env */
	if (!env) {
		log::source().error("wasm::Module", "Fail to create exec env for '",
				inst->getModule()->getName(), "' instance");
		return false;
	}

	_env = env;
	_instance = inst;

	wasm_runtime_set_user_data(_env, this);

#ifdef WASM_DEBUG
	auto debugEnv = ::getenv("WASM_DEBUG");
	if (debugEnv) {
		auto port = wasm_runtime_start_debug_instance(env);
		log::source().info("wasm::Runtime", "start debug server with port ", port,
				"; Wait for debugger connection...");
	}
#endif

	return true;
}

bool ExecEnv::init(ModuleInstance *inst, wasm_exec_env_t env) {
	if (wasm_runtime_get_user_data(env)) {
		log::source().warn("wasm::Module", "Userdata is not empty for '",
				inst->getModule()->getName(), "' instance env, it will be lost");
	}

	_env = env;
	_instance = inst;
	_isSingleton = true;

	wasm_runtime_set_user_data(_env, this);

	return true;
}

uint32_t ExecEnv::nativeToApp(void *ptr) const { return _instance->nativeToApp(ptr); }

bool ExecEnv::callIndirect(uint32_t fn, uint32_t argc, uint32_t argv[]) {
	return wasm_runtime_call_indirect(_env, fn, argc, argv);
}

bool ExecFunction::init(ModuleInstance *inst, StringView name) {
	// search not prefixed version first
	_name = name.str<Interface>();

	auto fn = wasm_runtime_lookup_function(inst->getInstance(), _name.data());
	if (!fn) {
		_name = toString(inst->getModule()->getName(), "#", name);
		// search module-prefixed
		fn = wasm_runtime_lookup_function(inst->getInstance(), _name.data());
	}

	if (!fn) {
		log::source().warn("wasm::ExecFunction", "Fail to lookup function '", name, "' in module '",
				inst->getModule()->getName(), "'");
		return false;
	}

	_func = fn;
	_inst = inst;
	_nArgs = wasm_func_get_param_count(_func, _inst->getInstance());
	_nResults = wasm_func_get_result_count(_func, _inst->getInstance());

	if (_nArgs > StaticArgumentsLimit) {
		log::source().warn("wasm::ExecFunction", "Too many arguments for '", _name, "' in module '",
				inst->getModule()->getName(), "'");
	} else {
		wasm_func_get_param_types(_func, _inst->getInstance(), _argTypesStatic);
	}

	if (_nResults > StaticResultsLimit) {
		log::source().warn("wasm::ExecFunction", "Too many results for '", _name, "' in module '",
				inst->getModule()->getName(), "'");
	} else {
		wasm_func_get_result_types(_func, _inst->getInstance(), _resultTypesStatic);
	}

	return true;
}

Vector<wasm_valkind_t> ExecFunction::getArgsFull() const {
	Vector<wasm_valkind_t> ret;
	ret.resize(_nArgs);
	wasm_func_get_param_types(_func, _inst->getInstance(), ret.data());
	return ret;
}

Vector<wasm_valkind_t> ExecFunction::getResultsFull() const {
	Vector<wasm_valkind_t> ret;
	ret.resize(_nResults);
	wasm_func_get_result_types(_func, _inst->getInstance(), ret.data());
	return ret;
}

bool ExecFunction::call(ExecEnv *env, SpanView<wasm_val_t> args,
		VectorAdapter<wasm_val_t> *results) const {
	if (args.size() != _nArgs) {
		log::source().warn("wasm::ExecFunction", "Wrong number of arguments for '", _name,
				"' from module '", _inst->getModule()->getName(), "'");
	}

	bool ret = false;
	if (results) {
		results->resize(_nResults);
		ret = wasm_runtime_call_wasm_a(env->getEnv(), _func, _nResults, results->begin(),
				uint32_t(args.size()), const_cast<wasm_val_t *>(args.data()));
	} else {
		if (_nResults) {
			log::source().warn("wasm::ExecFunction",
					"Results buffer was not provided for call of '", _name, "' from module '",
					_inst->getModule()->getName(), "'");
		}
		ret = wasm_runtime_call_wasm_a(env->getEnv(), _func, 0, nullptr, uint32_t(args.size()),
				const_cast<wasm_val_t *>(args.data()));
	}

	if (!ret) {
		auto ex = wasm_runtime_get_exception(_inst->getInstance());
		if (ex) {
			log::source().error("wasm::ExecFunction", "Exception when call '", _name,
					"' from module '", _inst->getModule()->getName(), "': ", ex);
		}
	}

	return false;
}

wasm_val_t ExecFunction::call1(ExecEnv *env, SpanView<wasm_val_t> args) const {
	if (args.size() != _nArgs) {
		log::source().warn("wasm::ExecFunction", "Wrong number of arguments for '", _name,
				"' from module '", _inst->getModule()->getName(), "'");
	}

	wasm_val_t ret;
	ret.kind = WASM_EXTERNREF;
	ret.of.foreign = 0;

	if (_nResults != 1) {
		log::source().warn("wasm::ExecFunction", "Function '", _name, "' from module '",
				_inst->getModule()->getName(), "' called as single-argument function");
	}
	auto success = wasm_runtime_call_wasm_a(env->getEnv(), _func, 1, &ret, uint32_t(args.size()),
			const_cast<wasm_val_t *>(args.data()));
	if (!success) {
		auto ex = wasm_runtime_get_exception(_inst->getInstance());
		if (ex) {
			log::source().error("wasm::ExecFunction", "Exception when call '", _name,
					"' from module '", _inst->getModule()->getName(), "': ", ex);
		}
	}
	return ret;
}

} // namespace stappler::wasm
