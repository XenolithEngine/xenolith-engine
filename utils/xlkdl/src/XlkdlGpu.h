/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef UTILS_XLKDL_SRC_XLKDLGPU_H_
#define UTILS_XLKDL_SRC_XLKDLGPU_H_

#include "XlkdlAbi.h"

// A GPU schema: the records C++ and GLSL both read, laid out by std430, and the constants they
// share. A description writes one header, valid as C++ through the types of sprt_glsl.h and as
// GLSL.
namespace STAPPLER_VERSIONIZED stappler::xlkdl::gpu {

// A type a field or a constant can have, spelled the same in GLSL and in sprt::glsl.
struct FieldType {
	StringView name;
	uint32_t size = 0;
	uint32_t align = 0; // the std430 base alignment

	// an array of it has the same stride in C++ as in std430
	bool arrays = true;

	// a constant of it is an integer literal, written with this suffix
	bool integer = false;
	StringView suffix;
};

const FieldType *findType(StringView name);

struct Field {
	String name;
	const FieldType *type = nullptr;
	uint32_t count = 0; // an array's length, 0 for a single value
	String doc;
	kdl::Location location;

	// set by the layout
	uint32_t offset = 0;
	bool padding = false;
};

struct Record {
	String name;
	String doc;
	kdl::Location location;
	Vector<Field> fields;

	// set by the layout
	uint32_t size = 0;
	uint32_t align = 0;
};

struct Constant {
	String name;
	const FieldType *type = nullptr;
	String value; // as C and GLSL spell it
	String doc;
	kdl::Location location;
};

// A definition in the order of the description.
struct Item {
	bool record = false;
	size_t index = 0;
};

struct Schema {
	String file;
	String name;
	String output; // from the description's directory
	String guard;
	String ns;
	String macroPrefix;
	String copyright;
	String license;
	Vector<Constant> constants;
	Vector<Record> records;
	Vector<Item> items;
};

// Reads a description and lays its records out; false, with the reasons in `diag`, when it is not
// a valid one.
bool loadSchema(StringView file, Schema &schema, Diagnostics &diag);

// The std430 offsets and sizes of the records, and the padding that ends each at its size.
bool layoutSchema(Schema &schema, Diagnostics &diag);

void emitHeader(const Schema &schema, String &out);

} // namespace stappler::xlkdl::gpu

#endif /* UTILS_XLKDL_SRC_XLKDLGPU_H_ */
