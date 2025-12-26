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

#ifndef CORE_WASM_SPWASM_H_
#define CORE_WASM_SPWASM_H_

#include "SPFilepath.h"
#include "SPMemory.h"
#include "SPRef.h"

#ifdef SP_STATIC_DEPS
#define WASM_RUNTIME_API_EXTERN
#endif

#include "wamr/wasm_export.h"

namespace stappler::wasm {

using namespace mem_std;

class ModuleInstance;
class ExecFunction;

struct ListOutput {
	uint32_t ptr;
	uint32_t len;

	template <typename T>
	void setData(ModuleInstance *, const T *, size_t count);
};

class Runtime final {
public:
	static Runtime *getInstance();

	~Runtime();

protected:
	struct Data;

	Runtime();

	Data *_data;
};

struct NativeModule {
	NativeModule(StringView, NativeSymbol *, uint32_t count);
	~NativeModule();

	String name;
	NativeSymbol *symbols;
	uint32_t symbolsCount;
};

class Module final : public Ref {
public:
	virtual ~Module();

	bool init(StringView name, BytesView);
	bool init(StringView name, Bytes &&);
	bool init(StringView name, const FileInfo &);

	StringView getName() const { return _name; }
	wasm_module_t getModule() const { return _module; }

protected:
	String _name;
	Bytes _data;
	Runtime *_runtime = nullptr;
	wasm_module_t _module = nullptr;
};

class ModuleInstance final : public Ref {
public:
	static constexpr uint32_t InvalidHandle = maxOf<uint32_t>();

	virtual ~ModuleInstance();

	bool init(Module *, uint32_t stackSize = 16_KiB, uint32_t heapSize = 16_KiB);

	Module * getModule() const { return _module; }
	wasm_module_inst_t getInstance() const { return _inst; }

	void *appToNative(uint32_t offset) const;
	uint32_t nativeToApp(void *ptr) const;

	uint32_t allocate(uint32_t size, void ** = nullptr);
	uint32_t reallocate(uint32_t ptr, uint32_t size, void ** = nullptr);

	void free(uint32_t ptr);

	Rc<ExecFunction> lookup(StringView);

	template <typename T>
	uint32_t addHandle(T *, Function<void()> &&dtor = nullptr);

	template <typename T>
	uint32_t getHandle(T *) const;

	template <typename T = void>
	T *getObject(uint32_t) const;

	uint32_t getHandle(void *) const;

	void removeHandle(uint32_t);
	void removeObject(void *);

protected:
	uint32_t addHandleObject(void *, std::type_index &&, Function<void()> &&);
	uint32_t getHandleObject(void *, const std::type_index &) const;
	void *getObjectHandle(uint32_t, const std::type_index &) const;

	struct HandleSlot {
		void *object = nullptr;
		std::type_index type = std::type_index(typeid(void));
		uint32_t index;
		uint32_t nextIndex = InvalidHandle;
		Function<void()> destructor;
	};

	Rc<Module> _module;
	wasm_module_inst_t _inst = nullptr;
	wasm_function_inst_t _finalize = nullptr;
	wasm_function_inst_t _realloc = nullptr;

	uint32_t _selfHandle = 0;
	uint32_t _freeHandleSlot = InvalidHandle;
	Vector<HandleSlot> _handles;
	HashMap<void *, uint32_t> _objects;
};

class ExecEnv final : public Ref {
public:
	static ExecEnv *get(wasm_exec_env_t);

	virtual ~ExecEnv();

	bool init(ModuleInstance *, uint32_t stackSize = 16_KiB);
	bool init(ModuleInstance *, wasm_exec_env_t);

	wasm_exec_env_t getEnv() const { return _env; }
	ModuleInstance *getInstance() const { return _instance; }

	template <typename T = void>
	auto appToNative(uint32_t offset) const -> T * {
		return reinterpret_cast<T *>(_instance->appToNative(offset));
	}
	uint32_t nativeToApp(void *ptr) const;

	template <typename T = void>
	uint32_t allocate(uint32_t size, T **ptr = nullptr) {
		return _instance->allocate(size, reinterpret_cast<void **>(ptr));
	}

	void free(uint32_t ptr) {
		_instance->free(ptr);
	}

	bool callIndirect(uint32_t, uint32_t argc, uint32_t argv[]);

protected:
	Rc<ModuleInstance> _instance;
	wasm_exec_env_t _env = nullptr;
	bool _isSingleton = false;
};

class ExecFunction final : public Ref {
public:
	static constexpr uint32_t StaticArgumentsLimit = 28;
	static constexpr uint32_t StaticResultsLimit = 4;

	virtual ~ExecFunction() = default;

	bool init(ModuleInstance *, StringView name);

	StringView getName() const { return _name; }
	wasm_function_inst_t getFunc() const { return _func; }

	uint32_t getNumArgs() const { return _nArgs; }
	uint32_t getNumResults() const { return _nResults; }

	SpanView<wasm_valkind_t> getArgs() const { return makeSpanView(_argTypesStatic, std::max(_nArgs, StaticArgumentsLimit)); }
	SpanView<wasm_valkind_t> getResults() const { return makeSpanView(_resultTypesStatic, std::max(_nResults, StaticResultsLimit)); }

	// use when statics are out of limit
	Vector<wasm_valkind_t> getArgsFull() const;
	Vector<wasm_valkind_t> getResultsFull() const;

	bool call(ExecEnv *, SpanView<wasm_val_t> args = SpanView<wasm_val_t>(), VectorAdapter<wasm_val_t> *results = nullptr) const;

	wasm_val_t call1(ExecEnv *, SpanView<wasm_val_t> args = SpanView<wasm_val_t>()) const;

protected:
	String _name;
	wasm_function_inst_t _func = nullptr;
	Rc<ModuleInstance> _inst;
	uint32_t _nArgs = 0;
	uint32_t _nResults = 0;
	wasm_valkind_t _resultTypesStatic[StaticResultsLimit] = { 0 };
	wasm_valkind_t _argTypesStatic[StaticArgumentsLimit] = { 0 };
};

inline wasm_val_t MakeValue(uint32_t val) {
	wasm_val_t ret;
	ret.kind = WASM_I32;
	ret.of.i32 = val;
	return ret;
}

inline wasm_val_t MakeValue(int32_t val) {
	wasm_val_t ret;
	ret.kind = WASM_I32;
	ret.of.i32 = val;
	return ret;
}

inline wasm_val_t MakeValue(uint64_t val) {
	wasm_val_t ret;
	ret.kind = WASM_I64;
	ret.of.i64 = val;
	return ret;
}

inline wasm_val_t MakeValue(int64_t val) {
	wasm_val_t ret;
	ret.kind = WASM_I64;
	ret.of.i64 = val;
	return ret;
}

inline wasm_val_t MakeValue(float val) {
	wasm_val_t ret;
	ret.kind = WASM_F32;
	ret.of.f32 = val;
	return ret;
}

inline wasm_val_t MakeValue(double val) {
	wasm_val_t ret;
	ret.kind = WASM_F64;
	ret.of.f64 = val;
	return ret;
}

template <typename T>
void ListOutput::setData(ModuleInstance *inst, const T *data, size_t count) {
	uint8_t *buf = nullptr;
	this->ptr = inst->allocate(uint32_t(count * sizeof(T)), (void **)&buf);
	this->len = uint32_t(count);
	memcpy(buf, data, count * sizeof(T));
}

template <typename T>
uint32_t ModuleInstance::addHandle(T *obj, Function<void()> &&dtor) {
	using Type = typename std::remove_reference<typename std::remove_cv<T>::type>::type;
	return addHandleObject((Type *)obj, std::type_index(typeid(Type)), sp::move(dtor));
}

template <typename T>
uint32_t ModuleInstance::getHandle(T *obj) const {
	return getHandleObject(obj, std::type_index(typeid(typename std::remove_reference<typename std::remove_cv<T>::type>::type)));
}

template <typename T>
T *ModuleInstance::getObject(uint32_t idx) const {
	return reinterpret_cast<T *>(getObjectHandle(idx, std::type_index(typeid(typename std::remove_reference<typename std::remove_cv<T>::type>::type))));
}

}

#endif /* CORE_WASM_SPWASM_H_ */
