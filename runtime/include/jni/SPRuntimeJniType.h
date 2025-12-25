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

#ifndef CORE_RUNTIME_INCLUDE_JNI_SPRUNTIMEJNITYPE_H_
#define CORE_RUNTIME_INCLUDE_JNI_SPRUNTIMEJNITYPE_H_

#include "SPRuntimeString.h" // IWYU pragma: keep
#include "SPRuntimeArray.h"

#if SPRT_ANDROID

#define JNIDEBUG 1

#include <jni.h>

namespace sprt::jni::detail {

// Type to store string literal as temp0lte argument
template <size_t N>
struct ClassNameSignature {
	array<char, N> data;

	// Constexpr constructor to initialize from a string literal
	template <size_t... Is>
	constexpr ClassNameSignature(const char (&s)[N], integer_sequence<size_t, Is...>)
	: data{{s[Is]...}} { }

	constexpr ClassNameSignature(const char (&s)[N])
	: ClassNameSignature(s, make_index_sequence<N>{}) { }
};

} // namespace sprt::jni::detail

namespace sprt::jni {

enum GetFlags {
	None = 0,
	Optional = 1 << 0,
};


class Local;
class LocalClass;
class LocalString;

template <typename ElementType>
class LocalArray;

class Global;
class GlobalClass;
class GlobalString;

template <typename ElementType>
class GlobalArray;

class Ref;
class RefClass;
class RefString;

template <typename ElementType>
class RefArray;

template <detail::ClassNameSignature Name, typename Signature>
class Method;

template <detail::ClassNameSignature Name, typename Signature>
class StaticMethod;

template <typename Signature>
class Constructor;

template <detail::ClassNameSignature Name, typename Signature>
class Field;

template <detail::ClassNameSignature Name, typename Signature>
class StaticField;

} // namespace sprt::jni

namespace sprt::jni::detail {

template <typename _Type, typename _Result>
struct ObjectTypeInfo {
	using Type = _Type;
	using Result = _Result;
	using ArrayType = jobjectArray;
	using Intermediate = Type;

	static constexpr auto Call = &_JNIEnv::CallObjectMethod;
	static constexpr auto CallStatic = &_JNIEnv::CallStaticObjectMethod;
	static constexpr auto CallNonvirtual = &_JNIEnv::CallNonvirtualObjectMethod;

	static constexpr auto GetField = &_JNIEnv::GetObjectField;
	static constexpr auto GetStaticField = &_JNIEnv::GetStaticObjectField;
	static constexpr auto SetField = &_JNIEnv::SetObjectField;
	static constexpr auto SetStaticField = &_JNIEnv::SetStaticObjectField;

	static constexpr auto NewArray = &_JNIEnv::NewObjectArray;
};

template <typename _Type, typename _Result>
struct ArrayTypeInfo {
	using Type = _Type;
	using Result = _Result;
	using ArrayType = jobjectArray;
	using Intermediate = Type;

	static constexpr auto Call = &_JNIEnv::CallObjectMethod;
	static constexpr auto CallStatic = &_JNIEnv::CallStaticObjectMethod;
	static constexpr auto CallNonvirtual = &_JNIEnv::CallNonvirtualObjectMethod;

	static constexpr auto GetField = &_JNIEnv::GetObjectField;
	static constexpr auto GetStaticField = &_JNIEnv::GetStaticObjectField;
	static constexpr auto SetField = &_JNIEnv::SetObjectField;
	static constexpr auto SetStaticField = &_JNIEnv::SetStaticObjectField;
};

template <typename Type>
struct SPRT_API TypeInfo;

template <>
struct SPRT_API TypeInfo<jobject> : public ObjectTypeInfo<jobject, Local> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jstring> : public ObjectTypeInfo<jstring, LocalString> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jclass> : public ObjectTypeInfo<jclass, LocalClass> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jarray> : public ArrayTypeInfo<jarray, LocalArray<jobject>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jobjectArray> : public ArrayTypeInfo<jobjectArray, LocalArray<jobject>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API
		TypeInfo<jbooleanArray> : public ArrayTypeInfo<jbooleanArray, LocalArray<jboolean>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jbyteArray> : public ArrayTypeInfo<jbyteArray, LocalArray<jbyte>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jcharArray> : public ArrayTypeInfo<jcharArray, LocalArray<jchar>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jshortArray> : public ArrayTypeInfo<jshortArray, LocalArray<jshort>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jintArray> : public ArrayTypeInfo<jintArray, LocalArray<jint>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jlongArray> : public ArrayTypeInfo<jlongArray, LocalArray<jlong>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jfloatArray> : public ArrayTypeInfo<jfloatArray, LocalArray<jfloat>> {
	static Result wrap(Type t, JNIEnv *env);
};

template <>
struct SPRT_API TypeInfo<jdoubleArray> : public ArrayTypeInfo<jdoubleArray, LocalArray<jdouble>> {
	static Result wrap(Type t, JNIEnv *env);
};

#define SPJNI_DEFINE_TYPE_METHODS(Name) \
	static constexpr auto Call = &_JNIEnv::Call##Name##Method; \
	static constexpr auto CallStatic = &_JNIEnv::CallStatic##Name##Method; \
	static constexpr auto CallNonvirtual = &_JNIEnv::CallNonvirtual##Name##Method; \
	static constexpr auto GetField = &_JNIEnv::Get##Name##Field; \
	static constexpr auto GetStaticField = &_JNIEnv::GetStatic##Name##Field; \
	static constexpr auto SetField = &_JNIEnv::Set##Name##Field; \
	static constexpr auto SetStaticField = &_JNIEnv::SetStatic##Name##Field; \
	static constexpr auto NewArray = &_JNIEnv::New##Name##Array; \
	static constexpr auto GetArray = &_JNIEnv::Get##Name##ArrayElements; \
	static constexpr auto GetArrayRegion = &_JNIEnv::Get##Name##ArrayRegion; \
	static constexpr auto SetArrayRegion = &_JNIEnv::Set##Name##ArrayRegion; \
	static constexpr auto ReleaseArray = &_JNIEnv::Release##Name##ArrayElements;

template <>
struct TypeInfo<jboolean> {
	using Type = jboolean;
	using ArrayType = jbooleanArray;

	using Result = Type;
	using Intermediate = Type;

	SPJNI_DEFINE_TYPE_METHODS(Boolean)

	static constexpr Result wrap(Type t, JNIEnv *env) { return t; }
};

template <>
struct TypeInfo<jbyte> {
	using Type = jbyte;
	using ArrayType = jbyteArray;

	using Result = Type;
	using Intermediate = Type;

	SPJNI_DEFINE_TYPE_METHODS(Byte)

	static constexpr Result wrap(Type t, JNIEnv *env) { return t; }
};

template <>
struct TypeInfo<jchar> {
	using Type = jchar;
	using ArrayType = jcharArray;

	using Result = Type;
	using Intermediate = Type;

	SPJNI_DEFINE_TYPE_METHODS(Char)

	static constexpr Result wrap(Type t, JNIEnv *env) { return t; }
};

template <>
struct TypeInfo<jshort> {
	using Type = jshort;
	using ArrayType = jshortArray;

	using Result = Type;
	using Intermediate = Type;

	SPJNI_DEFINE_TYPE_METHODS(Short)

	static constexpr Result wrap(Type t, JNIEnv *env) { return t; }
};

template <>
struct TypeInfo<jint> {
	using Type = jint;
	using ArrayType = jintArray;

	using Result = Type;
	using Intermediate = Type;

	SPJNI_DEFINE_TYPE_METHODS(Int)

	static constexpr Result wrap(Type t, JNIEnv *env) { return t; }
};

template <>
struct TypeInfo<jlong> {
	using Type = jlong;
	using ArrayType = jlongArray;

	using Result = Type;
	using Intermediate = Type;

	SPJNI_DEFINE_TYPE_METHODS(Long)

	static constexpr Result wrap(Type t, JNIEnv *env) { return t; }
};

template <>
struct TypeInfo<jfloat> {
	using Type = jfloat;
	using ArrayType = jfloatArray;

	using Result = Type;
	using Intermediate = Type;

	SPJNI_DEFINE_TYPE_METHODS(Float)

	static constexpr Result wrap(Type t, JNIEnv *env) { return t; }
};

template <>
struct TypeInfo<jdouble> {
	using Type = jdouble;
	using ArrayType = jdoubleArray;

	using Result = Type;
	using Intermediate = Type;

	SPJNI_DEFINE_TYPE_METHODS(Double)

	static constexpr Result wrap(Type t, JNIEnv *env) { return t; }
};

template <>
struct TypeInfo<void> {
	using Type = void;
	using ArrayType = void;

	using Result = Type;
	using Intermediate = Type;

	static constexpr auto Call = &_JNIEnv::CallVoidMethod;
	static constexpr auto CallStatic = &_JNIEnv::CallStaticVoidMethod;
	static constexpr auto CallNonvirtual = &_JNIEnv::CallNonvirtualVoidMethod;
};

#undef SPJNI_DEFINE_TYPE_METHODS

// jobject with named type
template <ClassNameSignature Value>
struct ObjectSignature { };

// jarray with subtype
template <typename T>
struct ArraySignature { };

template <typename Value>
struct SignatureArrayWrapper;

template <ClassNameSignature Value>
struct SignatureArrayWrapper<ObjectSignature<Value>> {
	using Type = jobjectArray;
};

template <>
struct SignatureArrayWrapper<jobject> {
	using Type = jobjectArray;
};

template <>
struct SignatureArrayWrapper<jstring> {
	using Type = jobjectArray;
};

template <>
struct SignatureArrayWrapper<jclass> {
	using Type = jobjectArray;
};

template <>
struct SignatureArrayWrapper<jboolean> {
	using Type = jbooleanArray;
};

template <>
struct SignatureArrayWrapper<jbyte> {
	using Type = jbyteArray;
};

template <>
struct SignatureArrayWrapper<jchar> {
	using Type = jcharArray;
};

template <>
struct SignatureArrayWrapper<jshort> {
	using Type = jshortArray;
};

template <>
struct SignatureArrayWrapper<jint> {
	using Type = jintArray;
};

template <>
struct SignatureArrayWrapper<jlong> {
	using Type = jlongArray;
};

template <>
struct SignatureArrayWrapper<jfloat> {
	using Type = jfloatArray;
};

template <>
struct SignatureArrayWrapper<jdouble> {
	using Type = jdoubleArray;
};

template <typename T>
struct SignatureWrapper;

template <ClassNameSignature Value>
struct SignatureWrapper<ObjectSignature<Value>> {
	static consteval size_t getSize() {
		// strip trailing zero, add wrapper symbols
		return Value.data.size() - 1 + 2;
	}

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'L';
		for (auto c : Value.data) { buf[offset++] = c == '.' ? '/' : c; }
		--offset;
		buf[offset++] = ';';
	}

	using Type = jobject;
	using Result = Local;
};

template <>
struct SignatureWrapper<jobject> {
	static consteval size_t getSize() { return "Ljava/lang/Object;"_len; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		SignatureWrapper<ObjectSignature<"java/lang/Object">>::append(buf, offset);
	}

	using Type = jobject;
	using Result = Local;
};

template <>
struct SignatureWrapper<jstring> {
	static consteval size_t getSize() { return "Ljava/lang/String;"_len; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		SignatureWrapper<ObjectSignature<"java/lang/String">>::append(buf, offset);
	}

	using Type = jstring;
	using Result = LocalString;
};

template <>
struct SignatureWrapper<jclass> {
	static consteval size_t getSize() { return "Ljava/lang/Class;"_len; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		SignatureWrapper<ObjectSignature<"java/lang/Class">>::append(buf, offset);
	}

	using Type = jclass;
	using Result = LocalClass;
};

template <>
struct SignatureWrapper<jboolean> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'Z';
	}

	using Type = jboolean;
	using Result = Type;
};

template <>
struct SignatureWrapper<jbyte> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'B';
	}

	using Type = jbyte;
	using Result = Type;
};

template <>
struct SignatureWrapper<jchar> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'C';
	}

	using Type = jchar;
	using Result = Type;
};

template <>
struct SignatureWrapper<jshort> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'S';
	}

	using Type = jshort;
	using Result = Type;
};

template <>
struct SignatureWrapper<jint> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'I';
	}

	using Type = jint;
	using Result = Type;
};

template <>
struct SignatureWrapper<jlong> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'J';
	}

	using Type = jlong;
	using Result = Type;
};

template <>
struct SignatureWrapper<jfloat> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'F';
	}

	using Type = jfloat;
	using Result = Type;
};

template <>
struct SignatureWrapper<jdouble> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'D';
	}

	using Type = jdouble;
	using Result = Type;
};

template <>
struct SignatureWrapper<void> {
	static consteval size_t getSize() { return 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = 'V';
	}

	using Type = void;
	using Result = void;
};

template <typename Value>
struct SignatureWrapper<ArraySignature<Value>> {
	static consteval size_t getSize() { return SignatureWrapper<Value>::getSize() + 1; }

	template <typename Buf>
	static consteval void append(Buf &buf, size_t &offset) {
		buf[offset++] = '[';
		SignatureWrapper<Value>::append(buf, offset);
	}

	using Type = typename SignatureArrayWrapper<Value>::Type;
	using Result = typename TypeInfo<Type>::Result;
};

SPRT_API void checkErrors(JNIEnv *);

} // namespace sprt::jni::detail

namespace sprt::jni {

// JNI signature deduction
// Useful to define JNI methods like Method<Signature>
// where signature is like jboolean(L<"full.object.name">, jint, A<jlong>)
template <typename T>
struct MethodSignature;

template <typename Result, typename... Args>
struct MethodSignature<Result(Args...)> {
	consteval static auto getSignature() {
		array< char,
				detail::SignatureWrapper<Result>::getSize()
						+ (detail::SignatureWrapper<Args>::getSize() + ... + 3)>
				buf{};

		size_t i = 0;
		buf[i++] = '(';
		(detail::SignatureWrapper<Args>::append(buf, i), ...);
		buf[i++] = ')';
		detail::SignatureWrapper<Result>::append(buf, i);
		buf[buf.size() - 1] = 0;

		return buf;
	}

	static constexpr auto Buffer = getSignature();
	static constexpr StringView Name = StringView(Buffer);
};

template <typename Type>
struct FieldSignature {
	consteval static auto getSignature() {
		array< char, detail::SignatureWrapper<Type>::getSize() + 1> buf{};

		size_t i = 0;
		detail::SignatureWrapper<Type>::append(buf, i);
		buf[buf.size() - 1] = 0;

		return buf;
	}

	static constexpr auto Buffer = getSignature();
	static constexpr StringView Name = StringView(Buffer);
};

} // namespace sprt::jni

#endif //  ANDROID

#endif // CORE_RUNTIME_INCLUDE_JNI_SPRUNTIMEJNITYPE_H_
