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
// share - written to one header, valid as C++ through the types of sprt_glsl.h and as GLSL - and
// the interface of the shaders over them: for each shader a preamble it includes, with its
// extensions, push constants, buffer references, descriptors and varyings.
//
// A schema may import others: what they define is visible and is not written again; a preamble
// includes the headers of the records it uses.
namespace STAPPLER_VERSIONIZED stappler::xlkdl::gpu {

// A type a field, a varying or a constant can have, spelled the same in GLSL and in sprt::glsl.
struct FieldType {
	StringView name;
	uint32_t size = 0;
	uint32_t align = 0; // the std430 base alignment

	// an array of it has the same stride in C++ as in std430
	bool arrays = true;

	// a constant of it is an integer literal, written with this suffix
	bool integer = false;
	StringView suffix;

	// a texture of a material: a slot, written as a uint, of the texture array (1) or the layered
	// one (2)
	uint8_t texture = 0;

	// a packed field of a vertex, a uint in the record: what it holds unpacked, and how C++ packs
	// it and GLSL unpacks it
	StringView unpacked;
	StringView pack;
	StringView unpack;

	bool packed() const { return !unpacked.empty(); }

	// as GLSL and the record spell it
	StringView spelled() const { return texture || packed() ? StringView("uint") : name; }
};

const FieldType *findType(StringView name);

struct Field {
	String name;
	const FieldType *type = nullptr;
	uint32_t count = 0; // an array's length, 0 for a single value
	String fallback; // a builder's initial value, as C++ spells it; 0 when none is given
	String doc;
	kdl::Location location;

	// set by the layout
	uint32_t offset = 0;
	bool padding = false;
};

// A record, or the data of a schema a shader reads through a pointer of the push constants: a
// draw's material or its view's block, which may have textures and get a builder in C++.
enum class RecordKind : uint8_t {
	Record,
	Material,
	Block,
	Vertex, // a vertex of a mesh, which may have packed fields
};

struct Record {
	String name;
	String doc;
	kdl::Location location;
	Vector<Field> fields;

	// the header that defines it, as a preamble includes it; set for an imported one as well
	String header;
	bool imported = false;

	RecordKind kind = RecordKind::Record;

	// a material or a block
	bool isData() const { return kind == RecordKind::Material || kind == RecordKind::Block; }

	// a vertex's position, a vec3 or a vec4: the mesh's bounds
	String position;

	// set by the layout
	uint32_t size = 0;
	uint32_t align = 0;

	const Field *findField(StringView) const;
};

struct Constant {
	String name;
	String macro; // the name with its schema's prefix
	const FieldType *type = nullptr;
	String value; // as C and GLSL spell it
	String doc;
	kdl::Location location;
	bool imported = false;
};

// A buffer the push constants point to: an array of a record, or a single one, read through a
// function of its name.
struct Buffer {
	String name;
	String recordName;
	const Record *record = nullptr;
	String pointer; // the field of the push constants that holds its address
	bool single = false;
	kdl::Location location;
};

// An array of combined image samplers at set 0, in fragment shaders.
struct Samplers {
	String name;
	String type; // sampler2D or sampler2DArray
	uint32_t binding = 0;
	String count; // as GLSL spells it: a number, or a constant's macro
	String countName; // the constant, until linked
	kdl::Location location;
};

// The depth of what the pass drew before, as an input attachment of the fragment shaders of a
// translucent pipeline.
struct Depth {
	String name;
	uint32_t binding = 0;
	uint32_t index = 0;
	bool present = false;
	kdl::Location location;
};

// What a pass gives the shaders of its pipelines.
struct Interface {
	String name;
	String doc;
	String file; // the description that defines it
	kdl::Location location;
	bool imported = false;

	String pushRecordName;
	const Record *push = nullptr;
	String pushName;
	kdl::Location pushLocation;

	Vector<Buffer> buffers;
	Vector<Samplers> samplers;
	Depth depth;

	// the fields of the push constants that hold the addresses of a draw's material and of its
	// view's block
	String materialPointer;
	kdl::Location materialLocation;
	String blockPointer;
	kdl::Location blockLocation;

	// the field of the push constants that holds the address of a draw's mesh's vertexes
	String vertexPointer;
	kdl::Location vertexLocation;
};

struct Varying {
	String name;
	const FieldType *type = nullptr;
	bool flat = false;
	String doc;
	kdl::Location location;
};

// What a vertex shader hands the fragment shaders of its pipelines: an interface block both
// declare, its locations in the order of its members.
struct Varyings {
	String name;
	String doc;
	kdl::Location location;
	bool imported = false;
	Vector<Varying> fields;
};

enum class Stage : uint8_t {
	Vertex,
	Fragment,
};

// A shader file and what its preamble declares; the preamble is <name>.glsl.
struct Shader {
	String name;
	Stage stage = Stage::Vertex;
	String interfaceName;
	const Interface *interface = nullptr;
	String varyingsName;
	const Varyings *varyings = nullptr;
	String materialName;
	const Record *material = nullptr;
	String blockName;
	const Record *block = nullptr;
	String vertexName;
	const Record *vertex = nullptr;
	bool translucent = false;
	String file; // the description that defines it
	kdl::Location location;
	bool imported = false;
};

// A definition of the header in the order of the description.
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
	String preambles; // the directory of the preambles, from the description's directory

	// every file loaded, the root first
	Vector<String> files;

	Vector<Constant> constants;
	Vector<Record> records;
	Vector<Item> items;
	Vector<Interface> interfaces;
	Vector<Varyings> varyings;
	Vector<Shader> shaders;

	// whether the schema writes a header: it has records or constants of its own
	bool hasHeader() const;

	// whether it has materials or blocks, or vertexes, of its own, which its header makes builders
	// for
	bool hasData() const;
	bool hasVertexes() const;
};

// Reads a description and what it imports, links the names and lays the records out; false, with
// the reasons in `diag`, when it is not a valid one.
bool loadSchema(StringView file, Schema &schema, Diagnostics &diag);

// The std430 offsets and sizes of the records, and the padding that ends each at its size.
bool layoutSchema(Schema &schema, Diagnostics &diag);

void emitHeader(const Schema &schema, String &out);
void emitPreamble(const Schema &schema, const Shader &shader, String &out);

} // namespace stappler::xlkdl::gpu

#endif /* UTILS_XLKDL_SRC_XLKDLGPU_H_ */
