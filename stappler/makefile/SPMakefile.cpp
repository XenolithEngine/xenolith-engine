/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "SPMakefile.h"
#include "SPFilepath.h"
#include "SPFilesystem.h"
#include "SPMakefileBlock.h"
#include "SPMakefileError.h"
#include "SPMakefileStmt.h"
#include "SPMemInterface.h"
#include "SPMemory.h"

#include "SPMakefileError.cc"
#include "SPMakefileBlock.cc"
#include "SPMakefileRule.cc"
#include "SPMakefileStmt.cc"
#include "SPMakefileVariable.cc"

#include "xcode/SPPBXObject.cc"
#include "xcode/SPPBXBuildPhase.cc"
#include "xcode/SPPBXFile.cc"
#include "xcode/SPPBXProject.cc"
#include "xcode/SPPBXTarget.cc"
#include "xcode/SPXCodeProject.cc"

namespace STAPPLER_VERSIONIZED stappler::makefile {

bool Makefile::init() {
	_engine.init(_pool);
	return true;
}

void Makefile::setLogCallback(LogCallback cb, void *ref) {
	_logCallback = cb;
	_logCallbackRef = ref;
}

void Makefile::setIncludeCallback(IncludeCallback cb, void *ref) {
	_includeCallback = cb;
	_includeCallbackRef = ref;
}

void Makefile::setRootPath(StringView str) { _engine.setRootPath(str); }

bool Makefile::include(StringView name, StringView data, bool copyData, ErrorReporter *e) {
	return perform([&] {
		auto fname = filepath::lastComponent(name);
		ErrorReporter err(e);
		err.outer = e;
		err.filename = fname.pdup(_pool);
		err.callback = _logCallback;
		err.ref = _logCallbackRef;

		Block *rootBlock = new (std::nothrow) Block;
		rootBlock->loc = err;
		rootBlock->identifier = err.filename;
		rootBlock->content = name.pdup(_pool);

		_engine.pushBlock(rootBlock);

		auto ret = processMakefileContent(copyData ? data.pdup(_pool) : data, err);

		if (_engine.getCurrentBlock() != rootBlock) {
			err.reportError("block was not closed", nullptr, _engine.getCurrentBlock());
		}

		_engine.popBlock();
		return ret;
	});
}

bool Makefile::include(const FileInfo &info, ErrorReporter *err, bool optional) {
	auto path = filesystem::findPath<Interface>(info, filesystem::Access::Read);
	if (path.empty()) {
		log::source().error("Makefile", "Fail to open ", info);
		return false;
	}
	auto f = filesystem::openForReading(FileInfo{path});
	if (f) {
		auto fsize = f.size();
		auto buf = (uint8_t *)memory::pool::palloc(_pool, fsize);

		f.seek(0, io::Seek::Set);
		f.read(buf, fsize);
		f.close();

		return include(path, BytesView(buf, fsize).toStringView(), false, err);
	} else {
		if (!optional) {
			log::source().error("Makefile", "Fail to open ", info);
		}
		return false;
	}
}

bool Makefile::includeFileByPath(StringView file, ErrorReporter *err, bool optional) {
	if (_includeCallback) {
		bool loaded = false;
		_includeCallback(_includeCallbackRef, file, [&](StringView data) {
			if (!data.empty()) {
				if (include(file, data, true, err)) {
					loaded = true;
				}
			}
		});
		return loaded;
	} else {
		return include(FileInfo{file}, err, optional);
	}
}

const Variable *Makefile::assignSimpleVariable(StringView name, Origin o, StringView val,
		bool multiline) {
	ErrorReporter err(nullptr);
	err.filename = StringView("<lib>");
	err.line = val;
	err.callback = _logCallback;
	err.ref = _logCallbackRef;

	return assignSimpleVariable(name, o, val, err, multiline);
}
const Variable *Makefile::assignRecursiveVariable(StringView name, Origin o, StringView val,
		bool multiline) {
	ErrorReporter err(nullptr);
	err.filename = StringView("<lib>");
	err.line = val;
	err.callback = _logCallback;
	err.ref = _logCallbackRef;

	return assignRecursiveVariable(name, o, val, err, multiline);
}
const Variable *Makefile::appendToVariable(StringView name, Origin o, StringView val,
		bool multiline) {
	ErrorReporter err(nullptr);
	err.filename = StringView("<lib>");
	err.line = val;
	err.callback = _logCallback;
	err.ref = _logCallbackRef;

	return appendToVariable(name, o, val, err, multiline);
}

const Variable *Makefile::assignSimpleVariable(StringView identifier, Origin varOrigin,
		StringView str, ErrorReporter &err, bool multiline) {
	if (identifier.empty()) {
		err.reportError("Variable name resolved to empty string");
		return nullptr;
	}

	auto stmt = Stmt::readScoped(str, StmtType::WordList,
			multiline ? ReadContext::Multiline : ReadContext::LineEnd, err);
	if (!stmt) {
		return _engine.set(identifier, varOrigin, StringView());
	}

	auto val = _engine.resolve(stmt, err);

	return _engine.set(identifier, varOrigin, val);
}

const Variable *Makefile::assignRecursiveVariable(StringView identifier, Origin varOrigin,
		StringView str, ErrorReporter &err, bool multiline) {

	if (identifier.empty()) {
		err.reportError("Variable name resolved to empty string");
		return nullptr;
	}

	if (str.empty()) {
		return _engine.set(identifier, varOrigin, StringView());
	} else {
		auto stmt = Stmt::readScoped(str, StmtType::WordList,
				multiline ? ReadContext::Multiline : ReadContext::LineEnd, err);
		if (!stmt) {
			return nullptr;
		}

		return _engine.set(identifier, varOrigin, stmt);
	}
}

const Variable *Makefile::appendToVariable(StringView identifier, Origin varOrigin, StringView str,
		ErrorReporter &err, bool multiline) {
	if (identifier.empty()) {
		err.reportError("Variable name resolved to empty string");
		return nullptr;
	}

	auto stmt = Stmt::readScoped(str, StmtType::WordList,
			multiline ? ReadContext::Multiline : ReadContext::LineEnd, err);
	if (!stmt) {
		return nullptr;
	}

	auto v = _engine.get(identifier);
	if (!v) {
		err.reportWarning(toString("Variable '", identifier, "' is not defined for '+='"));
		return _engine.set(identifier, varOrigin, stmt);
	} else {
		if (v->type == Variable::Type::String) {
			auto val = _engine.resolve(stmt, err);
			if (!val.empty()) {
				const_cast<Variable *>(v)->str = StringView(toString(v->str, " ", val)).pdup();
			}
		} else if (v->type == Variable::Type::Stmt) {
			switch (v->stmt->type) {
			case StmtType::Word:
				const_cast<Variable *>(v)->stmt =
						new (std::nothrow) Stmt(err, StmtType::WordList, v->stmt);
				const_cast<Variable *>(v)->stmt->add(new (std::nothrow) StmtValue(stmt));
				break;
			case StmtType::WordList:
				const_cast<Variable *>(v)->stmt->add(new (std::nothrow) StmtValue(stmt));
				break;
			default: err.reportError("Invalid variable type for '+='"); break;
			}
		}
		return v;
	}
}

const Variable *Makefile::getVariable(StringView str) const { return _engine.getIfDefined(str); }

bool Makefile::eval(const Callback<void(StringView)> &out, StringView name, StringView content) {
	_engine.setCustomOutput(&out);
	auto ret = include(name, content);
	_engine.setCustomOutput(nullptr);
	return ret;
}

Target *Makefile::addTarget(StringView name) {
	auto it = _targets.find(name);
	if (it == _targets.end()) {
		it = _targets.emplace(name, new (_pool) Target(name)).first;
	}
	return it->second;
}

bool Makefile::addTargetPrerequisite(SpanView<Target *> targets, StringView decl,
		ErrorReporter &err) {
	Stmt::skipWhitespace(decl);

	Stmt *prerequisiteListStmt = nullptr;
	Stmt *orderOnlyListStmt = nullptr;
	Stmt *trailingRecipeStmt = nullptr;

	prerequisiteListStmt =
			Stmt::readScoped(decl, StmtType::WordList, ReadContext::PrerequisiteList, err);

	if (decl.is('|')) {
		++decl;
		orderOnlyListStmt =
				Stmt::readScoped(decl, StmtType::WordList, ReadContext::OrderOnlyList, err);
	}

	if (decl.is(';')) {
		++decl;
		trailingRecipeStmt =
				Stmt::readScoped(decl, StmtType::WordList, ReadContext::TrailingRecipe, err);
	}

	if (!prerequisiteListStmt && !orderOnlyListStmt && !trailingRecipeStmt) {
		err.reportError("Fail to read prerequisite line");
		return false;
	}

	if (prerequisiteListStmt) {
		auto prerequisiteList = _engine.resolve(prerequisiteListStmt, err);
		prerequisiteList.split<StringView::WhiteSpace>([&](StringView s) {
			for (auto &it : targets) { it->addPrerequisite(s); }
		});
	}

	if (orderOnlyListStmt) {
		auto OrderOnlyList = _engine.resolve(orderOnlyListStmt, err);
		OrderOnlyList.split<StringView::WhiteSpace>([&](StringView s) {
			for (auto &it : targets) { it->addOrderOnly(s); }
		});
	}

	if (trailingRecipeStmt) {
		for (auto &it : targets) { it->addRule(trailingRecipeStmt); }
	}

	return true;
}

bool Makefile::undefineVariable(StringView identifier, Origin varOrigin, ErrorReporter &err) {
	if (identifier.empty()) {
		err.reportError("Variable name resolved to empty string");
		return false;
	}

	auto v = _engine.get(identifier);
	if (!v) {
		err.reportWarning(toString("Variable '", identifier, "' was not defines"));
		return true;
	} else {
		if (!v->isOverridableBy(varOrigin)) {
			err.reportWarning(toString("Variable '", identifier, "' can not be undefined from '",
					varOrigin, "' (suggest `override undefine`)"));
			return false;
		} else {
			return _engine.clear(identifier, varOrigin);
		}
	}
}

bool Makefile::processMakefileContent(StringView str, ErrorReporter &err) {
	while (!str.empty()) {
		err.lineno += err.lineSize;
		err.lineSize = 1;

		auto line = Stmt::readLine(str, err);
		if (_engine.getCurrentBlock()->type != Keyword::Define && line.is('#')) {
			// skip (comment)
		} else if (line.is('\t') && !_currentTargets.empty()) {
			++line;

			auto stmt =
					Stmt::readScoped(line, StmtType::WordList, ReadContext::TrailingRecipe, err);
			if (!stmt) {
				err.setPos(line);
				err.reportError("Invalid recipe format");
				return false;
			}

			for (auto &it : _currentTargets) {
				if (it) {
					it->addRule(stmt);
				}
			}
		} else {
			// definition
			Stmt::skipWhitespace(line);
			if (!line.empty()) {
				_currentTargets.clear();
			}
			if (!processMakefileLine(line, err)) {
				return false;
			}
		}
		if (str.is("\r\n")) {
			str += 2;
		} else if (str.is("\r") || str.is("\n")) {
			str += 1;
		}
	}
	return true;
}

bool Makefile::processMakefileLine(StringView str, ErrorReporter &err) {
	err.line = str;
	err.pos = 0;

	Stmt::skipWhitespace(str);

	err.setPos(str);

	auto tmp = str;
	auto firstWord = tmp.readUntil<StringView::WhiteSpace>();
	if (tmp.is<StringView::Chars<'\r', '\n'>>() && firstWord.is<'\\'>()) {
		firstWord = firstWord.sub(0, firstWord.size() - 1);
	}

	Origin varOrigin = Origin::File;

	if (firstWord == "override") {
		varOrigin = Origin::Override;
		Stmt::skipWhitespace(tmp);
		str = tmp;
		firstWord = tmp.readUntil<StringView::WhiteSpace>();
		if (tmp.is<StringView::Chars<'\r', '\n'>>() && firstWord.is<'\\'>()) {
			firstWord = firstWord.sub(0, firstWord.size() - 1);
		}
	}

	auto keyword = Stmt::getKeyword(firstWord);

	if (_engine.getCurrentBlock()->type == Keyword::Define) {
		switch (keyword) {
		case Keyword::Endef: return processEndefLine(tmp, err); break;
		default: return processDefineContentLine(str, _engine.getCurrentBlock(), err); break;
		}
	}

	switch (keyword) {
	case Keyword::Include: return processIncludeLine(tmp, err, false); break;
	case Keyword::IncludeOptional: return processIncludeLine(tmp, err, true); break;
	case Keyword::Define: return processDefineLine(tmp, err); break;
	case Keyword::Endef: return processEndefLine(tmp, err); break;
	case Keyword::Override: return false; break;
	case Keyword::Ifdef: return processIfdefLine(tmp, false, err, nullptr); break;
	case Keyword::Ifndef: return processIfdefLine(tmp, true, err, nullptr); break;
	case Keyword::Ifeq: return processIfeqLine(tmp, false, err, nullptr); break;
	case Keyword::Ifneq: return processIfeqLine(tmp, true, err, nullptr); break;
	case Keyword::Else: return processElseLine(tmp, err); break;
	case Keyword::Endif: return processEndifLine(tmp, err); break;
	case Keyword::Undefine: return processUndefineLine(tmp, varOrigin, err); break;
	case Keyword::None: return processSimpleLine(str, varOrigin, err); break;
	}
	return false;
}

bool Makefile::processIfdefLine(StringView &str, bool negative, ErrorReporter &err,
		Block *original) {
	Stmt::skipWhitespace(str);

	if (str.empty()) {
		err.reportError("Expected variable name");
		return false;
	}

	bool isDefined = false;

	if (!_engine.getCurrentBlock()->enabled || perform_temporary([&]() {
		auto stmt = Stmt::readScoped(str, StmtType::WordList, ReadContext::LineStart, err);
		if (!stmt) {
			return err.nerrors == 0;
		}

		auto identifier = _engine.resolve(stmt, err);
		auto v = _engine.get(identifier);
		if (v) {
			isDefined = true;
		}
		return true;
	})) {
		auto block = new (_pool) Block;
		block->prev = original;
		block->loc = err;
		block->type = negative ? Keyword::Ifndef : Keyword::Ifdef;
		if (_engine.getCurrentBlock()->enabled && (!original || original->canEnableNext())) {
			if (isDefined && !negative) {
				block->enabled = true;
			} else if (!isDefined && negative) {
				block->enabled = true;
			} else {
				block->enabled = false;
			}
		} else {
			block->enabled = false;
		}
		_engine.pushBlock(block);
		return true;
	}

	return false;
}

bool Makefile::processIfeqLine(StringView &str, bool negative, ErrorReporter &err,
		Block *original) {
	Stmt::skipWhitespace(str);

	bool isEqual = false;

	auto readQuoted = [&]() -> StmtValue * {
		auto stmt = Stmt::readScoped(str, StmtType::WordList,
				str.is('"') ? ReadContext::ConditionalDoubleQuoted : ReadContext::ConditionalQuoted,
				err);
		if (!stmt) {
			return nullptr;
		}

		return stmt->value;
	};

	if (_engine.getCurrentBlock()->enabled && !perform_temporary([&]() {
		StmtValue *first = nullptr;
		StmtValue *second = nullptr;

		err.setPos(str);

		if (str.is('(')) {
			auto tmp = str;
			auto stmt = Stmt::readScoped(str, StmtType::WordList, ReadContext::Expansion, err);
			if (!stmt) {
				err.reportError("Invalid comparation statement");
				return false;
			}

			tmp = StringView(tmp.data(), str.data() - tmp.data());
			if (stmt->type != StmtType::ArgumentList && !tmp.ends_with(",)")) {
				err.reportError("Invalid comparation statement");
				return false;
			}

			first = stmt->value;
			if (stmt->type == StmtType::ArgumentList) {
				second = stmt->value->next;
			}
		} else if (str.is('"') || str.is('\'')) {
			first = readQuoted();
			if (!first) {
				err.reportError("Invalid comparation statement");
				return false;
			}

			Stmt::skipWhitespace(str);
			if (str.is('"') || str.is('\'')) {
				second = readQuoted();
			}
		} else {
			err.reportError("Invalid comparation statement");
			return false;
		}

		if (!first || (second && second->next)) {
			err.reportError("Invalid comparation statement");
			return false;
		}

		auto firstData = _engine.resolve(first, false, err);
		firstData.trimChars<StringView::WhiteSpace>();

		auto secondData = second ? _engine.resolve(second, false, err) : StringView();
		secondData.trimChars<StringView::WhiteSpace>();

		if (firstData == secondData) {
			isEqual = true;
		}
		return true;
	})) {
		return false;
	}

	auto block = new (_pool) Block;
	block->prev = original;
	block->loc = err;
	block->type = negative ? Keyword::Ifndef : Keyword::Ifdef;
	if (_engine.getCurrentBlock()->enabled && (!original || original->canEnableNext())) {
		if (isEqual && !negative) {
			block->enabled = true;
		} else if (!isEqual && negative) {
			block->enabled = true;
		} else {
			block->enabled = false;
		}
	} else {
		block->enabled = false;
	}
	_engine.pushBlock(block);
	return true;
}

bool Makefile::processElseLine(StringView &str, ErrorReporter &err) {
	auto condBlock = _engine.getCurrentBlock();
	switch (condBlock->type) {
	case Keyword::Ifdef:
	case Keyword::Ifndef:
	case Keyword::Ifeq:
	case Keyword::Ifneq: break;
	default:
		err.reportError("Fail to close conditional block, other block was not closed:", nullptr,
				condBlock);
		return false;
		break;
	}

	Stmt::skipWhitespace(str);

	auto tmp = str;
	auto firstWord = tmp.readUntil<StringView::WhiteSpace>();
	if (tmp.is<StringView::Chars<'\r', '\n'>>() && firstWord.is<'\\'>()) {
		firstWord = firstWord.sub(0, firstWord.size() - 1);
	}

	auto keyword = Stmt::getKeyword(firstWord);
	switch (keyword) {
	case Keyword::Ifdef:
		_engine.popBlock();
		return processIfdefLine(tmp, false, err, condBlock);
		break;
	case Keyword::Ifndef:
		_engine.popBlock();
		return processIfdefLine(tmp, true, err, condBlock);
		break;
	case Keyword::Ifeq:
		_engine.popBlock();
		return processIfeqLine(tmp, false, err, condBlock);
		break;
	case Keyword::Ifneq:
		_engine.popBlock();
		return processIfeqLine(tmp, true, err, condBlock);
		break;
	case Keyword::None:
		if (!str.empty() && !str.is('#')) {
			err.setPos(str);
			err.reportError("Unexpected 'else' statement");
			return false;
		} else {
			condBlock->enabled = condBlock->canEnableNext();
			return true;
		}
		break;
	default:
		err.setPos(str);
		err.reportError("Unexpected 'else' statement");
		return false;
		break;
	}

	return false;
}

bool Makefile::processEndifLine(StringView &str, ErrorReporter &err) {
	switch (_engine.getCurrentBlock()->type) {
	case Keyword::Ifdef:
	case Keyword::Ifndef:
	case Keyword::Ifeq:
	case Keyword::Ifneq:
	case Keyword::Else:
		_engine.popBlock();
		return true;
		break;
	default:
		err.reportError("Fail to close conditional block, other block was not closed:", nullptr,
				_engine.getCurrentBlock());
		break;
	}
	return false;
}

bool Makefile::processDefineLine(StringView &str, ErrorReporter &err) {
	Origin varOrigin = Origin::File;

	Stmt::skipWhitespace(str);

	if (str.starts_with("override") && Stmt::isWhitespace(str.sub("override"_len, 2))) {
		varOrigin = Origin::Override;
		str += "override"_len;
		Stmt::skipWhitespace(str);
	}

	auto stmt = Stmt::readScoped(str, StmtType::WordList, ReadContext::LineStart, err);
	if (!stmt) {
		return err.nerrors == 0;
	}

	auto identifier = _engine.resolve(stmt, err);
	if (identifier.empty()) {
		err.reportError("Variable name resolved to empty string");
		return false;
	}

	auto op = Stmt::getOperator(str, false);

	str += op.size();

	Stmt::skipWhitespace(str);
	if (!str.empty()) {
		err.reportError("Unexpected define format");
		return false;
	}

	auto block = new (std::nothrow) Block;
	block->loc = err;
	block->type = Keyword::Define;
	block->origin = varOrigin;
	block->identifier = identifier;
	block->enabled = _engine.getCurrentBlock()->enabled;
	block->op = op;

	_engine.pushBlock(block);

	return true;
}

bool Makefile::processDefineContentLine(StringView &str, Block *block, ErrorReporter &err) {
	if (!block->content.data()) {
		block->content = str;
	} else {
		block->content = StringView(block->content.data(),
				(str.data() + str.size()) - block->content.data());
	}
	return true;
}

bool Makefile::processEndefLine(StringView &str, ErrorReporter &err) {
	if (_engine.getCurrentBlock()->type != Keyword::Define) {
		err.reportError("No define for endef found", nullptr, _engine.getCurrentBlock());
		return false;
	}

	auto defBlock = _engine.getCurrentBlock();
	_engine.popBlock();

	if (!defBlock->enabled) {
		return true;
	}

	ErrorReporter err2 = err;
	err2.lineno = defBlock->loc.lineno + 1;
	err2.pos = 0;
	err2.line = defBlock->content;

	auto op = defBlock->op;
	if (op == ":=" || op == "::=") {
		assignSimpleVariable(defBlock->identifier, defBlock->origin, defBlock->content, err2, true);
	} else if (op == "=" || op.empty()) {
		assignRecursiveVariable(defBlock->identifier, defBlock->origin, defBlock->content, err2,
				true);
	} else if (op == "+=") {
		appendToVariable(defBlock->identifier, defBlock->origin, defBlock->content, err2, true);
	} else if (op == "?=") {
		auto v = _engine.get(defBlock->identifier);
		if (!v) {
			assignRecursiveVariable(defBlock->identifier, defBlock->origin, defBlock->content, err2,
					true);
		}
	} else if (op == ":::=") {
		// todo

		assignSimpleVariable(defBlock->identifier, defBlock->origin, defBlock->content, err2, true);
	}

	return true;
}

bool Makefile::processUndefineLine(StringView &str, Origin varOrigin, ErrorReporter &err) {
	if (!_engine.getCurrentBlock()->enabled) {
		return true;
	}

	Stmt::skipWhitespace(str);

	auto stmt = Stmt::readScoped(str, StmtType::WordList, ReadContext::LineEnd, err);
	if (!stmt) {
		return err.nerrors == 0;
	}

	auto identifier = _engine.resolve(stmt, err);

	return undefineVariable(identifier, varOrigin, err);
}

bool Makefile::processSimpleLine(StringView &str, Origin varOrigin, ErrorReporter &err) {
	if (!_engine.getCurrentBlock()->enabled) {
		return true;
	}

	Stmt::skipWhitespace(str);

	if (str.starts_with("override") && Stmt::isWhitespace(str.sub("override"_len, 2))) {
		varOrigin = Origin::Override;
		str += "override"_len;
		Stmt::skipWhitespace(str);
	}

	auto stmt = Stmt::readScoped(str, StmtType::WordList, ReadContext::LineStart, err);
	if (!stmt) {
		return err.nerrors == 0;
	}

	auto identifier = _engine.resolve(stmt, err);

	stmt = nullptr;

	auto op = Stmt::getOperator(str, true);

	if (op == ":=" || op == "::=") {
		str += op.size();
		Stmt::skipWhitespace(str);

		assignSimpleVariable(identifier, varOrigin, str, err);
	} else if (op == "=") {
		str += op.size();
		Stmt::skipWhitespace(str);

		assignRecursiveVariable(identifier, varOrigin, str, err);
	} else if (op == "+=") {
		str += op.size();
		Stmt::skipWhitespace(str);

		appendToVariable(identifier, varOrigin, str, err);
	} else if (op == "?=") {
		str += op.size();
		Stmt::skipWhitespace(str);

		if (identifier.empty()) {
			err.reportError("Variable name resolved to empty string");
			return err.nerrors == 0;
		}

		auto v = _engine.get(identifier);
		if (!v) {
			assignRecursiveVariable(identifier, varOrigin, str, err);
		}
	} else if (op == ":::=") {
		str += op.size();
		Stmt::skipWhitespace(str);

		// todo

		assignSimpleVariable(identifier, varOrigin, str, err);

	} else if (op == ":") {
		str += op.size();
		Stmt::skipWhitespace(str);

		Vector<Target *> targets;

		identifier.split<StringView::WhiteSpace>(
				[&](StringView s) { emplace_ordered(targets, addTarget(s)); });

		if (targets.empty()) {
			targets.emplace_back(nullptr);
		} else {
			if (!str.empty()) {
				if (!addTargetPrerequisite(targets, str, err)) {
					return false;
				}
			}
		}

		_currentTargets = move(targets);

	} else if (!str.empty()) {
		err.setPos(str);
		err.reportError("Invalid char sequence");
		return err.nerrors == 0;
	}

	return err.nerrors == 0;
}

bool Makefile::processIncludeLine(StringView &str, ErrorReporter &err, bool optional) {
	if (!_engine.getCurrentBlock()->enabled) {
		return true;
	}

	Stmt::skipWhitespace(str);
	err.setPos(str);

	auto stmt = Stmt::readScoped(str, StmtType::WordList, ReadContext::LineEnd, err);
	if (!stmt) {
		return err.nerrors == 0;
	}

	auto identifier = _engine.resolve(stmt, err);

	bool ret = true;
	identifier.split<StringView::WhiteSpace>([&](StringView s) {
		auto included = includeFileByPath(s, &err, optional);
		if (!included && !optional) {
			err.reportError(toString("Fail to include file: ", s));
			ret = false;
		}
	});
	return ret;
}

} // namespace stappler::makefile
