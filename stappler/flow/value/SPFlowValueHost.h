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

#ifndef STAPPLER_FLOW_VALUE_SPFLOWVALUEHOST_H_
#define STAPPLER_FLOW_VALUE_SPFLOWVALUEHOST_H_

#include "SPFlowValueVar.h"

// What a consumer of a scene says it needs from one, and what it asks of the host that holds the
// scene. A graph and a screen both state what they need; how the scene answers is the host's own
// business, and these interfaces are the whole of what a consumer may ask of it. An entity store
// implements them over its components; a host without one implements them over whatever it keeps.
namespace STAPPLER_VERSIONIZED stappler::flow::value {

// One field of a declared component, and the type the consumer expects it to have.
struct SceneFieldDecl {
	StringView name;
	VarType type = VarType::Nil;
};

// One component a consumer declares it needs from the scene, with the fields it names. Nothing here
// resolves a component, opens a registry or computes a layout: this is the statement, and checking
// it against a live registry is the caller's second gate - which is exactly why the first gate can
// be answered with no scene at all.
struct SceneDecl {
	StringView component;
	mem_std::Vector<SceneFieldDecl> fields;
};

// What a consumer declares it needs from the scene's extensions. `name` is the extension
// ("grid"), `id` the instance ("board") - and it is the id that is named, because a scene may
// carry two grids and a consumer has to say which. `params` is what the consumer believes that
// instance is; whether the live one agrees is asked of the instance.
struct ExtensionDecl {
	StringView name;
	StringView id;
	mem_std::Value params;
};

// One live extension instance. An operation that knows the extension casts it to the host's type.
class SP_PUBLIC ExtensionInstance {
public:
	virtual ~ExtensionInstance() = default;

	// The extension this is an instance of ("grid").
	virtual StringView getExtensionName() const = 0;

	// Whether this instance is what a declaration says it is.
	virtual bool matchExtensionParams(const mem_std::Value &declared) const = 0;
};

// The scene's extensions: which this process knows, instances by id, and the two moments of a
// journal an instance keeping something derived on the host has to hear about.
class SP_PUBLIC ExtensionHost {
public:
	virtual ~ExtensionHost() = default;

	virtual bool hasExtensionDefinition(StringView name) const = 0;
	virtual ExtensionInstance *findExtensionInstance(StringView id) const = 0;

	// Moves whenever an instance is created, dropped or replaced, so a binding can tell it is stale.
	virtual uint32_t getEpoch() const = 0;

	virtual void handleVersionClosed(Version) = 0;
	virtual void handleRollback(Version) = 0;
};

// The project's named entities: an id known before a store exists.
class SP_PUBLIC NamedHost {
public:
	virtual ~NamedHost() = default;

	// Ok with `out` set for an entity of the scene; ErrorNotFound for a name nothing declares;
	// ErrorNotSupported for an entity of another store, with that store's name in `store`.
	virtual Status findNamedEntity(StringView name, EntityId &out, StringView &store) const = 0;
};

} // namespace stappler::flow::value

#endif /* STAPPLER_FLOW_VALUE_SPFLOWVALUEHOST_H_ */
