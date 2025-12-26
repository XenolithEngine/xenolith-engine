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
#include "SPData.h"
#include "SPDataValue.h"

namespace stappler::wasm {

enum class ForeachResult {
	Continue,
	Stop,
	Drop,
};

struct ValueSource : Ref {
	Value value;
	const Value *readOnlySource = nullptr;
};

struct ValueContainer {
	Value *value;
	Rc<ValueSource> source;
};

static uint32_t StapplerDataRead(wasm_exec_env_t exec_env, uint8_t *buf, uint32_t bufLen, char *key,
		uint32_t keyLen) {
	auto mod = ExecEnv::get(exec_env)->getInstance();

	auto val = data::read<Interface>(BytesView(buf, bufLen), StringView(key, keyLen));
	if (val) {
		auto c = new ValueContainer;
		c->source = Rc<ValueSource>::alloc();
		c->source->value = move(val);
		c->value = &c->source->value;

		return mod->addHandle(c, [c] { delete c; });
	}

	return ModuleInstance::InvalidHandle;
}

static uint32_t StapplerDataReadFile(wasm_exec_env_t exec_env, char *buf, uint32_t bufLen,
		char *key, uint32_t keyLen) {
	auto mod = ExecEnv::get(exec_env)->getInstance();

	auto val = data::readFile<Interface>(StringView(buf, bufLen), StringView(key, keyLen));
	if (val) {
		auto c = new ValueContainer;
		c->source = Rc<ValueSource>::alloc();
		c->source->value = move(val);
		c->value = &c->source->value;

		return mod->addHandle(c, [c] { delete c; });
	}

	return ModuleInstance::InvalidHandle;
}

static uint32_t stappler_wasm_data_constructor_value(wasm_exec_env_t exec_env) {
	auto mod = ExecEnv::get(exec_env)->getInstance();

	auto c = new ValueContainer;
	c->source = Rc<ValueSource>::alloc();
	c->value = &c->source->value;

	return mod->addHandle(c, [c] { delete c; });
}

static uint32_t StapplerDataCopy(wasm_exec_env_t exec_env, uint32_t handle) {
	auto mod = ExecEnv::get(exec_env)->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", "[method]value.copy: invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto c = new ValueContainer;
	c->source = Rc<ValueSource>::alloc();
	c->source->value = *val->value;
	c->value = &c->source->value;

	return mod->addHandle(c, [c] { delete c; });
}

static void StapplerDataDrop(wasm_exec_env_t exec_env, uint32_t handle) {
	auto mod = ExecEnv::get(exec_env)->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", "[resource-drop]value: invalid handle");
		return;
	}

	mod->removeHandle(handle);
}

static uint32_t StapplerDataWriteToFile(wasm_exec_env_t exec_env, uint32_t handle, char *filename,
		uint32_t len, uint32_t fmt) {
	auto mod = ExecEnv::get(exec_env)->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", "[method]value.write-to-file: invalid handle");
		return false;
	}

	return data::save(*val->value, StringView(filename, len), data::EncodeFormat(fmt));
}

static uint32_t StapplerDataWriteToMemory(wasm_exec_env_t exec_env, uint32_t handle, uint32_t fmt,
		ListOutput *out) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", "[method]value.write-to-memory: invalid handle");
		return false;
	}

	auto d = data::write(*val->value, data::EncodeFormat(fmt));
	out->setData(mod, d.data(), d.size());
	return true;
}

static uint32_t StapplerDataToString(wasm_exec_env_t exec_env, uint32_t handle, uint32_t fmt,
		ListOutput *out) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", "[method]value.to-string: invalid handle");
		return false;
	}

	auto d = data::toString(*val->value, data::EncodeFormat::Format(fmt));
	out->setData(mod, d.data(), d.size());
	return true;
}

static uint32_t stappler_wasm_data_process_foreach_array(wasm_exec_env_t exec_env,
		ModuleInstance *inst, ValueContainer *val, Value::ArrayType &arr, uint32_t callback,
		uint32_t userdata) {
	ValueContainer iterContainer;
	iterContainer.source = val->source;

	auto iterHandle = inst->addHandle(&iterContainer);

	uint32_t idx = 0;
	uint32_t args[3];
	auto it = arr.begin();
	while (it != arr.end()) {
		args[0] = userdata;
		args[1] = idx;
		args[2] = iterHandle;

		iterContainer.value = &(*it);

		if (!wasm_runtime_call_indirect(exec_env, callback, 3, args)) {
			log::source().error("wasm::Runtime", __FUNCTION__, ": fail to call_indirect");
			inst->removeObject(&iterContainer);
			return 0;
		}

		ForeachResult res = ForeachResult(args[0]);
		switch (res) {
		case ForeachResult::Continue: ++it; break;
		case ForeachResult::Stop: it = arr.end(); break;
		case ForeachResult::Drop:
			if (iterContainer.source->readOnlySource) {
				log::source().error("wasm::Runtime", __FUNCTION__,
						": fail to drop in read-only object");
				it = arr.end();
			} else {
				it = arr.erase(it);
			}
			break;
		}

		++idx;
	}

	inst->removeObject(&iterContainer);
	return 1;
}

static uint32_t stappler_wasm_data_process_foreach_dict(wasm_exec_env_t exec_env,
		ModuleInstance *inst, ValueContainer *val, Value::DictionaryType &dict, uint32_t callback,
		uint32_t userdata) {
	ValueContainer iterContainer;
	iterContainer.source = val->source;

	auto iterHandle = inst->addHandle(&iterContainer);

	uint32_t args[4];
	auto it = dict.begin();
	while (it != dict.end()) {
		args[0] = userdata;

		char *buf = nullptr;
		auto bufOffset = inst->allocate(uint32_t(it->first.size()), (void **)&buf);

		memcpy(buf, it->first.data(), it->first.size());

		args[1] = bufOffset;
		args[2] = uint32_t(it->first.size());
		args[3] = iterHandle;

		iterContainer.value = &it->second;

		if (!wasm_runtime_call_indirect(exec_env, callback, 4, args)) {
			log::source().error("wasm::Runtime", __FUNCTION__, ": fail to call_indirect");
			inst->free(bufOffset);
			inst->removeObject(&iterContainer);
			return 0;
		}

		inst->free(bufOffset);

		ForeachResult res = ForeachResult(args[0]);
		switch (res) {
		case ForeachResult::Continue: ++it; break;
		case ForeachResult::Stop: it = dict.end(); break;
		case ForeachResult::Drop:
			if (iterContainer.source->readOnlySource) {
				log::source().error("wasm::Runtime", __FUNCTION__,
						": fail to drop in read-only object");
				it = dict.end();
			} else {
				it = dict.erase(it);
			}
			break;
		}
	}

	inst->removeObject(&iterContainer);
	return 1;
}

static uint32_t stappler_wasm_data_method_value_is_null(wasm_exec_env_t exec_env, uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isNull();
}

static uint32_t stappler_wasm_data_method_value_is_basic_type(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBasicType();
}

static uint32_t stappler_wasm_data_method_value_is_array(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isArray();
}

static uint32_t stappler_wasm_data_method_value_is_dictionary(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isDictionary();
}

static uint32_t stappler_wasm_data_method_value_is_bool(wasm_exec_env_t exec_env, uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBool();
}

static uint32_t stappler_wasm_data_method_value_is_integer(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isInteger();
}

static uint32_t stappler_wasm_data_method_value_is_double(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isDouble();
}

static uint32_t stappler_wasm_data_method_value_is_string(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isString();
}

static uint32_t stappler_wasm_data_method_value_is_bytes(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBytes();
}

static uint32_t stappler_wasm_data_method_value_get_type(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	if (val->value) {
		return uint32_t(val->value->getType());
	}
	return uint32_t(Value::Type::NONE);
}

static uint32_t stappler_wasm_data_method_value_is_null_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isNull(idx);
}

static uint32_t stappler_wasm_data_method_value_is_basic_type_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBasicType(idx);
}

static uint32_t stappler_wasm_data_method_value_is_array_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isArray(idx);
}

static uint32_t stappler_wasm_data_method_value_is_dictionary_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isDictionary(idx);
}

static uint32_t stappler_wasm_data_method_value_is_bool_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBool(idx);
}

static uint32_t stappler_wasm_data_method_value_is_integer_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isInteger(idx);
}

static uint32_t stappler_wasm_data_method_value_is_double_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isDouble(idx);
}

static uint32_t stappler_wasm_data_method_value_is_string_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isString(idx);
}

static uint32_t stappler_wasm_data_method_value_is_bytes_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBytes(idx);
}

static uint32_t stappler_wasm_data_method_value_get_type_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	if (val->value) {
		return uint32_t(val->value->getType(idx));
	}
	return uint32_t(Value::Type::NONE);
}

static uint32_t stappler_wasm_data_method_value_has_value_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	if (val->value) {
		return uint32_t(val->value->hasValue(idx));
	}
	return false;
}

static uint32_t stappler_wasm_data_method_value_is_null_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isNull(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_is_basic_type_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBasicType(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_is_array_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isArray(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_is_dictionary_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isDictionary(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_is_bool_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBool(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_is_integer_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isInteger(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_is_double_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isDouble(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_is_string_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isString(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_is_bytes_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value && val->value->isBytes(StringView(key, len));
}

static uint32_t stappler_wasm_data_method_value_get_type_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	if (val->value) {
		return uint32_t(val->value->getType(StringView(key, len)));
	}
	return uint32_t(Value::Type::NONE);
}

static uint32_t stappler_wasm_data_method_value_has_value_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto mod = env->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	if (val->value) {
		return uint32_t(val->value->hasValue(StringView(key, len)));
	}
	return false;
}

static uint32_t stappler_wasm_data_method_value_is_read_only(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return true;
	}

	if (val->value) {
		return val->source->readOnlySource ? true : false;
	}
	return true;
}

static uint32_t stappler_wasm_data_method_value_size(wasm_exec_env_t exec_env, uint32_t handle) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return 0;
	}

	if (val->value) {
		return uint32_t(val->value->size());
	}
	return 0;
}

static uint32_t stappler_wasm_data_method_value_empty(wasm_exec_env_t exec_env, uint32_t handle) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return 0;
	}

	if (val->value) {
		return uint32_t(val->value->empty());
	}
	return 0;
}

static void stappler_wasm_data_method_value_clear(wasm_exec_env_t exec_env, uint32_t handle) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	if (val->value && !val->source->readOnlySource) {
		val->value->clear();
	}
}

static int64_t stappler_wasm_data_method_value_get_integer(wasm_exec_env_t exec_env,
		uint32_t handle, int64_t def) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return def;
	}

	return val->value->getInteger(def);
}

static double stappler_wasm_data_method_value_get_double(wasm_exec_env_t exec_env, uint32_t handle,
		double def) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return def;
	}

	return val->value->getDouble(def);
}

static uint32_t stappler_wasm_data_method_value_get_bool(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value->getBool();
}

static void stappler_wasm_data_method_value_get_string(wasm_exec_env_t exec_env, uint32_t handle,
		ListOutput *target) {
	auto env = ExecEnv::get(exec_env);
	auto val = env->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	auto str = StringView(val->value->getString());
	target->setData(env->getInstance(), str.data(), str.size());
}

static void stappler_wasm_data_method_value_get_bytes(wasm_exec_env_t exec_env, uint32_t handle,
		ListOutput *target) {
	auto env = ExecEnv::get(exec_env);
	auto val = env->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	auto str = BytesView(val->value->getBytes());
	target->setData(env->getInstance(), str.data(), str.size());
}

static uint32_t stappler_wasm_data_method_value_foreach_array(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t callback, uint32_t userdata) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return 0;
	}

	if (!val->value->isArray()) {
		return 0;
	}

	return stappler_wasm_data_process_foreach_array(exec_env, inst, val, val->value->getArray(),
			callback, userdata);
}

static uint32_t stappler_wasm_data_method_value_foreach_dict(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t callback, uint32_t userdata) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return 0;
	}

	if (!val->value->isDictionary()) {
		return 0;
	}

	return stappler_wasm_data_process_foreach_dict(exec_env, inst, val, val->value->getDict(),
			callback, userdata);
}

static uint32_t stappler_wasm_data_method_value_get_value_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto mod = ExecEnv::get(exec_env)->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	if (!val->value->hasValue(idx)) {
		return ModuleInstance::InvalidHandle;
	}

	auto &newVal = val->value->getValue(idx);

	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &newVal;

	return mod->addHandle(c, [c] { delete c; });
}

static int64_t stappler_wasm_data_method_value_get_integer_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx, int64_t def) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return def;
	}

	return val->value->getInteger(idx, def);
}

static double stappler_wasm_data_method_value_get_double_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx, double def) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return def;
	}

	return val->value->getDouble(idx, def);
}

static uint32_t stappler_wasm_data_method_value_get_bool_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value->getBool(idx);
}

static void stappler_wasm_data_method_value_get_string_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx, ListOutput *target) {
	auto env = ExecEnv::get(exec_env);
	auto val = env->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	auto str = StringView(val->value->getString(idx));
	target->setData(env->getInstance(), str.data(), str.size());
}

static void stappler_wasm_data_method_value_get_bytes_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx, ListOutput *target) {
	auto env = ExecEnv::get(exec_env);
	auto val = env->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	auto str = BytesView(val->value->getBytes(idx));
	target->setData(env->getInstance(), str.data(), str.size());
}

static uint32_t stappler_wasm_data_method_value_foreach_array_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx, uint32_t callback, uint32_t userdata) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return 0;
	}

	if (!val->value->isArray(idx)) {
		return 0;
	}

	return stappler_wasm_data_process_foreach_array(exec_env, inst, val, val->value->getArray(idx),
			callback, userdata);
}

static uint32_t stappler_wasm_data_method_value_foreach_dict_by_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx, uint32_t callback, uint32_t userdata) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return 0;
	}

	if (!val->value->isDictionary(idx)) {
		return 0;
	}

	return stappler_wasm_data_process_foreach_dict(exec_env, inst, val, val->value->getDict(idx),
			callback, userdata);
}

static uint32_t stappler_wasm_data_method_value_get_value_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto mod = ExecEnv::get(exec_env)->getInstance();
	auto val = mod->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	if (!val->value->hasValue(StringView(key, len))) {
		return ModuleInstance::InvalidHandle;
	}

	auto &newVal = val->value->getValue(StringView(key, len));

	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &newVal;

	return mod->addHandle(c, [c] { delete c; });
}

static int64_t stappler_wasm_data_method_value_get_integer_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len, int64_t def) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return def;
	}

	return val->value->getInteger(StringView(key, len), def);
}

static double stappler_wasm_data_method_value_get_double_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len, double def) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return def;
	}

	return val->value->getDouble(StringView(key, len), def);
}

static uint32_t stappler_wasm_data_method_value_get_bool_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len) {
	auto val = ExecEnv::get(exec_env)->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value->getBool(StringView(key, len));
}

static void stappler_wasm_data_method_value_get_string_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len, ListOutput *target) {
	auto env = ExecEnv::get(exec_env);
	auto val = env->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	auto str = StringView(val->value->getString(StringView(key, len)));
	target->setData(env->getInstance(), str.data(), str.size());
}

static void stappler_wasm_data_method_value_get_bytes_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len, ListOutput *target) {
	auto env = ExecEnv::get(exec_env);
	auto val = env->getInstance()->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	auto str = BytesView(val->value->getBytes(StringView(key, len)));
	target->setData(env->getInstance(), str.data(), str.size());
}

static uint32_t stappler_wasm_data_method_value_foreach_array_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len, uint32_t callback, uint32_t userdata) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return 0;
	}

	if (!val->value->isArray(StringView(key, len))) {
		return 0;
	}

	return stappler_wasm_data_process_foreach_array(exec_env, inst, val,
			val->value->getArray(StringView(key, len)), callback, userdata);
}

static uint32_t stappler_wasm_data_method_value_foreach_dict_by_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t len, uint32_t callback, uint32_t userdata) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return 0;
	}

	if (!val->value->isDictionary(StringView(key, len))) {
		return 0;
	}

	return stappler_wasm_data_process_foreach_dict(exec_env, inst, val,
			val->value->getDict(StringView(key, len)), callback, userdata);
}

static void stappler_wasm_data_method_value_set_null(wasm_exec_env_t exec_env, uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setNull();
}
static void stappler_wasm_data_method_value_set_bool(wasm_exec_env_t exec_env, uint32_t handle,
		uint32_t value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setBool(value);
}
static void stappler_wasm_data_method_value_set_integer(wasm_exec_env_t exec_env, uint32_t handle,
		int64_t value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setInteger(value);
}
static void stappler_wasm_data_method_value_set_double(wasm_exec_env_t exec_env, uint32_t handle,
		double value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setDouble(value);
}
static void stappler_wasm_data_method_value_set_string(wasm_exec_env_t exec_env, uint32_t handle,
		char *value, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setString(StringView(value, len));
}
static void stappler_wasm_data_method_value_set_bytes(wasm_exec_env_t exec_env, uint32_t handle,
		uint8_t *value, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setBytes(BytesView(value, len));
}
static void stappler_wasm_data_method_value_set_dict(wasm_exec_env_t exec_env, uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setDict(Value::DictionaryType());
}
static void stappler_wasm_data_method_value_set_array(wasm_exec_env_t exec_env, uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setArray(Value::ArrayType());
}
static void stappler_wasm_data_method_value_set_value_copy(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}
	auto source = inst->getObject<ValueContainer>(value);
	if (!source) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setValue(*source->value);
}
static void stappler_wasm_data_method_value_set_null_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setNull(index);
}
static void stappler_wasm_data_method_value_set_bool_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t value, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setBool(value, index);
}
static void stappler_wasm_data_method_value_set_integer_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, int64_t value, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setInteger(value, index);
}
static void stappler_wasm_data_method_value_set_double_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, double value, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setDouble(value, index);
}
static void stappler_wasm_data_method_value_set_string_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, char *value, uint32_t len, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setString(StringView(value, len), index);
}
static void stappler_wasm_data_method_value_set_bytes_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint8_t *value, uint32_t len, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setBytes(BytesView(value, len), index);
}
static uint32_t stappler_wasm_data_method_value_set_dict_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->setValue(Value(Value::DictionaryType()), index);
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_set_array_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->setValue(Value(Value::ArrayType()), index);
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_set_value_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->setValue(Value(), index);
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_set_value_copy_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t value, uint32_t index) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}
	auto source = inst->getObject<ValueContainer>(value);
	if (!source) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->setValue(*source->value, index);
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static void stappler_wasm_data_method_value_set_null_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setNull(StringView(key, keyLen));
}
static void stappler_wasm_data_method_value_set_bool_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t value, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setBool(value, StringView(key, keyLen));
}
static void stappler_wasm_data_method_value_set_integer_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, int64_t value, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setInteger(value, StringView(key, keyLen));
}
static void stappler_wasm_data_method_value_set_double_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, double value, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setDouble(value, StringView(key, keyLen));
}
static void stappler_wasm_data_method_value_set_string_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *value, uint32_t len, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setString(StringView(value, len), StringView(key, keyLen));
}
static void stappler_wasm_data_method_value_set_bytes_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, uint8_t *value, uint32_t len, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->setBytes(BytesView(value, len), StringView(key, keyLen));
}
static uint32_t stappler_wasm_data_method_value_set_dict_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->setValue(Value(Value::DictionaryType()), StringView(key, keyLen));
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_set_array_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->setValue(Value(Value::ArrayType()), StringView(key, keyLen));
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_set_value_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->setValue(Value(), StringView(key, keyLen));
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_set_value_copy_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t value, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto source = inst->getObject<ValueContainer>(value);
	if (!source) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->setValue(Value(*source->value), StringView(key, keyLen));
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static void stappler_wasm_data_method_value_add_null(wasm_exec_env_t exec_env, uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->addValue(Value());
}
static void stappler_wasm_data_method_value_add_bool(wasm_exec_env_t exec_env, uint32_t handle,
		uint32_t value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->addBool(value);
}
static void stappler_wasm_data_method_value_add_integer(wasm_exec_env_t exec_env, uint32_t handle,
		int64_t value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->addInteger(value);
}
static void stappler_wasm_data_method_value_add_double(wasm_exec_env_t exec_env, uint32_t handle,
		double value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->addDouble(value);
}
static void stappler_wasm_data_method_value_add_string(wasm_exec_env_t exec_env, uint32_t handle,
		char *value, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->addString(StringView(value, len));
}
static void stappler_wasm_data_method_value_add_bytes(wasm_exec_env_t exec_env, uint32_t handle,
		uint8_t *value, uint32_t len) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return;
	}

	val->value->addBytes(BytesView(value, len));
}
static uint32_t stappler_wasm_data_method_value_add_dict(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->addArray();
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_add_array(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->addDict();
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_add_value(wasm_exec_env_t exec_env,
		uint32_t handle) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->addValue(Value());
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_add_value_copy(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto source = inst->getObject<ValueContainer>(value);
	if (!source) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return ModuleInstance::InvalidHandle;
	}

	auto &v = val->value->addValue(*source->value);
	auto c = new ValueContainer;
	c->source = val->source;
	c->value = &v;
	return inst->addHandle(c, [c] { delete c; });
}
static uint32_t stappler_wasm_data_method_value_erase_for_idx(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t idx) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value->erase(idx);
}
static uint32_t stappler_wasm_data_method_value_erase_for_key(wasm_exec_env_t exec_env,
		uint32_t handle, char *key, uint32_t keyLen) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return val->value->erase(StringView(key, keyLen));
}
static uint32_t stappler_wasm_data_method_value_is_equal(wasm_exec_env_t exec_env, uint32_t handle,
		uint32_t value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	auto source = inst->getObject<ValueContainer>(value);
	if (!source) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return false;
	}

	return *val->value == *source->value;
}
static uint32_t stappler_wasm_data_method_value_is_not_equal(wasm_exec_env_t exec_env,
		uint32_t handle, uint32_t value) {
	auto env = ExecEnv::get(exec_env);
	auto inst = env->getInstance();
	auto val = inst->getObject<ValueContainer>(handle);
	if (!val) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return true;
	}

	auto source = inst->getObject<ValueContainer>(value);
	if (!source) {
		log::source().error("wasm::Runtime", __FUNCTION__, ": invalid handle");
		return true;
	}

	return *val->value != *source->value;
}

static NativeSymbol stapper_data_symbols[] = {
	NativeSymbol{"read", (void *)&StapplerDataRead, "(*~*~)i", NULL},
	NativeSymbol{"read-file", (void *)&StapplerDataReadFile, "(*~*~)i", NULL},

	NativeSymbol{"[constructor]value", (void *)&stappler_wasm_data_constructor_value, "()i", NULL},

	NativeSymbol{"[method]value.copy", (void *)&StapplerDataCopy, "(i)i", NULL},
	NativeSymbol{"[method]value.write-to-file", (void *)&StapplerDataWriteToFile, "(i*~)i", NULL},
	NativeSymbol{"[method]value.write-to-memory", (void *)&StapplerDataWriteToMemory, "(ii*)i",
		NULL},
	NativeSymbol{"[method]value.to-string", (void *)&StapplerDataToString, "(ii*)", NULL},

	NativeSymbol{"[method]is-read-only", (void *)&stappler_wasm_data_method_value_is_read_only,
		"(i)i", NULL},
	NativeSymbol{"[method]size", (void *)&stappler_wasm_data_method_value_size, "(i)i", NULL},
	NativeSymbol{"[method]empty", (void *)&stappler_wasm_data_method_value_empty, "(i)i", NULL},
	NativeSymbol{"[method]clear", (void *)&stappler_wasm_data_method_value_clear, "(i)", NULL},

	NativeSymbol{"[method]value.is-null", (void *)&stappler_wasm_data_method_value_is_null, "(i)i",
		NULL},
	NativeSymbol{"[method]value.is-basic-type",
		(void *)&stappler_wasm_data_method_value_is_basic_type, "(i)i", NULL},
	NativeSymbol{"[method]value.is-array", (void *)&stappler_wasm_data_method_value_is_array,
		"(i)i", NULL},
	NativeSymbol{"[method]value.is-dictionary",
		(void *)&stappler_wasm_data_method_value_is_dictionary, "(i)i", NULL},
	NativeSymbol{"[method]value.is-bool", (void *)&stappler_wasm_data_method_value_is_bool, "(i)i",
		NULL},
	NativeSymbol{"[method]value.is-integer", (void *)&stappler_wasm_data_method_value_is_integer,
		"(i)i", NULL},
	NativeSymbol{"[method]value.is-double", (void *)&stappler_wasm_data_method_value_is_double,
		"(i)i", NULL},
	NativeSymbol{"[method]value.is-string", (void *)&stappler_wasm_data_method_value_is_string,
		"(i)i", NULL},
	NativeSymbol{"[method]value.is-bytes", (void *)&stappler_wasm_data_method_value_is_bytes,
		"(i)i", NULL},
	NativeSymbol{"[method]value.get-type", (void *)&stappler_wasm_data_method_value_get_type,
		"(i)i", NULL},
	NativeSymbol{"[method]value.is-null-by-idx",
		(void *)&stappler_wasm_data_method_value_is_null_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-basic-type-by-idx",
		(void *)&stappler_wasm_data_method_value_is_basic_type_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-array-by-idx",
		(void *)&stappler_wasm_data_method_value_is_array_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-dictionary-by-idx",
		(void *)&stappler_wasm_data_method_value_is_dictionary_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-bool-by-idx",
		(void *)&stappler_wasm_data_method_value_is_bool_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-integer-by-idx",
		(void *)&stappler_wasm_data_method_value_is_integer_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-double-by-idx",
		(void *)&stappler_wasm_data_method_value_is_double_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-string-by-idx",
		(void *)&stappler_wasm_data_method_value_is_string_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-bytes-by-idx",
		(void *)&stappler_wasm_data_method_value_is_bytes_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.get-type-by-idx",
		(void *)&stappler_wasm_data_method_value_get_type_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.has-value-by-key",
		(void *)&stappler_wasm_data_method_value_has_value_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.is-null-by-key",
		(void *)&stappler_wasm_data_method_value_is_null_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-basic-type-by-key",
		(void *)&stappler_wasm_data_method_value_is_basic_type_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-array-by-key",
		(void *)&stappler_wasm_data_method_value_is_array_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-dictionary-by-key",
		(void *)&stappler_wasm_data_method_value_is_dictionary_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-bool-by-key",
		(void *)&stappler_wasm_data_method_value_is_bool_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-integer-by-key",
		(void *)&stappler_wasm_data_method_value_is_integer_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-double-by-key",
		(void *)&stappler_wasm_data_method_value_is_double_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-string-by-key",
		(void *)&stappler_wasm_data_method_value_is_string_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-bytes-by-key",
		(void *)&stappler_wasm_data_method_value_is_bytes_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.get-type-by-key",
		(void *)&stappler_wasm_data_method_value_get_type_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.has-value-by-key",
		(void *)&stappler_wasm_data_method_value_has_value_by_key, "(i*~)i", NULL},

	NativeSymbol{"[method]value.get-integer", (void *)&stappler_wasm_data_method_value_get_integer,
		"(iI)I", NULL},
	NativeSymbol{"[method]value.get-double", (void *)&stappler_wasm_data_method_value_get_double,
		"(iF)F", NULL},
	NativeSymbol{"[method]value.get-bool", (void *)&stappler_wasm_data_method_value_get_bool,
		"(i)i", NULL},
	NativeSymbol{"[method]value.get-string", (void *)&stappler_wasm_data_method_value_get_string,
		"(i*)", NULL},
	NativeSymbol{"[method]value.get-bytes", (void *)&stappler_wasm_data_method_value_get_bytes,
		"(i*)", NULL},
	NativeSymbol{"[method]value.foreach-array",
		(void *)&stappler_wasm_data_method_value_foreach_array, "(iii)i", NULL},
	NativeSymbol{"[method]value.foreach-dict",
		(void *)&stappler_wasm_data_method_value_foreach_dict, "(iii)i", NULL},
	NativeSymbol{"[method]value.get-value-by-idx",
		(void *)&stappler_wasm_data_method_value_get_value_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.get-integer-by-idx",
		(void *)&stappler_wasm_data_method_value_get_integer_by_idx, "(iiI)I", NULL},
	NativeSymbol{"[method]value.get-double-by-idx",
		(void *)&stappler_wasm_data_method_value_get_double_by_idx, "(iiF)F", NULL},
	NativeSymbol{"[method]value.get-bool-by-idx",
		(void *)&stappler_wasm_data_method_value_get_bool_by_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.get-string-by-idx",
		(void *)&stappler_wasm_data_method_value_get_string_by_idx, "(ii*)", NULL},
	NativeSymbol{"[method]value.get-bytes-by-idx",
		(void *)&stappler_wasm_data_method_value_get_bytes_by_idx, "(ii*)", NULL},
	NativeSymbol{"[method]value.foreach-array-by-idx",
		(void *)&stappler_wasm_data_method_value_foreach_array_by_idx, "(iiii)i", NULL},
	NativeSymbol{"[method]value.foreach-dict-by-idx",
		(void *)&stappler_wasm_data_method_value_foreach_dict_by_idx, "(iiii)i", NULL},
	NativeSymbol{"[method]value.get-value-by-key",
		(void *)&stappler_wasm_data_method_value_get_value_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.get-integer-by-key",
		(void *)&stappler_wasm_data_method_value_get_integer_by_key, "(i*~I)I", NULL},
	NativeSymbol{"[method]value.get-double-by-key",
		(void *)&stappler_wasm_data_method_value_get_double_by_key, "(i*~F)F", NULL},
	NativeSymbol{"[method]value.get-bool-by-key",
		(void *)&stappler_wasm_data_method_value_get_bool_by_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.get-string-by-key",
		(void *)&stappler_wasm_data_method_value_get_string_by_key, "(i*~*)", NULL},
	NativeSymbol{"[method]value.get-bytes-by-key",
		(void *)&stappler_wasm_data_method_value_get_bytes_by_key, "(i*~*)", NULL},
	NativeSymbol{"[method]value.foreach-array-by-key",
		(void *)&stappler_wasm_data_method_value_foreach_array_by_key, "(i*~ii)i", NULL},
	NativeSymbol{"[method]value.foreach-dict-by-key",
		(void *)&stappler_wasm_data_method_value_foreach_dict_by_key, "(i*~ii)i", NULL},

	NativeSymbol{"[method]value.set-null", (void *)&stappler_wasm_data_method_value_set_null, "(i)",
		NULL},
	NativeSymbol{"[method]value.set-bool", (void *)&stappler_wasm_data_method_value_set_bool,
		"(ii)", NULL},
	NativeSymbol{"[method]value.set-integer", (void *)&stappler_wasm_data_method_value_set_integer,
		"(iI)", NULL},
	NativeSymbol{"[method]value.set-double", (void *)&stappler_wasm_data_method_value_set_double,
		"(iF)", NULL},
	NativeSymbol{"[method]value.set-string", (void *&)stappler_wasm_data_method_value_set_string,
		"(i*~)", NULL},
	NativeSymbol{"[method]value.set-bytes", (void *)&stappler_wasm_data_method_value_set_bytes,
		"(i*~)", NULL},
	NativeSymbol{"[method]value.set-dict", (void *)&stappler_wasm_data_method_value_set_dict, "(i)",
		NULL},
	NativeSymbol{"[method]value.set-array", (void *)&stappler_wasm_data_method_value_set_array,
		"(i)", NULL},
	NativeSymbol{"[method]value.set-value-copy",
		(void *)&stappler_wasm_data_method_value_set_value_copy, "(ii)", NULL},
	NativeSymbol{"[method]value.set-null-for-idx",
		(void *)&stappler_wasm_data_method_value_set_null_for_idx, "(ii)", NULL},
	NativeSymbol{"[method]value.set-bool-for-idx",
		(void *)&stappler_wasm_data_method_value_set_bool_for_idx, "(iii)", NULL},
	NativeSymbol{"[method]value.set-integer-for-idx",
		(void *)&stappler_wasm_data_method_value_set_integer_for_idx, "(iIi)", NULL},
	NativeSymbol{"[method]value.set-double-for-idx",
		(void *)&stappler_wasm_data_method_value_set_double_for_idx, "(iFi)", NULL},
	NativeSymbol{"[method]value.set-string-for-idx",
		(void *)&stappler_wasm_data_method_value_set_string_for_idx, "(i*~i)", NULL},
	NativeSymbol{"[method]value.set-bytes-for-idx",
		(void *)&stappler_wasm_data_method_value_set_bytes_for_idx, "(i*~i)", NULL},
	NativeSymbol{"[method]value.set-dict-for-idx",
		(void *)&stappler_wasm_data_method_value_set_dict_for_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.set-array-for-idx",
		(void *)&stappler_wasm_data_method_value_set_array_for_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.set-value-for-idx",
		(void *)&stappler_wasm_data_method_value_set_value_for_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.set-value-copy-for-idx",
		(void *)&stappler_wasm_data_method_value_set_value_copy_for_idx, "(iii)i", NULL},
	NativeSymbol{"[method]value.set-null-for-key",
		(void *)&stappler_wasm_data_method_value_set_null_for_key, "(i*~)", NULL},
	NativeSymbol{"[method]value.set-bool-for-key",
		(void *)&stappler_wasm_data_method_value_set_bool_for_key, "(ii*~)", NULL},
	NativeSymbol{"[method]value.set-integer-for-key",
		(void *)&stappler_wasm_data_method_value_set_integer_for_key, "(iI*~)", NULL},
	NativeSymbol{"[method]value.set-double-for-key",
		(void *)&stappler_wasm_data_method_value_set_double_for_key, "(iF*~)", NULL},
	NativeSymbol{"[method]value.set-string-for-key",
		(void *)&stappler_wasm_data_method_value_set_string_for_key, "(i*~*~)", NULL},
	NativeSymbol{"[method]value.set-bytes-for-key",
		(void *)&stappler_wasm_data_method_value_set_bytes_for_key, "(i*~*~)", NULL},
	NativeSymbol{"[method]value.set-dict-for-key",
		(void *)&stappler_wasm_data_method_value_set_dict_for_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.set-array-for-key",
		(void *)&stappler_wasm_data_method_value_set_array_for_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.set-value-for-key",
		(void *)&stappler_wasm_data_method_value_set_value_for_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.set-value-copy-for-key",
		(void *)&stappler_wasm_data_method_value_set_value_copy_for_key, "(ii*~)i", NULL},
	NativeSymbol{"[method]value.add-null", (void *)&stappler_wasm_data_method_value_add_null, "(i)",
		NULL},
	NativeSymbol{"[method]value.add-bool", (void *)&stappler_wasm_data_method_value_add_bool,
		"(ii)", NULL},
	NativeSymbol{"[method]value.add-integer", (void *)&stappler_wasm_data_method_value_add_integer,
		"(iI)", NULL},
	NativeSymbol{"[method]value.add-double", (void *)&stappler_wasm_data_method_value_add_double,
		"(iF)", NULL},
	NativeSymbol{"[method]value.add-string", (void *)&stappler_wasm_data_method_value_add_string,
		"(i*~)", NULL},
	NativeSymbol{"[method]value.add-bytes", (void *)&stappler_wasm_data_method_value_add_bytes,
		"(i*~)", NULL},
	NativeSymbol{"[method]value.add-dict", (void *)&stappler_wasm_data_method_value_add_dict,
		"(i)i", NULL},
	NativeSymbol{"[method]value.add-array", (void *)&stappler_wasm_data_method_value_add_array,
		"(i)i", NULL},
	NativeSymbol{"[method]value.add-value", (void *)&stappler_wasm_data_method_value_add_value,
		"(i)i", NULL},
	NativeSymbol{"[method]value.add-value-copy",
		(void *)&stappler_wasm_data_method_value_add_value_copy, "(ii)i", NULL},
	NativeSymbol{"[method]value.erase-for-idx",
		(void *)&stappler_wasm_data_method_value_erase_for_idx, "(ii)i", NULL},
	NativeSymbol{"[method]value.erase-for-key",
		(void *)&stappler_wasm_data_method_value_erase_for_key, "(i*~)i", NULL},
	NativeSymbol{"[method]value.is-equal", (void *)&stappler_wasm_data_method_value_is_equal,
		"(ii)i", NULL},
	NativeSymbol{"[method]value.is-not-equal",
		(void *)&stappler_wasm_data_method_value_is_not_equal, "(ii)i", NULL},

	NativeSymbol{"[resource-drop]value", (void *)&StapplerDataDrop, "(i)", NULL}};

static NativeModule s_dataModule("stappler:wasm/data", stapper_data_symbols,
		sizeof(stapper_data_symbols) / sizeof(NativeSymbol));

} // namespace stappler::wasm
