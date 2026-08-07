/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

// <commdlg.h>: the colour and font choosers.
//
// These two have no COM successor — IFileDialog replaced the file pickers, but ChooseColor and
// ChooseFont are still the only system UI for their job — so unlike the file dialogs they are
// called through their original Win32 entry points.
//
// Both run their own modal message loop inside the call and return only once the user is done.
//
// Clean names only; the values and the two struct layouts live in abi/commdlg.h.

#ifndef SPRT_WRAPPERS_WINDOWS_COMMDLG_H_
#define SPRT_WRAPPERS_WINDOWS_COMMDLG_H_

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/abi/commdlg.h>

/* Clean public names (materialized __SPRT_ values live in abi/commdlg.h) */
#define CC_RGBINIT __SPRT_CC_RGBINIT
#define CC_FULLOPEN __SPRT_CC_FULLOPEN
#define CC_PREVENTFULLOPEN __SPRT_CC_PREVENTFULLOPEN
#define CC_SHOWHELP __SPRT_CC_SHOWHELP
#define CC_ENABLEHOOK __SPRT_CC_ENABLEHOOK
#define CC_ENABLETEMPLATE __SPRT_CC_ENABLETEMPLATE
#define CC_ENABLETEMPLATEHANDLE __SPRT_CC_ENABLETEMPLATEHANDLE
#define CC_SOLIDCOLOR __SPRT_CC_SOLIDCOLOR
#define CC_ANYCOLOR __SPRT_CC_ANYCOLOR

#define CF_SCREENFONTS __SPRT_CF_SCREENFONTS
#define CF_PRINTERFONTS __SPRT_CF_PRINTERFONTS
#define CF_BOTH __SPRT_CF_BOTH
#define CF_SHOWHELP __SPRT_CF_SHOWHELP
#define CF_ENABLEHOOK __SPRT_CF_ENABLEHOOK
#define CF_ENABLETEMPLATE __SPRT_CF_ENABLETEMPLATE
#define CF_ENABLETEMPLATEHANDLE __SPRT_CF_ENABLETEMPLATEHANDLE
#define CF_INITTOLOGFONTSTRUCT __SPRT_CF_INITTOLOGFONTSTRUCT
#define CF_USESTYLE __SPRT_CF_USESTYLE
#define CF_EFFECTS __SPRT_CF_EFFECTS
#define CF_APPLY __SPRT_CF_APPLY
#define CF_ANSIONLY __SPRT_CF_ANSIONLY
#define CF_NOVECTORFONTS __SPRT_CF_NOVECTORFONTS
#define CF_NOSIMULATIONS __SPRT_CF_NOSIMULATIONS
#define CF_LIMITSIZE __SPRT_CF_LIMITSIZE
#define CF_FIXEDPITCHONLY __SPRT_CF_FIXEDPITCHONLY
#define CF_WYSIWYG __SPRT_CF_WYSIWYG
#define CF_FORCEFONTEXIST __SPRT_CF_FORCEFONTEXIST
#define CF_SCALABLEONLY __SPRT_CF_SCALABLEONLY
#define CF_TTONLY __SPRT_CF_TTONLY
#define CF_NOFACESEL __SPRT_CF_NOFACESEL
#define CF_NOSTYLESEL __SPRT_CF_NOSTYLESEL
#define CF_NOSIZESEL __SPRT_CF_NOSIZESEL
#define CF_SELECTSCRIPT __SPRT_CF_SELECTSCRIPT
#define CF_NOSCRIPTSEL __SPRT_CF_NOSCRIPTSEL
#define CF_NOVERTFONTS __SPRT_CF_NOVERTFONTS
#define CF_INACTIVEFONTS __SPRT_CF_INACTIVEFONTS

#define FW_NORMAL __SPRT_FW_NORMAL
#define FW_BOLD __SPRT_FW_BOLD

#ifdef __cplusplus
extern "C" {
#endif

// FALSE means either "the user cancelled" or "it failed"; CommDlgExtendedError tells the two
// apart — it returns 0 after a plain cancel.
__SPRT_WIN_IMPORT WINAPI BOOL ChooseColorW(LPCHOOSECOLORW);
__SPRT_WIN_IMPORT WINAPI BOOL ChooseFontW(LPCHOOSEFONTW);
__SPRT_WIN_IMPORT WINAPI DWORD CommDlgExtendedError(void);

#ifdef __cplusplus
}
#endif

#endif // SPRT_WRAPPERS_WINDOWS_COMMDLG_H_
