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
};

// What a node may carry: how many arguments, which properties, whether children.
struct NodeShape {
	size_t minArgs = 0;
	size_t maxArgs = 0;
	SpanView<StringView> props;
	bool children = false;
};

const StringView s_constProps[] = {"type", "doc"};
const StringView s_recordProps[] = {"doc"};
const StringView s_fieldProps[] = {"count", "doc"};

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

class Loader {
public:
	Loader(Schema &schema, Diagnostics &diag) : _schema(&schema), _diag(&diag) { }

	bool load(const KdlDocument &doc) {
		bool ok = true;
		const KdlNode *head = nullptr;
		for (auto &node : doc.nodes) {
			StringView name(node.name);
			if (name == "schema") {
				if (head) {
					error(node.location, "a description has one 'schema'");
					ok = false;
					continue;
				}
				head = &node;
				ok = loadHead(node) && ok;
			} else if (name == "const") {
				ok = loadConstant(node) && ok;
			} else if (name == "record") {
				ok = loadRecord(node) && ok;
			} else {
				error(node.location,
						mem_std::toString("unknown node '", name,
								"': a schema has 'schema', " "'const' and 'record'"));
				ok = false;
			}
		}
		if (!head) {
			error(kdl::Location{}, "no 'schema' node");
			return false;
		}
		return ok;
	}

protected:
	void error(kdl::Location loc, StringView message) { _diag->error(_schema->file, loc, message); }

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
			bool known = false;
			for (auto &it : shape.props) {
				if (it == StringView(prop.name)) {
					known = true;
				}
			}
			if (!known) {
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

	bool loadHead(const KdlNode &node) {
		if (!checkShape(node, NodeShape{1, 1, SpanView<StringView>(), true})
				|| !readName(node.arguments[0], _schema->name, "the schema's name")) {
			return false;
		}
		bool ok = true;
		for (auto &child : node.children) {
			StringView name(child.name);
			String *target = nullptr;
			if (name == "output") {
				target = &_schema->output;
			} else if (name == "guard") {
				target = &_schema->guard;
			} else if (name == "namespace") {
				target = &_schema->ns;
			} else if (name == "macro-prefix") {
				target = &_schema->macroPrefix;
			} else if (name == "copyright") {
				target = &_schema->copyright;
			} else if (name == "license") {
				target = &_schema->license;
			} else {
				error(child.location,
						mem_std::
								toString("unknown node '", name,
										"' of 'schema': output, guard, namespace, macro-prefix, "
										"copyright, " "license"));
				ok = false;
				continue;
			}
			if (!checkShape(child, NodeShape{1, 1})
					|| !readString(child.arguments[0], *target,
							mem_std::toString("'", name, "'"))) {
				ok = false;
			}
		}
		struct Required {
			StringView name;
			const String *value;
		} required[] = {
			{"output", &_schema->output},
			{"guard", &_schema->guard},
			{"namespace", &_schema->ns},
		};
		for (auto &it : required) {
			if (it.value->empty()) {
				error(node.location, mem_std::toString("'schema' has no '", it.name, "'"));
				ok = false;
			}
		}
		return ok;
	}

	bool loadConstant(const KdlNode &node) {
		Constant c;
		c.location = node.location;
		if (!checkShape(node, NodeShape{2, 2, s_constProps})
				|| !readName(node.arguments[0], c.name, "a constant's name")
				|| !readDoc(node, c.doc)) {
			return false;
		}
		auto typeValue = node.getPropertyValue("type");
		if (!typeValue) {
			error(node.location, mem_std::toString("the constant '", c.name, "' has no 'type'"));
			return false;
		}
		c.type = readType(*typeValue, "a constant's type");
		if (!c.type) {
			return false;
		}
		if (c.type->size != 4) {
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

		for (auto &it : _schema->constants) {
			if (it.name == c.name) {
				error(node.location,
						mem_std::toString("the constant '", c.name, "' is defined twice"));
				return false;
			}
		}
		_schema->items.emplace_back(Item{false, _schema->constants.size()});
		_schema->constants.emplace_back(sprt::move(c));
		return true;
	}

	bool loadRecord(const KdlNode &node) {
		Record r;
		r.location = node.location;
		if (!checkShape(node, NodeShape{1, 1, s_recordProps, true})
				|| !readName(node.arguments[0], r.name, "a record's name")
				|| !readDoc(node, r.doc)) {
			return false;
		}
		for (auto &it : _schema->records) {
			if (it.name == r.name) {
				error(node.location,
						mem_std::toString("the record '", r.name, "' is defined twice"));
				return false;
			}
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
			for (auto &it : r.fields) {
				if (it.name == f.name) {
					error(child.location,
							mem_std::toString("the field '", f.name, "' of '", r.name,
									"' is defined twice"));
					ok = false;
				}
			}
			r.fields.emplace_back(sprt::move(f));
		}
		if (!ok) {
			return false;
		}
		_schema->items.emplace_back(Item{true, _schema->records.size()});
		_schema->records.emplace_back(sprt::move(r));
		return true;
	}

	Schema *_schema = nullptr;
	Diagnostics *_diag = nullptr;
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

bool loadSchema(StringView file, Schema &schema, Diagnostics &diag) {
	schema.file = file.str<memory::StandardInterface>();
	if (!filesystem::exists(FileInfo{file})) {
		diag.error(file, kdl::Location{}, "no such file");
		return false;
	}
	auto text = filesystem::readTextFile<memory::StandardInterface>(FileInfo{file});
	KdlDocument doc;
	kdl::ParseError err;
	if (kdl::read(text, doc, &err) != Status::Ok) {
		diag.error(file, err.location, err.message);
		return false;
	}
	Loader loader(schema, diag);
	return loader.load(doc) && layoutSchema(schema, diag);
}

} // namespace stappler::xlkdl::gpu
