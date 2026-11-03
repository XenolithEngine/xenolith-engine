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

#include "XlkdlGpu.h"

namespace STAPPLER_VERSIONIZED stappler::xlkdl::gpu {

namespace {

// std430: a vec3 aligns as a vec4, but an array of them would step 16 bytes in GLSL and 12 in C++
const FieldType s_types[] = {
	FieldType{"float", 4, 4},
	FieldType{"int", 4, 4, true, true},
	FieldType{"uint", 4, 4, true, true, "u"},
	FieldType{"vec2", 8, 8},
	FieldType{"vec3", 12, 16, false},
	FieldType{"vec4", 16, 16},
	FieldType{"uvec2", 8, 8},
	FieldType{"uvec3", 12, 16, false},
	FieldType{"uvec4", 16, 16},
	FieldType{"color4", 16, 16},
	FieldType{"mat4", 64, 16},
	FieldType{"texture2d", 4, 4, false, false, "", 1},
	FieldType{"texture-array", 4, 4, false, false, "", 2},
};

// GLSL interpolates no integer: a varying of these types is flat
const StringView s_flatTypes[] = {"int", "uint", "uvec2", "uvec3", "uvec4"};

const StringView s_samplerTypes[] = {"sampler2D", "sampler2DArray"};

const StringView s_headNodes[] = {"output", "guard", "namespace", "macro-prefix", "copyright",
	"license", "preambles"};

const StringView s_interfaceNodes[] = {"push-constants", "buffer", "samplers", "depth", "material"};

const StringView s_nodes[] = {"schema", "import", "const", "record", "material", "interface",
	"varyings", "shader"};

// What a node may carry: how many arguments, which properties, whether children.
struct NodeShape {
	size_t minArgs = 0;
	size_t maxArgs = 0;
	SpanView<StringView> props;
	bool children = false;
};

const StringView s_constProps[] = {"type", "doc"};
const StringView s_docProps[] = {"doc"};
const StringView s_fieldProps[] = {"count", "doc"};
const StringView s_pushProps[] = {"name"};
const StringView s_bufferProps[] = {"pointer", "single"};
const StringView s_samplersProps[] = {"binding", "count"};
const StringView s_depthProps[] = {"binding", "index"};
const StringView s_varyingProps[] = {"flat", "doc"};
const StringView s_shaderProps[] = {"interface", "varyings", "material", "translucent", "doc"};
const StringView s_materialProps[] = {"pointer"};

bool isIdentifier(StringView name) {
	if (name.empty() || (!isalpha(name[0]) && name[0] != '_')) {
		return false;
	}
	for (auto c : name) {
		if (!isalnum(c) && c != '_') {
			return false;
		}
	}
	return true;
}

bool contains(SpanView<StringView> list, StringView name) {
	for (auto &it : list) {
		if (it == name) {
			return true;
		}
	}
	return false;
}

String joined(SpanView<StringView> list) {
	String out;
	for (auto &it : list) {
		out.append(out.empty() ? "" : ", ");
		out.append(it.data(), it.size());
	}
	return out;
}

// The schema node of a description: what its own definitions are written with.
struct Head {
	String name;
	String output;
	String guard;
	String ns;
	String macroPrefix;
	String copyright;
	String license;
	String preambles;
	kdl::Location location;
};

class Loader {
public:
	Loader(Schema &schema, Diagnostics &diag) : _schema(&schema), _diag(&diag) { }

	// A description and, first where they are written, what it imports. A file imported twice is
	// read once; a file that imports itself through others is an error at the import.
	bool loadFile(StringView file, bool imported, kdl::Location from = kdl::Location()) {
		auto path = filepath::reconstructPath<memory::StandardInterface>(file);
		for (auto &it : _stack) {
			if (it == path) {
				error(from,
						mem_std::toString("an import cycle: '", path, "' is being read already"));
				return false;
			}
		}
		for (auto &it : _schema->files) {
			if (it == path) {
				return true;
			}
		}
		if (!filesystem::exists(FileInfo{path})) {
			if (imported) {
				error(from, mem_std::toString("no such file: '", path, "'"));
			} else {
				_diag->error(path, kdl::Location{}, "no such file");
			}
			return false;
		}
		auto text = filesystem::readTextFile<memory::StandardInterface>(FileInfo{path});
		KdlDocument doc;
		kdl::ParseError err;
		if (kdl::read(text, doc, &err) != Status::Ok) {
			_diag->error(path, err.location, err.message);
			return false;
		}

		auto prevFile = sprt::move(_file);
		auto prevHead = sprt::move(_head);
		auto prevImported = _imported;
		auto prevOwnData = _ownData;
		auto prevOwnShaders = _ownShaders;
		_file = path;
		_head = Head();
		_imported = imported;
		_ownData = _ownShaders = 0;
		_stack.emplace_back(path);
		_schema->files.emplace_back(path);

		auto ok = load(doc);

		_stack.pop_back();
		_file = sprt::move(prevFile);
		_head = sprt::move(prevHead);
		_imported = prevImported;
		_ownData = prevOwnData;
		_ownShaders = prevOwnShaders;
		return ok;
	}

	// The names a definition refers to, once every file is in.
	bool link() {
		bool ok = true;
		for (auto &it : _schema->interfaces) { ok = linkInterface(it) && ok; }
		for (auto &it : _schema->shaders) { ok = linkShader(it) && ok; }
		return ok;
	}

protected:
	bool load(const KdlDocument &doc) {
		// the schema node first: the prefix and the header of the definitions come from it
		bool ok = true;
		const KdlNode *head = nullptr;
		for (auto &node : doc.nodes) {
			if (StringView(node.name) == "schema") {
				if (head) {
					error(node.location, "a description has one 'schema'");
					ok = false;
					continue;
				}
				head = &node;
				ok = loadHead(node) && ok;
			}
		}
		if (!head) {
			error(kdl::Location{}, "no 'schema' node");
			return false;
		}

		for (auto &node : doc.nodes) {
			StringView name(node.name);
			if (name == "schema") {
				continue;
			} else if (name == "import") {
				ok = loadImport(node) && ok;
			} else if (name == "const") {
				ok = loadConstant(node) && ok;
			} else if (name == "record") {
				ok = loadRecord(node, false) && ok;
			} else if (name == "material") {
				ok = loadRecord(node, true) && ok;
			} else if (name == "interface") {
				ok = loadInterface(node) && ok;
			} else if (name == "varyings") {
				ok = loadVaryings(node) && ok;
			} else if (name == "shader") {
				ok = loadShader(node) && ok;
			} else {
				error(node.location,
						mem_std::toString("unknown node '", name, "' (", joined(s_nodes), ")"));
				ok = false;
			}
		}
		if (!ok) {
			return false;
		}

		// what this file defines is written somewhere, so it needs a place for it
		if (_ownData > 0) {
			ok = require(_head.output, "output") && ok;
			if (!_imported) {
				ok = require(_head.guard, "guard") && ok;
				ok = require(_head.ns, "namespace") && ok;
			}
		}
		if (!_imported && _ownShaders > 0) {
			ok = require(_head.preambles, "preambles") && ok;
		}
		if (!_imported) {
			_schema->name = _head.name;
			_schema->output = _head.output;
			_schema->guard = _head.guard;
			_schema->ns = _head.ns;
			_schema->macroPrefix = _head.macroPrefix;
			_schema->copyright = _head.copyright;
			_schema->license = _head.license;
			_schema->preambles = _head.preambles;
		}
		return ok;
	}

	void error(kdl::Location loc, StringView message) { _diag->error(_file, loc, message); }

	bool require(const String &value, StringView name) {
		if (value.empty()) {
			error(_head.location, mem_std::toString("'schema' has no '", name, "'"));
			return false;
		}
		return true;
	}

	bool checkShape(const KdlNode &node, const NodeShape &shape) {
		bool ok = true;
		if (node.arguments.size() < shape.minArgs || node.arguments.size() > shape.maxArgs) {
			error(node.location,
					mem_std::toString("'", node.name, "' takes ",
							shape.minArgs == shape.maxArgs
									? mem_std::toString(shape.minArgs)
									: mem_std::toString(shape.minArgs, " to ", shape.maxArgs),
							" arguments, not ", node.arguments.size()));
			ok = false;
		}
		for (auto &prop : node.properties) {
			if (!contains(shape.props, StringView(prop.name))) {
				error(prop.location,
						mem_std::toString("unknown property '", prop.name, "' of '", node.name,
								"'"));
				ok = false;
			}
		}
		if (!shape.children && !node.children.empty()) {
			error(node.location, mem_std::toString("'", node.name, "' has no children"));
			ok = false;
		}
		return ok;
	}

	bool readString(const KdlValue &value, String &out, StringView what) {
		if (!value.isString()) {
			error(value.location, mem_std::toString(what, " must be a string"));
			return false;
		}
		out = value.getString().str<memory::StandardInterface>();
		return true;
	}

	bool readName(const KdlValue &value, String &out, StringView what) {
		if (!readString(value, out, what)) {
			return false;
		}
		if (!isIdentifier(out)) {
			error(value.location, mem_std::toString(what, " '", out, "' is not an identifier"));
			return false;
		}
		return true;
	}

	bool readDoc(const KdlNode &node, String &out) {
		auto value = node.getPropertyValue("doc");
		return !value || readString(*value, out, "'doc'");
	}

	bool readBool(const KdlNode &node, StringView key, bool &out) {
		auto value = node.getPropertyValue(key);
		if (!value) {
			return true;
		}
		if (!value->isBool()) {
			error(value->location, mem_std::toString("'", key, "' is #true or #false"));
			return false;
		}
		out = value->boolean;
		return true;
	}

	bool readUnsigned(const KdlNode &node, StringView key, uint32_t &out) {
		auto value = node.getPropertyValue(key);
		if (!value) {
			error(node.location, mem_std::toString("'", node.name, "' has no '", key, "'"));
			return false;
		}
		auto v = value->getUnsigned();
		if (!value->isInteger() || !v || v.get() > UINT16_MAX) {
			error(value->location, mem_std::toString("'", key, "' is a whole number"));
			return false;
		}
		out = uint32_t(v.get());
		return true;
	}

	const FieldType *readType(const KdlValue &value, StringView what) {
		String name;
		if (!readString(value, name, what)) {
			return nullptr;
		}
		auto type = findType(name);
		if (!type) {
			String known;
			for (auto &it : s_types) {
				known.append(known.empty() ? "" : ", ");
				known.append(it.name.data(), it.name.size());
			}
			error(value.location, mem_std::toString("unknown type '", name, "' (", known, ")"));
		}
		return type;
	}

	// Every name is one definition, whichever file it is in.
	bool checkUnique(StringView kind, StringView name, kdl::Location loc) {
		auto taken = [&](auto &list) {
			for (auto &it : list) {
				if (it.name == name) {
					return true;
				}
			}
			return false;
		};
		if (taken(_schema->constants) || taken(_schema->records) || taken(_schema->interfaces)
				|| taken(_schema->varyings) || taken(_schema->shaders)) {
			error(loc,
					mem_std::toString("the ", kind, " '", name, "' takes a name defined before"));
			return false;
		}
		return true;
	}

	bool loadHead(const KdlNode &node) {
		_head.location = node.location;
		if (!checkShape(node, NodeShape{1, 1, SpanView<StringView>(), true})
				|| !readName(node.arguments[0], _head.name, "the schema's name")) {
			return false;
		}
		bool ok = true;
		for (auto &child : node.children) {
			StringView name(child.name);
			String *target = nullptr;
			if (name == "output") {
				target = &_head.output;
			} else if (name == "guard") {
				target = &_head.guard;
			} else if (name == "namespace") {
				target = &_head.ns;
			} else if (name == "macro-prefix") {
				target = &_head.macroPrefix;
			} else if (name == "copyright") {
				target = &_head.copyright;
			} else if (name == "license") {
				target = &_head.license;
			} else if (name == "preambles") {
				target = &_head.preambles;
			} else {
				error(child.location,
						mem_std::toString("unknown node '", name, "' of 'schema' (",
								joined(s_headNodes), ")"));
				ok = false;
				continue;
			}
			if (!checkShape(child, NodeShape{1, 1})
					|| !readString(child.arguments[0], *target,
							mem_std::toString("'", name, "'"))) {
				ok = false;
			}
		}
		return ok;
	}

	bool loadImport(const KdlNode &node) {
		String path;
		if (!checkShape(node, NodeShape{1, 1})
				|| !readString(node.arguments[0], path, "an import")) {
			return false;
		}
		// from the importing file's directory, unless it is absolute
		auto full = filepath::isAbsolute(path) ? path
											   : filepath::merge<memory::StandardInterface>(
														 filepath::root(StringView(_file)), path);
		return loadFile(full, true, node.arguments[0].location);
	}

	bool loadConstant(const KdlNode &node) {
		Constant c;
		c.location = node.location;
		c.imported = _imported;
		if (!checkShape(node, NodeShape{2, 2, s_constProps})
				|| !readName(node.arguments[0], c.name, "a constant's name")
				|| !readDoc(node, c.doc)) {
			return false;
		}
		c.macro = mem_std::toString(_head.macroPrefix, c.name);
		auto typeValue = node.getPropertyValue("type");
		if (!typeValue) {
			error(node.location, mem_std::toString("the constant '", c.name, "' has no 'type'"));
			return false;
		}
		c.type = readType(*typeValue, "a constant's type");
		if (!c.type) {
			return false;
		}
		if (c.type->size != 4 || c.type->texture) {
			error(typeValue->location, "a constant is a float, an int or a uint");
			return false;
		}

		auto &value = node.arguments[1];
		if (c.type->integer) {
			bool fits = false;
			if (c.type->suffix.empty()) {
				auto v = value.getInteger();
				fits = value.isInteger() && v && v.get() >= INT32_MIN && v.get() <= INT32_MAX;
			} else {
				auto v = value.getUnsigned();
				fits = value.isInteger() && v && v.get() <= UINT32_MAX;
			}
			if (!fits) {
				error(value.location,
						mem_std::toString("the value of '", c.name, "' is not a 32-bit ",
								c.type->name));
				return false;
			}
		} else if (!value.isNumber() || value.type != kdl::ValueType::Float) {
			error(value.location,
					mem_std::toString("the value of '", c.name, "' is not a float literal"));
			return false;
		}
		// KDL lets digits be grouped by `_`, which neither C nor GLSL takes
		for (auto ch : StringView(value.literal)) {
			if (ch != '_') {
				c.value.push_back(ch);
			}
		}
		c.value.append(c.type->suffix.data(), c.type->suffix.size());

		if (!checkUnique("constant", c.name, node.location)) {
			return false;
		}
		++_ownData;
		_schema->items.emplace_back(Item{false, _schema->constants.size()});
		_schema->constants.emplace_back(sprt::move(c));
		return true;
	}

	bool loadRecord(const KdlNode &node, bool material) {
		Record r;
		r.location = node.location;
		r.imported = _imported;
		r.material = material;
		r.header =
				filepath::lastComponent(StringView(_head.output)).str<memory::StandardInterface>();
		if (!checkShape(node, NodeShape{1, 1, s_docProps, true})
				|| !readName(node.arguments[0], r.name,
						material ? "a material's name" : "a record's name")
				|| !readDoc(node, r.doc)
				|| !checkUnique(material ? "material" : "record", r.name, node.location)) {
			return false;
		}
		if (node.children.empty()) {
			error(node.location, mem_std::toString("the record '", r.name, "' has no fields"));
			return false;
		}

		bool ok = true;
		for (auto &child : node.children) {
			Field f;
			f.name = StringView(child.name).str<memory::StandardInterface>();
			f.location = child.location;
			if (!isIdentifier(f.name)) {
				error(child.location,
						mem_std::toString("the field '", f.name, "' is not an identifier"));
				ok = false;
				continue;
			}
			if (!checkShape(child, NodeShape{1, 1, s_fieldProps}) || !readDoc(child, f.doc)) {
				ok = false;
				continue;
			}
			f.type = readType(child.arguments[0], "a field's type");
			if (!f.type) {
				ok = false;
				continue;
			}
			if (f.type->texture && !material) {
				error(child.arguments[0].location,
						mem_std::toString("a texture is a field of a material, not of the record '",
								r.name, "'"));
				ok = false;
				continue;
			}
			if (f.type->texture && child.getPropertyValue("count")) {
				error(child.location, "a texture field is one texture, not an array");
				ok = false;
				continue;
			}
			if (auto count = child.getPropertyValue("count")) {
				auto v = count->getUnsigned();
				if (!count->isInteger() || !v || v.get() == 0 || v.get() > UINT16_MAX) {
					error(count->location, "'count' is a whole number from 1");
					ok = false;
					continue;
				}
				if (!f.type->arrays) {
					error(count->location,
							mem_std::toString("an array of ", f.type->name,
									" steps 16 bytes in std430 and ", f.type->size,
									" in C++: use the vec4 form"));
					ok = false;
					continue;
				}
				f.count = uint32_t(v.get());
			}
			if (r.findField(f.name)) {
				error(child.location,
						mem_std::toString("the field '", f.name, "' of '", r.name,
								"' is defined twice"));
				ok = false;
			}
			r.fields.emplace_back(sprt::move(f));
		}
		if (!ok) {
			return false;
		}
		++_ownData;
		_schema->items.emplace_back(Item{true, _schema->records.size()});
		_schema->records.emplace_back(sprt::move(r));
		return true;
	}

	bool loadInterface(const KdlNode &node) {
		Interface iface;
		iface.location = node.location;
		iface.file = _file;
		iface.imported = _imported;
		if (!checkShape(node, NodeShape{1, 1, s_docProps, true})
				|| !readName(node.arguments[0], iface.name, "an interface's name")
				|| !readDoc(node, iface.doc)
				|| !checkUnique("interface", iface.name, node.location)) {
			return false;
		}

		auto takenHere = [&](StringView name) {
			if (name == iface.pushName || (iface.depth.present && name == iface.depth.name)
					|| (!iface.materialPointer.empty() && name == "material")) {
				return true;
			}
			for (auto &it : iface.buffers) {
				if (it.name == name) {
					return true;
				}
			}
			for (auto &it : iface.samplers) {
				if (it.name == name) {
					return true;
				}
			}
			return false;
		};

		bool ok = true;
		for (auto &child : node.children) {
			StringView kind(child.name);
			if (kind == "push-constants") {
				if (!iface.pushRecordName.empty()) {
					error(child.location, "an interface has one 'push-constants'");
					ok = false;
					continue;
				}
				auto nameValue = child.getPropertyValue("name");
				if (!checkShape(child, NodeShape{1, 1, s_pushProps})
						|| !readName(child.arguments[0], iface.pushRecordName, "a record's name")
						|| !nameValue
						|| !readName(*nameValue, iface.pushName, "the push constants' name")) {
					if (!nameValue) {
						error(child.location, "'push-constants' has no 'name'");
					}
					ok = false;
					continue;
				}
				iface.pushLocation = child.location;
			} else if (kind == "buffer") {
				Buffer b;
				b.location = child.location;
				auto pointer = child.getPropertyValue("pointer");
				if (!checkShape(child, NodeShape{2, 2, s_bufferProps})
						|| !readName(child.arguments[0], b.name, "a buffer's name")
						|| !readName(child.arguments[1], b.recordName, "a record's name")
						|| !readBool(child, "single", b.single)) {
					ok = false;
					continue;
				}
				if (!pointer) {
					error(child.location,
							mem_std::toString("the buffer '", b.name, "' has no 'pointer'"));
					ok = false;
					continue;
				}
				if (!readName(*pointer, b.pointer, "'pointer'")) {
					ok = false;
					continue;
				}
				if (takenHere(b.name)) {
					error(child.location,
							mem_std::toString("'", b.name, "' is in the interface twice"));
					ok = false;
					continue;
				}
				iface.buffers.emplace_back(sprt::move(b));
			} else if (kind == "samplers") {
				Samplers s;
				s.location = child.location;
				if (!checkShape(child, NodeShape{2, 2, s_samplersProps})
						|| !readName(child.arguments[0], s.name, "an array's name")
						|| !readString(child.arguments[1], s.type, "a sampler type")
						|| !readUnsigned(child, "binding", s.binding)) {
					ok = false;
					continue;
				}
				if (!contains(s_samplerTypes, s.type)) {
					error(child.arguments[1].location,
							mem_std::toString("unknown sampler type '", s.type, "' (",
									joined(s_samplerTypes), ")"));
					ok = false;
					continue;
				}
				auto count = child.getPropertyValue("count");
				if (!count) {
					error(child.location, mem_std::toString("'", s.name, "' has no 'count'"));
					ok = false;
					continue;
				}
				if (count->isString()) {
					s.countName = count->getString().str<memory::StandardInterface>();
				} else if (auto v = count->getUnsigned();
						count->isInteger() && v && v.get() > 0 && v.get() <= UINT16_MAX) {
					s.count = mem_std::toString(v.get());
				} else {
					error(count->location, "'count' is a whole number from 1, or a constant");
					ok = false;
					continue;
				}
				if (takenHere(s.name)) {
					error(child.location,
							mem_std::toString("'", s.name, "' is in the interface twice"));
					ok = false;
					continue;
				}
				iface.samplers.emplace_back(sprt::move(s));
			} else if (kind == "depth") {
				if (iface.depth.present) {
					error(child.location, "an interface has one 'depth'");
					ok = false;
					continue;
				}
				Depth d;
				d.location = child.location;
				if (!checkShape(child, NodeShape{1, 1, s_depthProps})
						|| !readName(child.arguments[0], d.name, "the depth's name")
						|| !readUnsigned(child, "binding", d.binding)
						|| !readUnsigned(child, "index", d.index)) {
					ok = false;
					continue;
				}
				if (takenHere(d.name)) {
					error(child.location,
							mem_std::toString("'", d.name, "' is in the interface twice"));
					ok = false;
					continue;
				}
				d.present = true;
				iface.depth = sprt::move(d);
			} else if (kind == "material") {
				if (!iface.materialPointer.empty()) {
					error(child.location, "an interface has one 'material'");
					ok = false;
					continue;
				}
				auto pointer = child.getPropertyValue("pointer");
				if (!checkShape(child, NodeShape{0, 0, s_materialProps}) || !pointer
						|| !readName(*pointer, iface.materialPointer, "'pointer'")) {
					if (!pointer) {
						error(child.location, "'material' has no 'pointer'");
					}
					ok = false;
					continue;
				}
				iface.materialLocation = child.location;
			} else {
				error(child.location,
						mem_std::toString("unknown node '", kind, "' of 'interface' (",
								joined(s_interfaceNodes), ")"));
				ok = false;
			}
		}
		if (!ok) {
			return false;
		}
		_schema->interfaces.emplace_back(sprt::move(iface));
		return true;
	}

	bool loadVaryings(const KdlNode &node) {
		Varyings v;
		v.location = node.location;
		v.imported = _imported;
		if (!checkShape(node, NodeShape{1, 1, s_docProps, true})
				|| !readName(node.arguments[0], v.name, "the varyings' name")
				|| !readDoc(node, v.doc) || !checkUnique("varyings", v.name, node.location)) {
			return false;
		}
		if (node.children.empty()) {
			error(node.location, mem_std::toString("the varyings '", v.name, "' are empty"));
			return false;
		}
		bool ok = true;
		for (auto &child : node.children) {
			Varying f;
			f.name = StringView(child.name).str<memory::StandardInterface>();
			f.location = child.location;
			if (!isIdentifier(f.name)) {
				error(child.location,
						mem_std::toString("the varying '", f.name, "' is not an identifier"));
				ok = false;
				continue;
			}
			if (!checkShape(child, NodeShape{1, 1, s_varyingProps}) || !readDoc(child, f.doc)
					|| !readBool(child, "flat", f.flat)) {
				ok = false;
				continue;
			}
			f.type = readType(child.arguments[0], "a varying's type");
			if (!f.type) {
				ok = false;
				continue;
			}
			if (f.type->size > 16 || f.type->texture) {
				error(child.arguments[0].location,
						mem_std::toString("a varying is a scalar or a vector, not a ",
								f.type->name));
				ok = false;
				continue;
			}
			if (contains(s_flatTypes, f.type->name)) {
				f.flat = true;
			}
			for (auto &it : v.fields) {
				if (it.name == f.name) {
					error(child.location,
							mem_std::toString("the varying '", f.name, "' of '", v.name,
									"' is defined twice"));
					ok = false;
				}
			}
			v.fields.emplace_back(sprt::move(f));
		}
		if (!ok) {
			return false;
		}
		_schema->varyings.emplace_back(sprt::move(v));
		return true;
	}

	bool loadShader(const KdlNode &node) {
		Shader s;
		s.location = node.location;
		s.file = _file;
		s.imported = _imported;
		if (!checkShape(node, NodeShape{1, 1, s_shaderProps})
				|| !readString(node.arguments[0], s.name, "a shader's name")
				|| !readBool(node, "translucent", s.translucent)) {
			return false;
		}
		StringView name(s.name);
		auto dot = name.rfind('.');
		auto stem = dot == maxOf<size_t>() ? name : name.sub(0, dot);
		auto ext = dot == maxOf<size_t>() ? StringView() : name.sub(dot + 1);
		bool stemOk = !stem.empty();
		for (auto c : stem) { stemOk = stemOk && (isalnum(c) || c == '_' || c == '-'); }
		if (ext == "vert") {
			s.stage = Stage::Vertex;
		} else if (ext == "frag") {
			s.stage = Stage::Fragment;
		} else {
			stemOk = false;
		}
		if (!stemOk) {
			error(node.arguments[0].location,
					mem_std::toString("'", s.name,
							"' is not a shader's file name: a name, then .vert or .frag"));
			return false;
		}
		auto iface = node.getPropertyValue("interface");
		if (!iface) {
			error(node.location, mem_std::toString("the shader '", s.name, "' has no 'interface'"));
			return false;
		}
		if (!readName(*iface, s.interfaceName, "'interface'")) {
			return false;
		}
		if (auto varyings = node.getPropertyValue("varyings")) {
			if (!readName(*varyings, s.varyingsName, "'varyings'")) {
				return false;
			}
		}
		if (auto material = node.getPropertyValue("material")) {
			if (!readName(*material, s.materialName, "'material'")) {
				return false;
			}
		}
		if (s.translucent && s.stage != Stage::Fragment) {
			error(node.location, "only a fragment shader reads the depth a translucent one does");
			return false;
		}
		if (!checkUnique("shader", s.name, node.location)) {
			return false;
		}
		++_ownShaders;
		_schema->shaders.emplace_back(sprt::move(s));
		return true;
	}

	template <typename T>
	const T *find(const Vector<T> &list, StringView name) {
		for (auto &it : list) {
			if (it.name == name) {
				return &it;
			}
		}
		return nullptr;
	}

	bool linkInterface(Interface &iface) {
		bool ok = true;
		auto fail = [&](kdl::Location loc, StringView message) {
			_diag->error(iface.file, loc, message);
			ok = false;
		};
		if (!iface.pushRecordName.empty()) {
			iface.push = find(_schema->records, iface.pushRecordName);
			if (!iface.push) {
				fail(iface.pushLocation,
						mem_std::toString("no record '", iface.pushRecordName, "'"));
			}
		}
		for (auto &b : iface.buffers) {
			b.record = find(_schema->records, b.recordName);
			if (!b.record) {
				fail(b.location, mem_std::toString("no record '", b.recordName, "'"));
				continue;
			}
			for (auto &it : iface.buffers) {
				if (&it == &b) {
					break;
				}
				if (it.recordName == b.recordName) {
					fail(b.location,
							mem_std::toString("'", it.name, "' is a buffer of ", b.recordName,
									" already: its block would be declared twice"));
				}
			}
			if (!iface.push) {
				if (iface.pushRecordName.empty()) {
					fail(b.location,
							mem_std::toString("the buffer '", b.name,
									"' has no push constants to hold its pointer"));
				}
				continue;
			}
			auto field = iface.push->findField(b.pointer);
			if (!field || field->type->name != "uvec2" || field->count) {
				fail(b.location,
						mem_std::toString("the pointer of '", b.name, "' is not a uvec2 of ",
								iface.push->name));
			}
		}
		if (!iface.materialPointer.empty()) {
			auto field = iface.push ? iface.push->findField(iface.materialPointer) : nullptr;
			if (!field || field->type->name != "uvec2" || field->count) {
				fail(iface.materialLocation,
						mem_std::toString("the material's pointer '", iface.materialPointer,
								"' is not a uvec2 of the push constants"));
			}
		}
		for (auto &s : iface.samplers) {
			if (s.countName.empty()) {
				continue;
			}
			auto c = find(_schema->constants, s.countName);
			if (!c || !c->type->integer) {
				fail(s.location,
						mem_std::toString("the count of '", s.name,
								"' is not an integer constant: '", s.countName, "'"));
				continue;
			}
			s.count = c->macro;
		}
		return ok;
	}

	bool linkShader(Shader &s) {
		bool ok = true;
		s.interface = find(_schema->interfaces, s.interfaceName);
		if (!s.interface) {
			_diag->error(s.file, s.location,
					mem_std::toString("no interface '", s.interfaceName, "'"));
			ok = false;
		}
		if (!s.materialName.empty()) {
			s.material = find(_schema->records, s.materialName);
			if (!s.material || !s.material->material) {
				_diag->error(s.file, s.location,
						s.material ? mem_std::toString("'", s.materialName,
											 "' is a record, not a material")
								   : mem_std::toString("no material '", s.materialName, "'"));
				s.material = nullptr;
				ok = false;
			} else if (s.interface && s.interface->materialPointer.empty()) {
				_diag->error(s.file, s.location,
						mem_std::toString("the interface '", s.interface->name,
								"' gives its shaders no material"));
				ok = false;
			}
		}
		if (!s.varyingsName.empty()) {
			s.varyings = find(_schema->varyings, s.varyingsName);
			if (!s.varyings) {
				_diag->error(s.file, s.location,
						mem_std::toString("no varyings '", s.varyingsName, "'"));
				ok = false;
			}
		}
		return ok;
	}

	Schema *_schema = nullptr;
	Diagnostics *_diag = nullptr;
	String _file;
	Head _head;
	bool _imported = false;
	Vector<String> _stack;

	// the definitions of the file being read, not of what it imports
	size_t _ownData = 0;
	size_t _ownShaders = 0;
};

} // namespace

const FieldType *findType(StringView name) {
	for (auto &it : s_types) {
		if (it.name == name) {
			return &it;
		}
	}
	return nullptr;
}

const Field *Record::findField(StringView name) const {
	for (auto &it : fields) {
		if (it.name == name) {
			return &it;
		}
	}
	return nullptr;
}

bool Schema::hasHeader() const {
	for (auto &it : records) {
		if (!it.imported) {
			return true;
		}
	}
	for (auto &it : constants) {
		if (!it.imported) {
			return true;
		}
	}
	return false;
}

bool Schema::hasMaterials() const {
	for (auto &it : records) {
		if (!it.imported && it.material) {
			return true;
		}
	}
	return false;
}

bool loadSchema(StringView file, Schema &schema, Diagnostics &diag) {
	schema.file = file.str<memory::StandardInterface>();
	Loader loader(schema, diag);
	return loader.loadFile(file, false) && layoutSchema(schema, diag) && loader.link();
}

} // namespace stappler::xlkdl::gpu
