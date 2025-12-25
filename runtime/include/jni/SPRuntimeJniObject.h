/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_JNI_SPRUNTIMEJNIOBJECT_H_
#define CORE_RUNTIME_INCLUDE_JNI_SPRUNTIMEJNIOBJECT_H_

#include "SPRuntimeJniType.h" // IWYU pragma: keep
#include "SPRuntimeLog.h"

#if SPRT_ANDROID

namespace sprt::jni::detail {

template <typename Origin>
class SPRT_API ObjectInterface {
public:
	template <typename Type>
	auto getField(jfieldID id) const -> typename TypeInfo<Type>::Result;

	template <typename Type, typename... Args>
	auto callMethod(jmethodID methodID, Args &&...args) const -> typename TypeInfo<Type>::Result;

	LocalClass getClass() const;

	LocalString getClassName() const;

private:
	JNIEnv *getObjectEnv() const;
	jobject getObject() const;
};

template <typename Origin>
class SPRT_API ClassInterface {
public:
	jmethodID getMethodID(const char *name, const char *sig) const;

	jmethodID getStaticMethodID(const char *name, const char *sig) const;

	jfieldID getFieldID(const char *name, const char *sig) const;

	jfieldID getStaticFieldID(const char *name, const char *sig) const;

	template <typename Type>
	auto getStaticField(jfieldID id) const -> typename SignatureWrapper<Type>::Result;

	template <typename Type>
	auto getStaticField(const char *name) const -> typename SignatureWrapper<Type>::Result;

	template <typename Type, typename... Args>
	auto callStaticMethod(jmethodID methodID, Args &&...args) const ->
			typename TypeInfo<Type>::Result;

	template <typename... Args>
	auto callConstructor(jmethodID methodID, Args &&...args) const -> Local;

	template <typename T>
	auto get(GetFlags = GetFlags::None) const;

	jint registerNatives(const JNINativeMethod *methods, jint nMethods) const;
	jint registerNatives(SpanView<JNINativeMethod>) const;
	jint unregisterNatives() const;

	LocalString getName() const;

private:
	JNIEnv *getClassEnv() const;
	jclass getInterfaceClass() const;
};

template <typename Origin>
class SPRT_API StringInterface {
public:
	WideStringView getWideString();
	StringView getString();

	void reset();

	void swap(StringInterface &);

private:
	JNIEnv *getStringEnv() const;
	jstring getInterfaceString() const;

protected:
	jboolean _isCopy = 0;
	jboolean _utfIsCopy = 0;
	const jchar *_chars = nullptr;
	const char *_utfChar = nullptr;
};

template <typename Origin, typename ElementType>
class SPRT_API BasicArrayInterface {
public:
	using ArrayType = typename TypeInfo<ElementType>::ArrayType;

	SpanView<ElementType> getArray();
	void setArray(SpanView<ElementType>);

	ElementType getElement(size_t);
	void setElement(size_t, ElementType);

	size_t size() const;

	void reset();
	void swap(BasicArrayInterface &);

	auto begin() { return getArray().begin(); }
	auto end() { return getArray().end(); }

private:
	JNIEnv *getArrayEnv() const;
	ArrayType getInterfaceArray() const;

protected:
	bool _dirty = false;
	jboolean _arrayIsCopy = 0;
	size_t _arrayBufferSize = 0;
	ElementType *_arrayBuffer = nullptr;
};

template <typename Origin>
class SPRT_API ObjectArrayInterface {
public:
	struct Iterator {
		Iterator(ObjectArrayInterface *ptr, size_t idx);

		Iterator &operator++();
		Iterator &operator--();

		Local operator*();

		bool operator==(const Iterator &other) const;
		bool operator!=(const Iterator &other) const;

		ObjectArrayInterface *ptr = nullptr;
		size_t idx = -1;
	};

	using ArrayType = jobjectArray;

	Local getElement(size_t) const;
	void setElement(size_t, const Ref &);
	void setElement(size_t, const jobject &);

	size_t size() const;

	void reset();
	void swap(ObjectArrayInterface &);

	Iterator begin() { return Iterator(this, 0); }
	Iterator end() { return Iterator(this, size()); }

private:
	JNIEnv *getArrayEnv() const;
	ArrayType getInterfaceArray() const;
};

template <typename Origin, typename Type>
class ArrayInterface;

template <typename Origin>
class ArrayInterface<Origin, jobject> : public ObjectArrayInterface<Origin> { };

template <typename Origin>
class ArrayInterface<Origin, jclass> : public ObjectArrayInterface<Origin> { };

template <typename Origin>
class ArrayInterface<Origin, jstring> : public ObjectArrayInterface<Origin> { };

template <typename Origin>
class ArrayInterface<Origin, jboolean> : public BasicArrayInterface<Origin, jboolean> { };

template <typename Origin>
class ArrayInterface<Origin, jbyte> : public BasicArrayInterface<Origin, jbyte> { };

template <typename Origin>
class ArrayInterface<Origin, jchar> : public BasicArrayInterface<Origin, jchar> { };

template <typename Origin>
class ArrayInterface<Origin, jshort> : public BasicArrayInterface<Origin, jshort> { };

template <typename Origin>
class ArrayInterface<Origin, jint> : public BasicArrayInterface<Origin, jint> { };

template <typename Origin>
class ArrayInterface<Origin, jlong> : public BasicArrayInterface<Origin, jlong> { };

template <typename Origin>
class ArrayInterface<Origin, jfloat> : public BasicArrayInterface<Origin, jfloat> { };

template <typename Origin>
class ArrayInterface<Origin, jdouble> : public BasicArrayInterface<Origin, jdouble> { };

} // namespace sprt::jni::detail


namespace sprt::jni {

class SPRT_API Local : public detail::ObjectInterface<Local> {
public:
	~Local();
	Local(jobject obj, JNIEnv *env);

	Local(const Local &) = delete;
	Local &operator=(const Local &) = delete;

	Local(Local &&other);
	Local &operator=(Local &&);

	Local(nullptr_t);
	Local &operator=(nullptr_t);

	Local() : Local(nullptr) { }

	jobject getObject() const { return _obj; }
	operator jobject() const { return getObject(); }
	explicit operator bool() const { return _obj != nullptr; }

	JNIEnv *getEnv() const { return _env; }

	Global getGlobal() const;

protected:
	jobject _obj = nullptr;
	JNIEnv *_env = nullptr;
};

class SPRT_API LocalString : public Local, public detail::StringInterface<LocalString> {
public:
	~LocalString();
	LocalString(jstring obj, JNIEnv *env);

	LocalString(const LocalString &) = delete;
	LocalString &operator=(const LocalString &) = delete;

	LocalString(LocalString &&other);
	LocalString &operator=(LocalString &&);

	LocalString(nullptr_t);
	LocalString &operator=(nullptr_t);

	LocalString() : LocalString(nullptr) { }

	jstring getObject() const { return static_cast<jstring>(_obj); }
	operator jstring() const { return getObject(); }

	GlobalString getGlobal() const;
};

class SPRT_API LocalClass : public Local, public detail::ClassInterface<LocalClass> {
public:
	LocalClass(jclass obj, JNIEnv *env);

	LocalClass(const LocalClass &) = delete;
	LocalClass &operator=(const LocalClass &) = delete;

	LocalClass(LocalClass &&other);
	LocalClass &operator=(LocalClass &&);

	LocalClass(nullptr_t);
	LocalClass &operator=(nullptr_t);

	LocalClass() : LocalClass(nullptr) { }

	jclass getObject() const { return static_cast<jclass>(_obj); }
	operator jclass() const { return getObject(); }

	GlobalClass getGlobal() const;
};

template <typename ElementType>
class SPRT_API LocalArray : public Local,
							public detail::ArrayInterface<LocalArray<ElementType>, ElementType> {
public:
	using ArrayType = typename detail::TypeInfo<ElementType>::ArrayType;

	~LocalArray();
	LocalArray(ArrayType obj, JNIEnv *env);

	LocalArray(const LocalArray &) = delete;
	LocalArray &operator=(const LocalArray &) = delete;

	LocalArray(LocalArray &&other);
	LocalArray &operator=(LocalArray &&other);

	LocalArray(nullptr_t);
	LocalArray &operator=(nullptr_t);

	LocalArray() : LocalArray(nullptr) { }

	ArrayType getObject() const { return static_cast<ArrayType>(_obj); }
	operator ArrayType() const { return getObject(); }

	GlobalArray<ElementType> getGlobal() const;
};

class SPRT_API Global : public detail::ObjectInterface<Global> {
public:
	~Global();
	Global(const Local &obj);
	Global(const Ref &obj);

	Global(const Global &);
	Global &operator=(const Global &);

	Global(Global &&other);
	Global &operator=(Global &&);

	Global(nullptr_t);
	Global &operator=(nullptr_t);

	jobject getObject() const { return _obj; }
	operator jobject() const { return getObject(); }
	explicit operator bool() const { return _obj != nullptr; }

	JNIEnv *getEnv() const;

	Ref ref(JNIEnv *env = nullptr) const;

protected:
	jobject _obj = nullptr;
};

class SPRT_API GlobalString : public Global /*, public detail::StringInterface<GlobalString>*/ {
public:
	GlobalString(const LocalString &obj);
	GlobalString(const RefString &obj);

	GlobalString(const GlobalString &);
	GlobalString &operator=(const GlobalString &);

	GlobalString(GlobalString &&other);
	GlobalString &operator=(GlobalString &&);

	GlobalString(nullptr_t);
	GlobalString &operator=(nullptr_t);

	jstring getObject() const { return static_cast<jstring>(_obj); }
	operator jstring() const { return getObject(); }

	RefString ref(JNIEnv *env = nullptr) const;
};

class SPRT_API GlobalClass : public Global, public detail::ClassInterface<GlobalClass> {
public:
	GlobalClass(const LocalClass &obj);
	GlobalClass(const RefClass &obj);

	GlobalClass(const GlobalClass &);
	GlobalClass &operator=(const GlobalClass &);

	GlobalClass(GlobalClass &&other);
	GlobalClass &operator=(GlobalClass &&);

	GlobalClass(nullptr_t);
	GlobalClass &operator=(nullptr_t);

	jclass getObject() const { return static_cast<jclass>(_obj); }
	operator jclass() const { return getObject(); }

	RefClass ref(JNIEnv *env = nullptr) const;
};

template <typename ElementType>
class SPRT_API GlobalArray : public Global {
public:
	using ArrayType = typename detail::TypeInfo<ElementType>;

	GlobalArray(const LocalArray<ElementType> &obj);
	GlobalArray(const RefArray<ElementType> &obj);

	GlobalArray(const GlobalArray &);
	GlobalArray &operator=(const GlobalArray &);

	GlobalArray(GlobalArray &&other);
	GlobalArray &operator=(GlobalArray &&);

	GlobalArray(nullptr_t);
	GlobalArray &operator=(nullptr_t);

	ArrayType getObject() const { return static_cast<ArrayType>(_obj); }
	operator ArrayType() const { return getObject(); }

	RefClass ref(JNIEnv *env = nullptr) const;
};

class SPRT_API Ref : public detail::ObjectInterface<Ref> {
public:
	Ref(jobject obj, JNIEnv *env) : _obj(obj), _env(env) { }
	Ref(const Local &obj) : _obj(obj.getObject()), _env(obj.getEnv()) { }
	Ref(const Global &obj, JNIEnv *env) : _obj(obj.getObject()), _env(env) { }
	Ref(nullptr_t) : _obj(nullptr), _env(nullptr) { }

	jobject getObject() const { return _obj; }
	operator jobject() const { return getObject(); }
	explicit operator bool() const { return _obj != nullptr; }

	JNIEnv *getEnv() const { return _env; }

	Global getGlobal() const;

protected:
	jobject _obj = nullptr;
	JNIEnv *_env = nullptr;
};

class SPRT_API RefString : public Ref, public detail::StringInterface<RefString> {
public:
	~RefString();
	RefString(jstring obj, JNIEnv *env) : Ref(obj, env) { }
	RefString(const LocalString &obj) : Ref(obj.getObject(), obj.getEnv()) { }
	RefString(const GlobalString &obj, JNIEnv *env) : Ref(obj.getObject(), env) { }
	RefString(nullptr_t) : Ref(nullptr) { }

	jstring getObject() const { return static_cast<jstring>(_obj); }
	operator jstring() const { return getObject(); }

	GlobalString getGlobal() const;
};

class SPRT_API RefClass : public Ref, public detail::ClassInterface<RefClass> {
public:
	RefClass(jclass obj, JNIEnv *env) : Ref(obj, env) { }
	RefClass(const LocalClass &obj) : Ref(obj.getObject(), obj.getEnv()) { }
	RefClass(const GlobalClass &obj, JNIEnv *env) : Ref(obj.getObject(), env) { }
	RefClass(nullptr_t) : Ref(nullptr) { }

	jclass getObject() const { return static_cast<jclass>(_obj); }
	operator jclass() const { return getObject(); }

	GlobalClass getGlobal() const;
};

template <typename ElementType>
class SPRT_API RefArray : public Ref,
						  public detail::ArrayInterface<RefArray<ElementType>, ElementType> {
public:
	using ArrayType = typename detail::TypeInfo<ElementType>;

	RefArray(jclass obj, JNIEnv *env) : Ref(obj, env) { }
	RefArray(const LocalArray<ElementType> &obj) : Ref(obj.getObject(), obj.getEnv()) { }
	RefArray(const GlobalArray<ElementType> &obj, JNIEnv *env) : Ref(obj.getObject(), env) { }
	RefArray(nullptr_t) : Ref(nullptr) { }

	ArrayType getObject() const { return static_cast<ArrayType>(_obj); }
	operator ArrayType() const { return getObject(); }

	GlobalArray<ElementType> getGlobal() const { return GlobalArray<ElementType>(*this); }
};

} // namespace sprt::jni


namespace sprt::jni::detail {

SPRT_API inline auto Forward(const Local &l) { return l.getObject(); }
SPRT_API inline auto Forward(const LocalString &l) { return l.getObject(); }
SPRT_API inline auto Forward(const LocalClass &l) { return l.getObject(); }
SPRT_API inline auto Forward(const Global &l) { return l.getObject(); }
SPRT_API inline auto Forward(const GlobalString &l) { return l.getObject(); }
SPRT_API inline auto Forward(const GlobalClass &l) { return l.getObject(); }
SPRT_API inline auto Forward(const Ref &l) { return l.getObject(); }
SPRT_API inline auto Forward(const RefString &l) { return l.getObject(); }
SPRT_API inline auto Forward(const RefClass &l) { return l.getObject(); }
SPRT_API inline auto Forward(jobject value) { return value; }
SPRT_API inline auto Forward(jstring value) { return value; }
SPRT_API inline auto Forward(jclass value) { return value; }
SPRT_API inline auto Forward(jboolean value) { return value; }
SPRT_API inline auto Forward(jbyte value) { return value; }
SPRT_API inline auto Forward(jchar value) { return value; }
SPRT_API inline auto Forward(jshort value) { return value; }
SPRT_API inline auto Forward(jint value) { return value; }
SPRT_API inline auto Forward(jlong value) { return value; }
SPRT_API inline auto Forward(jfloat value) { return value; }
SPRT_API inline auto Forward(jdouble value) { return value; }
SPRT_API inline auto Forward(nullptr_t value) { return value; }

template <typename Origin>
template <typename Type>
auto ObjectInterface<Origin>::getField(jfieldID id) const -> typename TypeInfo<Type>::Result {
	auto result = static_cast<typename TypeInfo<Type>::Intermediate>(
			(getObjectEnv()->*TypeInfo<Type>::GetField)(getObject(), id));
	auto ret = TypeInfo<Type>::wrap(move_unsafe(result), getObjectEnv());
#if JNIDEBUG
	checkErrors(getObjectEnv());
#endif
	return ret;
}

template <typename Origin>
template <typename Type, typename... Args>
auto ObjectInterface<Origin>::callMethod(jmethodID methodID, Args &&...args) const ->
		typename TypeInfo<Type>::Result {
	if constexpr (is_same_v<Type, void>) {
		(getObjectEnv()->*TypeInfo<Type>::Call)(getObject(), methodID,
				Forward(forward<Args>(args))...);
#if JNIDEBUG
		checkErrors(getObjectEnv());
#endif
	} else {
		auto result = static_cast<typename TypeInfo<Type>::Intermediate>(
				(getObjectEnv()->*TypeInfo<Type>::Call)(getObject(), methodID,
						Forward(forward<Args>(args))...));
		auto ret = TypeInfo<Type>::wrap(sprt::move_unsafe(result), getObjectEnv());
#if JNIDEBUG
		checkErrors(getObjectEnv());
#endif
		return ret;
	}
}

template <typename Origin>
LocalClass ObjectInterface<Origin>::getClass() const {
	return LocalClass(getObjectEnv()->GetObjectClass(getObject()), getObjectEnv());
}

template <typename Origin>
LocalString ObjectInterface<Origin>::getClassName() const {
	auto obj = Ref(getObject(), getObjectEnv());
	auto cl = obj.getClass();
	return cl.getName();
}

template <typename Origin>
JNIEnv *ObjectInterface<Origin>::getObjectEnv() const {
	return static_cast<const Origin *>(this)->getEnv();
}

template <typename Origin>
jobject ObjectInterface<Origin>::getObject() const {
	return static_cast<const Origin *>(this)->getObject();
}

template <typename Origin>
jmethodID ClassInterface<Origin>::getMethodID(const char *name, const char *sig) const {
	auto ret = getClassEnv()->GetMethodID(getInterfaceClass(), name, sig);
#if JNIDEBUG
	checkErrors(getClassEnv());
#endif
	return ret;
}

template <typename Origin>
jmethodID ClassInterface<Origin>::getStaticMethodID(const char *name, const char *sig) const {
	auto ret = getClassEnv()->GetStaticMethodID(getInterfaceClass(), name, sig);
#if JNIDEBUG
	checkErrors(getClassEnv());
#endif
	return ret;
}

template <typename Origin>
jfieldID ClassInterface<Origin>::getFieldID(const char *name, const char *sig) const {
	auto ret = getClassEnv()->GetFieldID(getInterfaceClass(), name, sig);
#if JNIDEBUG
	checkErrors(getClassEnv());
#endif
	return ret;
}

template <typename Origin>
jfieldID ClassInterface<Origin>::getStaticFieldID(const char *name, const char *sig) const {
	auto ret = getClassEnv()->GetStaticFieldID(getInterfaceClass(), name, sig);
#if JNIDEBUG
	checkErrors(getClassEnv());
#endif
	return ret;
}

template <typename Origin>
template <typename Type>
auto ClassInterface<Origin>::getStaticField(jfieldID id) const ->
		typename SignatureWrapper<Type>::Result {
	auto result = static_cast<typename TypeInfo<Type>::Intermediate>(
			(getClassEnv()->*TypeInfo<Type>::GetStaticField)(getInterfaceClass(), id));
	auto ret = TypeInfo<Type>::wrap(move_unsafe(result), getClassEnv());
#if JNIDEBUG
	checkErrors(getClassEnv());
#endif
	return ret;
}

template <typename Origin>
template <typename Type>
auto ClassInterface<Origin>::getStaticField(const char *name) const ->
		typename SignatureWrapper<Type>::Result {
	auto fieldId = getStaticFieldID(name, FieldSignature<Type>::Buffer.data());
	if (!fieldId) {
		log::vprint(log::LogType::Error, __SPRT_LOCATION, "JNI", "Fail to find static field (",
				FieldSignature<Type>::Buffer.data(), ") '", name, "' in class '",
				getName().getString(), "'");
		sprt_passert(fieldId, "Fail to find static field id");
	}
	return getStaticField<Type>(fieldId);
}

template <typename Origin>
template <typename Type, typename... Args>
auto ClassInterface<Origin>::callStaticMethod(jmethodID methodID, Args &&...args) const ->
		typename TypeInfo<Type>::Result {
	if constexpr (is_same_v<Type, void>) {
		(getClassEnv()->*TypeInfo<Type>::CallStatic)(getInterfaceClass(), methodID,
				Forward(forward<Args>(args))...);
#if JNIDEBUG
		checkErrors(getClassEnv());
#endif
	} else {
		auto result = static_cast<typename TypeInfo<Type>::Intermediate>(
				(getClassEnv()->*TypeInfo<Type>::CallStatic)(getInterfaceClass(), methodID,
						Forward(forward<Args>(args))...));
		auto ret = TypeInfo<Type>::wrap(sprt::move_unsafe(result), getClassEnv());
#if JNIDEBUG
		checkErrors(getClassEnv());
#endif
		return ret;
	}
}

template <typename Origin>
template <typename... Args>
auto ClassInterface<Origin>::callConstructor(jmethodID methodID, Args &&...args) const -> Local {
	return Local(getClassEnv()->NewObject(getInterfaceClass(), methodID,
						 Forward(forward<Args>(args))...),
			getClassEnv());
}

template <typename T>
struct ClassInterfaceGetter;

template <detail::ClassNameSignature Value, typename T>
struct ClassInterfaceGetter<Method<Value, T>> {
	template <typename Origin>
	static auto get(const ClassInterface<Origin> &origin, GetFlags flags) -> Method<Value, T> {
		auto methodId = origin.getMethodID(Value.data.data(), MethodSignature<T>::Buffer.data());
		if (!methodId && !hasFlag(flags, Optional)) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "JNI", "Method ", Value.data.data(),
					" : ", MethodSignature<T>::Name, " not found in class ",
					origin.getName().getString());
			sprt_passert(methodId, "Method not found");
		}
		return methodId;
	}
};

template <detail::ClassNameSignature Value, typename T>
struct ClassInterfaceGetter<StaticMethod<Value, T>> {
	template <typename Origin>
	static auto get(const ClassInterface<Origin> &origin, GetFlags flags)
			-> StaticMethod<Value, T> {
		auto methodId =
				origin.getStaticMethodID(Value.data.data(), MethodSignature<T>::Buffer.data());
		if (!methodId && !hasFlag(flags, Optional)) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "JNI", "Static method ",
					Value.data.data(), " : ", MethodSignature<T>::Name, " not found in class ",
					origin.getName().getString());
			sprt_passert(methodId, "Static method not found");
		}
		return methodId;
	}
};

template <typename T>
struct ClassInterfaceGetter<Constructor<T>> {
	template <typename Origin>
	static auto get(const ClassInterface<Origin> &origin, GetFlags flags) -> Constructor<T> {
		auto methodId = origin.getMethodID("<init>", MethodSignature<T>::Buffer.data());
		if (!methodId && !hasFlag(flags, Optional)) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "JNI", "Constructor ",
					MethodSignature<T>::Name, " not found in class ", origin.getName().getString());
			sprt_passert(methodId, "Constructor not found");
		}
		return methodId;
	}
};

template <detail::ClassNameSignature Value, typename T>
struct ClassInterfaceGetter<Field<Value, T>> {
	template <typename Origin>
	static auto get(const ClassInterface<Origin> &origin, GetFlags flags) -> Field<Value, T> {
		auto fieldId = origin.getFieldID(Value.data.data(), FieldSignature<T>::Buffer.data());
		if (!fieldId && !hasFlag(flags, Optional)) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "JNI", "Field (",
					FieldSignature<T>::Name, ") ", Value.data.data(), " not found in class ",
					origin.getName().getString());
			sprt_passert(fieldId, "Field not found");
		}
		return fieldId;
	}
};

template <detail::ClassNameSignature Value, typename T>
struct ClassInterfaceGetter<StaticField<Value, T>> {
	template <typename Origin>
	static auto get(const ClassInterface<Origin> &origin, GetFlags flags) -> StaticField<Value, T> {
		auto fieldId = origin.getFieldID(Value.data.data(), FieldSignature<T>::Buffer.data());
		if (!fieldId && !hasFlag(flags, Optional)) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "JNI", "Static field (",
					FieldSignature<T>::Name, ") ", Value.data.data(), " not found in class ",
					origin.getName().getString());
			sprt_passert(fieldId, "Static field not found");
		}
		return fieldId;
	}
};

template <typename Origin>
template <typename T>
auto ClassInterface<Origin>::get(GetFlags flags) const {
	return ClassInterfaceGetter<T>::get(*this, flags);
}

template <typename Origin>
jint ClassInterface<Origin>::registerNatives(const JNINativeMethod *methods, jint nMethods) const {
	return getClassEnv()->RegisterNatives(getInterfaceClass(), methods, nMethods);
}

template <typename Origin>
jint ClassInterface<Origin>::registerNatives(SpanView<JNINativeMethod> methods) const {
	return getClassEnv()->RegisterNatives(getInterfaceClass(), methods.data(), methods.size());
}

template <typename Origin>
jint ClassInterface<Origin>::unregisterNatives() const {
	return getClassEnv()->UnregisterNatives(getInterfaceClass());
}

template <typename Origin>
LocalString ClassInterface<Origin>::getName() const {
	static jmethodID getName = nullptr;
	auto cl = RefClass(getInterfaceClass(), getClassEnv());
	if (!getName) {
		auto classClass = cl.getClass();
		getName = classClass.getMethodID("getName", "()Ljava/lang/String;");
	}
	return cl.callMethod<jstring>(getName);
}

template <typename Origin>
JNIEnv *ClassInterface<Origin>::getClassEnv() const {
	return static_cast<const Origin *>(this)->getEnv();
}

template <typename Origin>
jclass ClassInterface<Origin>::getInterfaceClass() const {
	return static_cast<const Origin *>(this)->getObject();
}

template <typename Origin>
WideStringView StringInterface<Origin>::getWideString() {
	if (!_chars) {
		_chars = getStringEnv()->GetStringChars(getInterfaceString(), &_isCopy);
	}

	return WideStringView((const char16_t *)_chars,
			getStringEnv()->GetStringLength(getInterfaceString()));
}

template <typename Origin>
StringView StringInterface<Origin>::getString() {
	if (!_utfChar) {
		_utfChar = getStringEnv()->GetStringUTFChars(getInterfaceString(), &_utfIsCopy);
	}

	return StringView(_utfChar, getStringEnv()->GetStringUTFLength(getInterfaceString()));
}

template <typename Origin>
void StringInterface<Origin>::reset() {
	if (_chars) {
		getStringEnv()->ReleaseStringChars(getInterfaceString(), _chars);
		_chars = nullptr;
	}

	if (_utfChar) {
		getStringEnv()->ReleaseStringUTFChars(getInterfaceString(), _utfChar);
		_utfChar = nullptr;
	}
}

template <typename Origin>
void StringInterface<Origin>::swap(StringInterface &other) {
	jboolean tmp_isCopy = _isCopy;
	jboolean tmp_utfIsCopy = _utfIsCopy;
	const jchar *tmp_chars = _chars;
	const char *tmp_utfChar = _utfChar;

	_isCopy = other._isCopy;
	_utfIsCopy = other._utfIsCopy;
	_chars = other._chars;
	_utfChar = other._utfChar;

	other._isCopy = tmp_isCopy;
	other._utfIsCopy = tmp_utfIsCopy;
	other._chars = tmp_chars;
	other._utfChar = tmp_utfChar;
}

template <typename Origin>
JNIEnv *StringInterface<Origin>::getStringEnv() const {
	return static_cast<const Origin *>(this)->getEnv();
}

template <typename Origin>
jstring StringInterface<Origin>::getInterfaceString() const {
	return static_cast<const Origin *>(this)->getObject();
}

template <typename Origin, typename ElementType>
SpanView<ElementType> BasicArrayInterface<Origin, ElementType>::getArray() {
	if (!_arrayBuffer) {
		auto env = getArrayEnv();
		_arrayBuffer = (env->*TypeInfo<ElementType>::GetArray)(getInterfaceArray(), &_arrayIsCopy);
		_arrayBufferSize = env->GetArrayLength(getInterfaceArray());
	}

	return SpanView<ElementType>(_arrayBuffer, _arrayBufferSize);
}

template <typename Origin, typename ElementType>
void BasicArrayInterface<Origin, ElementType>::setArray(SpanView<ElementType> array) {
	reset();

	(getArrayEnv()->*TypeInfo<ElementType>::SetArray)(getInterfaceArray(), 0,
			min(array.size(), size()), array.data());
}

template <typename Origin, typename ElementType>
ElementType BasicArrayInterface<Origin, ElementType>::getElement(size_t idx) {
	auto arr = getArray();
	if (idx < arr.size()) {
		return arr[idx];
	}
	return ElementType(0);
}

template <typename Origin, typename ElementType>
void BasicArrayInterface<Origin, ElementType>::setElement(size_t idx, ElementType value) {
	auto arr = getArray();
	if (_arrayBuffer && idx < arr.size()) {
		_arrayBuffer[idx] = value;
		_dirty = true;
	}
}

template <typename Origin, typename ElementType>
size_t BasicArrayInterface<Origin, ElementType>::size() const {
	return getArrayEnv()->GetArrayLength(getInterfaceArray());
}

template <typename Origin, typename ElementType>
void BasicArrayInterface<Origin, ElementType>::reset() {
	if (_arrayBuffer) {
		(getArrayEnv()->*TypeInfo<ElementType>::ReleaseArray)(getInterfaceArray(), _arrayBuffer,
				_dirty ? JNI_COMMIT : JNI_ABORT);
		_arrayBuffer = 0;
		_dirty = false;
	}
}

template <typename Origin, typename ElementType>
void BasicArrayInterface<Origin, ElementType>::swap(BasicArrayInterface &other) {
	bool tmp_dirty = _dirty;
	jboolean tmp_arrayIsCopy = _arrayIsCopy;
	size_t tmp_arrayBufferSize = _arrayBufferSize;
	auto tmp_arrayBuffer = _arrayBuffer;

	_dirty = other._dirty;
	_arrayIsCopy = other._arrayIsCopy;
	_arrayBufferSize = other._arrayBufferSize;
	_arrayBuffer = other._arrayBuffer;

	other._dirty = tmp_dirty;
	other._arrayIsCopy = tmp_arrayIsCopy;
	other._arrayBufferSize = tmp_arrayBufferSize;
	other._arrayBuffer = tmp_arrayBuffer;
}

template <typename Origin, typename ElementType>
JNIEnv *BasicArrayInterface<Origin, ElementType>::getArrayEnv() const {
	return static_cast<const Origin *>(this)->getEnv();
}

template <typename Origin, typename ElementType>
auto BasicArrayInterface<Origin, ElementType>::getInterfaceArray() const -> ArrayType {
	return static_cast<const Origin *>(this)->getObject();
}

template <typename Origin>
ObjectArrayInterface<Origin>::Iterator::Iterator(ObjectArrayInterface *ptr, size_t idx)
: ptr(ptr), idx(idx) { }

template <typename Origin>
auto ObjectArrayInterface<Origin>::Iterator::operator++() -> Iterator & {
	++idx;
	return *this;
}

template <typename Origin>
auto ObjectArrayInterface<Origin>::Iterator::operator--() -> Iterator & {
	--idx;
	return *this;
}

template <typename Origin>
Local ObjectArrayInterface<Origin>::Iterator::operator*() {
	return ptr->getElement(idx);
}

template <typename Origin>
bool ObjectArrayInterface<Origin>::Iterator::operator==(const Iterator &other) const {
	return ptr == other.ptr && idx == other.idx;
}

template <typename Origin>
bool ObjectArrayInterface<Origin>::Iterator::operator!=(const Iterator &other) const {
	return ptr != other.ptr || idx != other.idx;
}

template <typename Origin>
Local ObjectArrayInterface<Origin>::getElement(size_t idx) const {
	auto env = getArrayEnv();
	return Local(env->GetObjectArrayElement(getInterfaceArray(), idx), env);
}

template <typename Origin>
void ObjectArrayInterface<Origin>::setElement(size_t idx, const Ref &ref) {
	ref.getEnv()->SetObjectArrayElement(getInterfaceArray(), idx, ref);
}

template <typename Origin>
void ObjectArrayInterface<Origin>::setElement(size_t idx, const jobject &ref) {
	getArrayEnv()->SetObjectArrayElement(getInterfaceArray(), idx, ref);
}

template <typename Origin>
size_t ObjectArrayInterface<Origin>::size() const {
	return getArrayEnv()->GetArrayLength(getInterfaceArray());
}

template <typename Origin>
void ObjectArrayInterface<Origin>::reset() { }

template <typename Origin>
void ObjectArrayInterface<Origin>::swap(ObjectArrayInterface &) { }

template <typename Origin>
JNIEnv *ObjectArrayInterface<Origin>::getArrayEnv() const {
	return static_cast<const Origin *>(this)->getEnv();
}

template <typename Origin>
auto ObjectArrayInterface<Origin>::getInterfaceArray() const -> ArrayType {
	return static_cast<const Origin *>(this)->getObject();
}

} // namespace sprt::jni::detail


namespace sprt::jni {

template <typename ElementType>
LocalArray<ElementType>::~LocalArray() {
	detail::ArrayInterface<LocalArray<ElementType>, ElementType>::reset();
}

template <typename ElementType>
LocalArray<ElementType>::LocalArray(ArrayType obj, JNIEnv *env) : Local(obj, env) { }

template <typename ElementType>
LocalArray<ElementType>::LocalArray(LocalArray &&other) : Local(move_unsafe(other)) {
	detail::ArrayInterface<LocalArray<ElementType>, ElementType>::swap(other);
}

template <typename ElementType>
auto LocalArray<ElementType>::operator=(LocalArray &&other) -> LocalArray & {
	if (&other == this) {
		return *this;
	}

	detail::ArrayInterface<LocalArray<ElementType>, ElementType>::reset();
	Local::operator=(move_unsafe(other));
	detail::ArrayInterface<LocalArray<ElementType>, ElementType>::swap(other);
	return *this;
}

template <typename ElementType>
LocalArray<ElementType>::LocalArray(nullptr_t) : Local(nullptr) { }

template <typename ElementType>
auto LocalArray<ElementType>::operator=(nullptr_t) -> LocalArray & {
	detail::ArrayInterface<LocalArray<ElementType>, ElementType>::reset();
	Local::operator=(nullptr);
	return *this;
}

template <typename ElementType>
auto LocalArray<ElementType>::getGlobal() const -> GlobalArray<ElementType> {
	return GlobalArray<ElementType>(*this);
}

template <typename ElementType>
GlobalArray<ElementType>::GlobalArray(const LocalArray<ElementType> &obj) : Global(nullptr) {
	if (obj) {
		_obj = obj.getEnv()->NewGlobalRef(obj);
	}
}

template <typename ElementType>
GlobalArray<ElementType>::GlobalArray(const RefArray<ElementType> &obj) : Global(nullptr) {
	if (obj) {
		_obj = obj.getEnv()->NewGlobalRef(obj);
	}
}

template <typename ElementType>
GlobalArray<ElementType>::GlobalArray(const GlobalArray &other) : Global(other) { }

template <typename ElementType>
auto GlobalArray<ElementType>::operator=(const GlobalArray &other) -> GlobalArray & {
	if (&other == this) {
		return *this;
	}

	Global::operator=(other);
	return *this;
}

template <typename ElementType>
GlobalArray<ElementType>::GlobalArray(GlobalArray &&other) : Global(move_unsafe(other)) { }

template <typename ElementType>
auto GlobalArray<ElementType>::operator=(GlobalArray &&other) -> GlobalArray & {
	if (&other == this) {
		return *this;
	}

	Global::operator=(move_unsafe(other));
	return *this;
}

template <typename ElementType>
GlobalArray<ElementType>::GlobalArray(nullptr_t) : Global(nullptr) { }

template <typename ElementType>
auto GlobalArray<ElementType>::operator=(nullptr_t) -> GlobalArray & {
	Global::operator=(nullptr);
	return *this;
}

template <typename ElementType>
RefClass GlobalArray<ElementType>::ref(JNIEnv *env) const {
	return RefArray<ElementType>(*this, env ? env : getEnv());
}

} // namespace sprt::jni

#endif

#endif // CORE_RUNTIME_INCLUDE_JNI_SPRUNTIMEJNIOBJECT_H_
