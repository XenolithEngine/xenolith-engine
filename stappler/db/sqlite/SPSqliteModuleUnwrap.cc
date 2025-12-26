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

#include "SPSqliteDriver.h"
#include "sqlite3.h"

namespace STAPPLER_VERSIONIZED stappler::db::sqlite {

static constexpr auto UNWRAP_VALUE = 0;
static constexpr auto UNWRAP_INPUT = 0;

struct UnwrapCursor {
	sqlite3_vtab_cursor base;
	BytesView origValue;
	BytesView currentValue;
	Value value;
	size_t current = 0;
};

static sqlite3_module s_UnwrapModule = {
	.iVersion = 4,
	.xCreate = nullptr, // [] (sqlite3*, void *pAux, int argc, const char *const* argv, sqlite3_vtab **ppVTab, char**) -> int { },
	.xConnect = [] (sqlite3 *db, void *pAux, int argc, const char * const* argv, sqlite3_vtab **ppVTab, char**) -> int {
		auto driver = (Driver *)pAux;

		sqlite3_vtab *pNew;
		int rc;

		rc = driver->getHandle()->_declare_vtab(db, "CREATE TABLE x(__unwrap_value, input HIDDEN)");
		if (rc == SQLITE_OK) {
			pNew = *ppVTab = (sqlite3_vtab *)driver->getHandle()->_malloc(sizeof(*pNew));
			if (pNew == 0) {
				return SQLITE_NOMEM;
			}
			memset(pNew, 0, sizeof(*pNew));
		}
		return rc;
	},
	.xBestIndex = [] (sqlite3_vtab *pVTab, sqlite3_index_info *pIdxInfo) -> int {
		int unusableMask = 0;
		int inputIndex = -1;
		int idxMask = 0;
		auto pConstraint = pIdxInfo->aConstraint;
		for (int i = 0; i < pIdxInfo->nConstraint; i++, pConstraint++) {
			int iCol;
			int iMask;
			if (pConstraint->iColumn < UNWRAP_INPUT)
				continue;
			iCol = pConstraint->iColumn - UNWRAP_INPUT;
			iMask = 1 << iCol;
			if (pConstraint->usable == 0) {
				unusableMask |= iMask;
			} else if (pConstraint->op == SQLITE_INDEX_CONSTRAINT_EQ) {
				inputIndex = i;
				idxMask |= iMask;
			}
		}

		if (pIdxInfo->nOrderBy > 0 && pIdxInfo->aOrderBy[0].iColumn < 0 && pIdxInfo->aOrderBy[0].desc == 0) {
			pIdxInfo->orderByConsumed = 1;
		}

		if ((unusableMask & ~idxMask) != 0) {
			/* If there are any unusable constraints, then reject
			 ** this entire plan */
			return SQLITE_CONSTRAINT;
		}

		if (inputIndex < 0) {
			/* No input */
			pIdxInfo->idxNum = 0;
		} else {
			pIdxInfo->estimatedCost = 1.0;
			pIdxInfo->aConstraintUsage[inputIndex].argvIndex = 1;
			pIdxInfo->aConstraintUsage[inputIndex].omit = 1;
			pIdxInfo->idxNum = 1; /* Only JSON supplied.  Plan 1 */
		}
		return SQLITE_OK;
	},
	.xDisconnect = [] (sqlite3_vtab *pVTab) -> int {
		auto sym = DriverSym::getCurrent();
		sym->_free(pVTab);
		return SQLITE_OK;
	},
	.xDestroy = nullptr, // [] (sqlite3_vtab *pVTab) -> int { },
	.xOpen = [] (sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor) -> int {
		auto sym = DriverSym::getCurrent();
		UnwrapCursor *pCur;
		pCur = (UnwrapCursor *)sym->_malloc(sizeof(*pCur));
		if (pCur == 0) {
			return SQLITE_NOMEM;
		}
		memset((void *)pCur, 0, sizeof(*pCur));
		*ppCursor = &pCur->base;
		return SQLITE_OK;
	},
	.xClose = [] (sqlite3_vtab_cursor *cur) -> int {
		auto sym = DriverSym::getCurrent();
		UnwrapCursor *p = (UnwrapCursor*)cur;
		p->~UnwrapCursor();
		sym->_free(cur);
		return SQLITE_OK;
	},
	.xFilter = [] (sqlite3_vtab_cursor *cur, int idxNum, const char *idxStr, int argc, sqlite3_value **argv) -> int {
		auto sym = DriverSym::getCurrent();
		UnwrapCursor *p = (UnwrapCursor*)cur;
		p->origValue = p->currentValue = BytesView((const uint8_t *)sym->_value_blob(argv[0]), sym->_value_bytes(argv[0]));
		p->value = data::read<Interface>(p->origValue);
		p->current = 0;
		if (p->value.isArray() || p->value.empty()) {
			return SQLITE_OK;
		}
		p->value = Value();
		return SQLITE_MISMATCH;
	},
	.xNext = [] (sqlite3_vtab_cursor *cur) -> int {
		UnwrapCursor *p = (UnwrapCursor*)cur;
		++ p->current;
		return SQLITE_OK;
	},
	.xEof = [] (sqlite3_vtab_cursor *cur) -> int {
		UnwrapCursor *p = (UnwrapCursor*)cur;
		if (p->value.empty() || p->current == p->value.size()) {
			return 1;
		}
		return 0;
	},
	.xColumn = [] (sqlite3_vtab_cursor *cur, sqlite3_context *db, int col) -> int {
		auto sym = DriverSym::getCurrent();
		UnwrapCursor *p = (UnwrapCursor*)cur;

		auto &val = p->value.getValue(p->current);
		switch (val.getType()) {
		case Value::Type::INTEGER:
			sym->_result_int64(db, val.getInteger());
			break;
		case Value::Type::DOUBLE:
			sym->_result_double(db, val.getDouble());
			break;
		case Value::Type::BOOLEAN:
			sym->_result_int(db, val.getBool() ? 1 : 0);
			break;
		case Value::Type::CHARSTRING:
			sym->_result_text64(db, val.getString().data(), val.getString().size(), nullptr, SQLITE_UTF8);
			break;
		case Value::Type::BYTESTRING:
			sym->_result_blob64(db, val.getBytes().data(), val.getBytes().size(), nullptr);
			break;
		default:
			sym->_result_null(db);
			break;
		}
		return SQLITE_OK;
	},
	.xRowid = [] (sqlite3_vtab_cursor *cur, sqlite3_int64 *pRowid) -> int {
		UnwrapCursor *p = (UnwrapCursor*) cur;
		*pRowid = p->current;
		return SQLITE_OK;
	},
	.xUpdate = nullptr, // [] (sqlite3_vtab *, int, sqlite3_value **, sqlite3_int64 *) -> int { },
	.xBegin = nullptr, // [] (sqlite3_vtab *pVTab) -> int { },
	.xSync = nullptr, // [] (sqlite3_vtab *pVTab) -> int { },
	.xCommit = nullptr, // [] (sqlite3_vtab *pVTab) -> int { },
	.xRollback = nullptr, // [] (sqlite3_vtab *pVTab) -> int { },
	.xFindFunction = nullptr, // [] (sqlite3_vtab *pVtab, int nArg, const char *zName,
			// void (**pxFunc)(sqlite3_context*,int,sqlite3_value**), void **ppArg) -> int { },
	.xRename = nullptr, // [] (sqlite3_vtab *pVtab, const char *zNew) -> int { },
	  /* The methods above are in version 1 of the sqlite_module object. Those
	  ** below are for version 2 and greater. */
	.xSavepoint = nullptr, // [] (sqlite3_vtab *pVTab, int) -> int { },
	.xRelease = nullptr, // [] (sqlite3_vtab *pVTab, int) -> int { },
	.xRollbackTo = nullptr, // [] (sqlite3_vtab *pVTab, int) -> int { },
	  /* The methods above are in versions 1 and 2 of the sqlite_module object.
	  ** Those below are for version 3 and greater. */
	.xShadowName = nullptr, // [] (const char*) -> int { }
};

}
