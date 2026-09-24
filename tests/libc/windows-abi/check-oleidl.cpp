// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/oleidl.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// What an IDropTarget receives: the effect bits it answers with, the FORMATETC/STGMEDIUM pair it
// reads an IDataObject through, and the DROPFILES header of a CF_HDROP block.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/oleidl.h>
#include "abi_check.h"

#include <windows.h>
#include <oleidl.h>
#include <shlobj.h> // DROPFILES

SPRT_CONST(DROPEFFECT_NONE);
SPRT_CONST(DROPEFFECT_COPY);
SPRT_CONST(DROPEFFECT_MOVE);
SPRT_CONST(DROPEFFECT_LINK);

SPRT_CONST(CF_TEXT);
SPRT_CONST(CF_UNICODETEXT);
SPRT_CONST(CF_HDROP);

SPRT_CONST(TYMED_HGLOBAL);
SPRT_CONST(DVASPECT_CONTENT);

SPRT_CONST(MK_SHIFT);
SPRT_CONST(MK_CONTROL);
SPRT_CONST(MK_ALT);

SPRT_SIZE(FORMATETC);
SPRT_OFFSET(FORMATETC, cfFormat);
SPRT_OFFSET(FORMATETC, ptd);
SPRT_OFFSET(FORMATETC, dwAspect);
SPRT_OFFSET(FORMATETC, lindex);
SPRT_OFFSET(FORMATETC, tymed);

SPRT_SIZE(STGMEDIUM);
SPRT_OFFSET(STGMEDIUM, tymed);
SPRT_OFFSET(STGMEDIUM, hGlobal);
SPRT_OFFSET(STGMEDIUM, pUnkForRelease);

SPRT_SIZE(DROPFILES);
SPRT_OFFSET(DROPFILES, pFiles);
SPRT_OFFSET(DROPFILES, pt);
SPRT_OFFSET(DROPFILES, fNC);
SPRT_OFFSET(DROPFILES, fWide);
