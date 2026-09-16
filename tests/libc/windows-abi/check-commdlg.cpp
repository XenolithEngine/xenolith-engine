// Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
//
// abi/commdlg.h <-> Windows SDK parity. Compile-time only; see check.sh.
//
// ChooseColorW and ChooseFontW take their parameter block by address and validate it against
// lStructSize, which the caller fills in from sizeof. A layout that drifts from the SDK is
// therefore not a subtle bug — comdlg32 either rejects the call outright or reads the wrong
// fields — so both structs are pinned field by field.

#define SPRT_ABI_HEADER <sprt/wrappers/windows/abi/commdlg.h>
#include "abi_check.h"

#include <windows.h>
#include <commdlg.h> // CHOOSECOLORW / CHOOSEFONTW + CC_* / CF_*
#include <wingdi.h> // FW_NORMAL / FW_BOLD

// === ChooseColorW flags ====================================================
SPRT_CONST(CC_RGBINIT);
SPRT_CONST(CC_FULLOPEN);
SPRT_CONST(CC_PREVENTFULLOPEN);
SPRT_CONST(CC_SHOWHELP);
SPRT_CONST(CC_ENABLEHOOK);
SPRT_CONST(CC_ENABLETEMPLATE);
SPRT_CONST(CC_ENABLETEMPLATEHANDLE);
SPRT_CONST(CC_SOLIDCOLOR);
SPRT_CONST(CC_ANYCOLOR);

// === ChooseFontW flags =====================================================
SPRT_CONST(CF_SCREENFONTS);
SPRT_CONST(CF_PRINTERFONTS);
SPRT_CONST(CF_BOTH);
SPRT_CONST(CF_SHOWHELP);
SPRT_CONST(CF_ENABLEHOOK);
SPRT_CONST(CF_ENABLETEMPLATE);
SPRT_CONST(CF_ENABLETEMPLATEHANDLE);
SPRT_CONST(CF_INITTOLOGFONTSTRUCT);
SPRT_CONST(CF_USESTYLE);
SPRT_CONST(CF_EFFECTS);
SPRT_CONST(CF_APPLY);
SPRT_CONST(CF_ANSIONLY);
SPRT_CONST(CF_NOVECTORFONTS);
SPRT_CONST(CF_NOSIMULATIONS);
SPRT_CONST(CF_LIMITSIZE);
SPRT_CONST(CF_FIXEDPITCHONLY);
SPRT_CONST(CF_WYSIWYG);
SPRT_CONST(CF_FORCEFONTEXIST);
SPRT_CONST(CF_SCALABLEONLY);
SPRT_CONST(CF_TTONLY);
SPRT_CONST(CF_NOFACESEL);
SPRT_CONST(CF_NOSTYLESEL);
SPRT_CONST(CF_NOSIZESEL);
SPRT_CONST(CF_SELECTSCRIPT);
SPRT_CONST(CF_NOSCRIPTSEL);
SPRT_CONST(CF_NOVERTFONTS);
SPRT_CONST(CF_INACTIVEFONTS);

// === LOGFONTW::lfWeight ====================================================
SPRT_CONST(FW_NORMAL);
SPRT_CONST(FW_BOLD);

// === CHOOSECOLORW ==========================================================
SPRT_SIZE(CHOOSECOLORW);
SPRT_OFFSET(CHOOSECOLORW, lStructSize);
SPRT_OFFSET(CHOOSECOLORW, hwndOwner);
SPRT_OFFSET(CHOOSECOLORW, hInstance);
SPRT_OFFSET(CHOOSECOLORW, rgbResult);
SPRT_OFFSET(CHOOSECOLORW, lpCustColors);
SPRT_OFFSET(CHOOSECOLORW, Flags);
SPRT_OFFSET(CHOOSECOLORW, lCustData);
SPRT_OFFSET(CHOOSECOLORW, lpfnHook);
SPRT_OFFSET(CHOOSECOLORW, lpTemplateName);

// === CHOOSEFONTW ===========================================================
SPRT_SIZE(CHOOSEFONTW);
SPRT_OFFSET(CHOOSEFONTW, lStructSize);
SPRT_OFFSET(CHOOSEFONTW, hwndOwner);
SPRT_OFFSET(CHOOSEFONTW, hDC);
SPRT_OFFSET(CHOOSEFONTW, lpLogFont);
SPRT_OFFSET(CHOOSEFONTW, iPointSize);
SPRT_OFFSET(CHOOSEFONTW, Flags);
SPRT_OFFSET(CHOOSEFONTW, rgbColors);
SPRT_OFFSET(CHOOSEFONTW, lCustData);
SPRT_OFFSET(CHOOSEFONTW, lpfnHook);
SPRT_OFFSET(CHOOSEFONTW, lpTemplateName);
SPRT_OFFSET(CHOOSEFONTW, hInstance);
SPRT_OFFSET(CHOOSEFONTW, lpszStyle);
SPRT_OFFSET(CHOOSEFONTW, nFontType);
SPRT_OFFSET(CHOOSEFONTW, nSizeMin);
SPRT_OFFSET(CHOOSEFONTW, nSizeMax);

// === LOGFONTW ==============================================================
// It lives in abi/message_api.h, but ChooseFontW is the only caller that hands it to the OS —
// check-message_api.cpp has no reason to look at it, so it is pinned here.
SPRT_SIZE(LOGFONTW);
SPRT_OFFSET(LOGFONTW, lfHeight);
SPRT_OFFSET(LOGFONTW, lfWeight);
SPRT_OFFSET(LOGFONTW, lfItalic);
SPRT_OFFSET(LOGFONTW, lfCharSet);
SPRT_OFFSET(LOGFONTW, lfPitchAndFamily);
SPRT_OFFSET(LOGFONTW, lfFaceName);
