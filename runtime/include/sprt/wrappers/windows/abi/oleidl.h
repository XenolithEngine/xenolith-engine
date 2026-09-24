/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_OLEIDL_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_OLEIDL_H_

#include <sprt/wrappers/windows/abi/basic_types.h>
#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/basic_api.h> // HGLOBAL

// clang-format off
#define __SPRT_DROPEFFECT_NONE 0
#define __SPRT_DROPEFFECT_COPY 1
#define __SPRT_DROPEFFECT_MOVE 2
#define __SPRT_DROPEFFECT_LINK 4

#define __SPRT_CF_TEXT 1
#define __SPRT_CF_UNICODETEXT 13
#define __SPRT_CF_HDROP 15

#define __SPRT_TYMED_HGLOBAL 1
#define __SPRT_DVASPECT_CONTENT 1

// grfKeyState of IDropTarget: the mouse buttons and modifiers held during the drag
#define __SPRT_MK_SHIFT 0x0004
#define __SPRT_MK_CONTROL 0x0008
#define __SPRT_MK_ALT 0x0020
// clang-format on

typedef WORD CLIPFORMAT;

struct IUnknown;
struct tagDVTARGETDEVICE;

typedef struct tagFORMATETC {
	CLIPFORMAT cfFormat;
	struct tagDVTARGETDEVICE *ptd;
	DWORD dwAspect;
	LONG lindex;
	DWORD tymed;
} FORMATETC, *LPFORMATETC;

// The union holds one handle or pointer whichever TYMED says; every member is pointer-sized
typedef struct tagSTGMEDIUM {
	DWORD tymed;
	union {
		void *hBitmap;
		void *hMetaFilePict;
		void *hEnhMetaFile;
		HGLOBAL hGlobal;
		LPWSTR lpszFileName;
		void *pstm;
		void *pstg;
	};
	struct IUnknown *pUnkForRelease;
} STGMEDIUM, *LPSTGMEDIUM;

// The header of a CF_HDROP block: the file list starts `pFiles` bytes in, double-NUL-terminated
typedef struct _DROPFILES {
	DWORD pFiles;
	POINT pt;
	BOOL fNC;
	BOOL fWide;
} DROPFILES, *LPDROPFILES;

#endif
