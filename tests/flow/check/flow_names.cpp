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

// The harness's words for the kernel's numbers: a code's kebab-case name, so that a check says
// `hasDiag(report, "unknown-op")` and a golden reads `unknown-op@2`. The kernel reports numbers; this
// table is the harness being a consumer of them.

#include "flow_check.h"

namespace STAPPLER_VERSIONIZED stappler::test {

StringView getDiagCodeName(flow::DiagCode c) {
	using flow::DiagCode;
	switch (c) {
	case DiagCode::OpNameEmpty: return StringView("op-name-empty");
	case DiagCode::OpPinInvalid: return StringView("op-pin-invalid");
	case DiagCode::OpPinDuplicate: return StringView("op-pin-duplicate");
	case DiagCode::OpPinLimit: return StringView("op-pin-limit");
	case DiagCode::OpDefaultInvalid: return StringView("op-default-invalid");
	case DiagCode::OpLocalInvalid: return StringView("op-local-invalid");
	case DiagCode::OpDuplicate: return StringView("op-duplicate");
	case DiagCode::OpPinRoleInvalid: return StringView("op-pin-role-invalid");
	case DiagCode::OpParallelInvalid: return StringView("op-parallel-invalid");
	case DiagCode::OpSettingInvalid: return StringView("op-setting-invalid");
	case DiagCode::AssetMalformed: return StringView("asset-malformed");
	case DiagCode::AssetUnknownKey: return StringView("asset-unknown-key");
	case DiagCode::NodeIdInvalid: return StringView("node-id-invalid");
	case DiagCode::NodeIdDuplicate: return StringView("node-id-duplicate");
	case DiagCode::GraphTooLarge: return StringView("graph-too-large");
	case DiagCode::UnknownOp: return StringView("unknown-op");
	case DiagCode::SignatureDrift: return StringView("signature-drift");
	case DiagCode::UnknownNode: return StringView("unknown-node");
	case DiagCode::UnknownPin: return StringView("unknown-pin");
	case DiagCode::PinKindMismatch: return StringView("pin-kind-mismatch");
	case DiagCode::PinArity: return StringView("pin-arity");
	case DiagCode::TypeMismatch: return StringView("type-mismatch");
	case DiagCode::ElementMismatch: return StringView("element-mismatch");
	case DiagCode::SubtypeMismatch: return StringView("subtype-mismatch");
	case DiagCode::DataCycle: return StringView("data-cycle");
	case DiagCode::MissingInput: return StringView("missing-input");
	case DiagCode::ConstantInvalid: return StringView("constant-invalid");
	case DiagCode::Unreachable: return StringView("unreachable");
	case DiagCode::SettingUnknown: return StringView("setting-unknown");
	case DiagCode::SettingInvalid: return StringView("setting-invalid");
	case DiagCode::ScopeConflict: return StringView("scope-conflict");
	case DiagCode::ScopeEscape: return StringView("scope-escape");
	case DiagCode::ParallelUnpaired: return StringView("parallel-unpaired");
	case DiagCode::ParallelEscape: return StringView("parallel-escape");
	case DiagCode::ParallelSerialOp: return StringView("parallel-serial-op");
	case DiagCode::ParallelForeignWrite: return StringView("parallel-foreign-write");
	case DiagCode::ParallelConflict: return StringView("parallel-conflict");
	case DiagCode::ParallelDynamicScene: return StringView("parallel-dynamic-scene");
	case DiagCode::ParallelNested: return StringView("parallel-nested");
	case DiagCode::ParallelReentry: return StringView("parallel-reentry");
	case DiagCode::ParallelGpuType: return StringView("parallel-gpu-type");
	case DiagCode::ParallelGpuOp: return StringView("parallel-gpu-op");
	case DiagCode::ParallelGpuShape: return StringView("parallel-gpu-shape");
	case DiagCode::ParallelEnumRange: return StringView("parallel-enum-range");
	case DiagCode::ParallelBranchFailed: return StringView("parallel-branch-failed");
	case DiagCode::ParallelPriorityDropped: return StringView("parallel-priority-dropped");
	case DiagCode::ParallelBlockTimeout: return StringView("parallel-block-timeout");
	case DiagCode::ParallelGpuGuard: return StringView("parallel-gpu-guard");
	case DiagCode::ParallelGpuLost: return StringView("parallel-gpu-lost");
	case DiagCode::NamedDynamic: return StringView("scene-named-dynamic");
	case DiagCode::NamedUnknown: return StringView("scene-named-unknown");
	case DiagCode::NamedArena: return StringView("scene-named-arena");
	case DiagCode::SceneUndeclared: return StringView("scene-undeclared");
	case DiagCode::SceneUnknownComponent: return StringView("scene-unknown-component");
	case DiagCode::SceneUnknownField: return StringView("scene-unknown-field");
	case DiagCode::SceneFieldType: return StringView("scene-field-type");
	case DiagCode::SceneNameDynamic: return StringView("scene-name-dynamic");
	case DiagCode::SceneUnused: return StringView("scene-unused");
	case DiagCode::EnumFamilyMissing: return StringView("enum-family-missing");
	case DiagCode::EnumFamilyDynamic: return StringView("enum-family-dynamic");
	case DiagCode::EnumFamilyUnknown: return StringView("enum-family-unknown");
	case DiagCode::EnumFamilyAlias: return StringView("enum-family-alias");
	case DiagCode::ExtUndeclared: return StringView("ext-undeclared");
	case DiagCode::ExtUnknown: return StringView("ext-unknown");
	case DiagCode::ExtInstanceMissing: return StringView("ext-instance-missing");
	case DiagCode::ExtParamMismatch: return StringView("ext-param-mismatch");
	case DiagCode::ExtNameDynamic: return StringView("ext-name-dynamic");
	case DiagCode::ExtUnused: return StringView("ext-unused");
	case DiagCode::EagerUnused: return StringView("eager-unused");
	case DiagCode::EagerSpeculative: return StringView("eager-speculative");
	case DiagCode::OpError: return StringView("op-error");
	case DiagCode::Deadlock: return StringView("deadlock");
	case DiagCode::ActivationLimit: return StringView("activation-limit");
	case DiagCode::StepLimit: return StringView("step-limit");
	case DiagCode::CodegenOpDrift: return StringView("codegen-op-drift");
	case DiagCode::CodegenSchemaDrift: return StringView("codegen-schema-drift");
	case DiagCode::CodegenLayoutDrift: return StringView("codegen-layout-drift");
	case DiagCode::CodegenAssetDrift: return StringView("codegen-asset-drift");
	case DiagCode::CodegenMalformed: return StringView("codegen-malformed");
	case DiagCode::CodegenQuantumUnsupported: return StringView("codegen-quantum-unsupported");
	case DiagCode::FunctionUnknown: return StringView("fn-unknown");
	case DiagCode::FunctionShadowed: return StringView("fn-shadowed");
	case DiagCode::FunctionInterfaceInvalid: return StringView("fn-interface-invalid");
	case DiagCode::FunctionBoundary: return StringView("fn-boundary");
	case DiagCode::FunctionInlineCycle: return StringView("fn-inline-cycle");
	case DiagCode::FunctionInlineMultiReturn: return StringView("fn-inline-multi-return");
	case DiagCode::FunctionDeclConflict: return StringView("fn-decl-conflict");
	case DiagCode::CallDepth: return StringView("call-depth");
	}
	return StringView("?");
}

StringView getDiagCodeName(flow::value::DiagCode c) {
	using flow::value::DiagCode;
	switch (c) {
	case DiagCode::FieldTypeNil: return StringView("field-type-nil");
	case DiagCode::FieldTypeUnknown: return StringView("field-type-unknown");
	case DiagCode::FieldElementUnexpected: return StringView("field-element-unexpected");
	case DiagCode::FieldElementMissing: return StringView("field-element-missing");
	case DiagCode::FieldElementUnknown: return StringView("field-element-unknown");
	case DiagCode::FieldElementTooDeep: return StringView("field-element-too-deep");
	case DiagCode::ComponentNameEmpty: return StringView("component-name-empty");
	case DiagCode::FieldNameEmpty: return StringView("field-name-empty");
	case DiagCode::FieldNameDuplicate: return StringView("field-name-duplicate");
	case DiagCode::FieldTypeInvalid: return StringView("field-type-invalid");
	case DiagCode::HostOffsetMismatch: return StringView("host-offset-mismatch");
	case DiagCode::HostSizeMismatch: return StringView("host-size-mismatch");
	case DiagCode::DefaultUnfit: return StringView("default-unfit");
	case DiagCode::ComponentDuplicate: return StringView("component-duplicate");
	case DiagCode::DeclarationDuplicate: return StringView("declaration-duplicate");
	case DiagCode::TypeNameUnknown: return StringView("type-name-unknown");
	case DiagCode::TypeNameIsReference: return StringView("type-name-is-reference");
	case DiagCode::AliasChainTooDeep: return StringView("alias-chain-too-deep");
	case DiagCode::AliasChainCycle: return StringView("alias-chain-cycle");
	case DiagCode::EnumNameEmpty: return StringView("enum-name-empty");
	case DiagCode::EnumMemberNameEmpty: return StringView("enum-member-name-empty");
	case DiagCode::EnumMemberDuplicate: return StringView("enum-member-duplicate");
	case DiagCode::AliasIncomplete: return StringView("alias-incomplete");
	case DiagCode::AliasSelf: return StringView("alias-self");
	}
	return StringView("?");
}

StringView getDiagCodeName(const mem_std::Value &entry) {
	auto code = uint16_t(entry.getInteger("code"));
	switch (flow::value::DiagDomain(entry.getInteger("domain"))) {
	case flow::value::DiagDomain::Value: return getDiagCodeName(flow::value::DiagCode(code));
	case flow::value::DiagDomain::Graph: return getDiagCodeName(flow::DiagCode(code));
	default: break;
	}
	return StringView("?");
}

StringView getDiagSeverityName(const mem_std::Value &entry) {
	switch (flow::value::DiagSeverity(entry.getInteger("severity"))) {
	case flow::value::DiagSeverity::Error: return StringView("error");
	case flow::value::DiagSeverity::Warning: return StringView("warning");
	case flow::value::DiagSeverity::Advice: return StringView("advice");
	}
	return StringView("?");
}

} // namespace stappler::test
