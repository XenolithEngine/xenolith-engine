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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_MESSAGE_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_MESSAGE_API_H_


#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>

// clang-format off
#define __SPRT_LOWORD(l)     ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define __SPRT_HIWORD(l)     ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))

#define __SPRT_GET_X_LPARAM(lp)                        ((int)(short)__SPRT_LOWORD(lp))
#define __SPRT_GET_Y_LPARAM(lp)                        ((int)(short)__SPRT_HIWORD(lp))

#define __SPRT_GET_KEYSTATE_WPARAM(wParam)     (__SPRT_LOWORD(wParam))
#define __SPRT_GET_NCHITTEST_WPARAM(wParam)    ((short)__SPRT_LOWORD(wParam))
#define __SPRT_GET_XBUTTON_WPARAM(wParam)      (__SPRT_HIWORD(wParam))

#define __SPRT_WM_NULL                         0x0000
#define __SPRT_WM_CREATE                       0x0001
#define __SPRT_WM_DESTROY                      0x0002
#define __SPRT_WM_MOVE                         0x0003
#define __SPRT_WM_SIZE                         0x0005
#define __SPRT_WM_ACTIVATE                     0x0006
#define __SPRT_WM_SETFOCUS                     0x0007
#define __SPRT_WM_KILLFOCUS                    0x0008
#define __SPRT_WM_ENABLE                       0x000A
#define __SPRT_WM_SETREDRAW                    0x000B
#define __SPRT_WM_SETTEXT                      0x000C
#define __SPRT_WM_GETTEXT                      0x000D
#define __SPRT_WM_GETTEXTLENGTH                0x000E
#define __SPRT_WM_PAINT                        0x000F
#define __SPRT_WM_CLOSE                        0x0010
#define __SPRT_WM_QUERYENDSESSION              0x0011
#define __SPRT_WM_QUERYOPEN                    0x0013
#define __SPRT_WM_ENDSESSION                   0x0016
#define __SPRT_WM_QUIT                         0x0012
#define __SPRT_WM_ERASEBKGND                   0x0014
#define __SPRT_WM_SYSCOLORCHANGE               0x0015
#define __SPRT_WM_SHOWWINDOW                   0x0018
#define __SPRT_WM_WININICHANGE                 0x001A
#define __SPRT_WM_SETTINGCHANGE                __SPRT_WM_WININICHANGE
#define __SPRT_WM_DEVMODECHANGE                0x001B
#define __SPRT_WM_ACTIVATEAPP                  0x001C
#define __SPRT_WM_FONTCHANGE                   0x001D
#define __SPRT_WM_TIMECHANGE                   0x001E
#define __SPRT_WM_CANCELMODE                   0x001F
#define __SPRT_WM_SETCURSOR                    0x0020
#define __SPRT_WM_MOUSEACTIVATE                0x0021
#define __SPRT_WM_CHILDACTIVATE                0x0022
#define __SPRT_WM_QUEUESYNC                    0x0023
#define __SPRT_WM_GETMINMAXINFO                0x0024
#define __SPRT_WM_PAINTICON                    0x0026
#define __SPRT_WM_ICONERASEBKGND               0x0027
#define __SPRT_WM_NEXTDLGCTL                   0x0028
#define __SPRT_WM_SPOOLERSTATUS                0x002A
#define __SPRT_WM_DRAWITEM                     0x002B
#define __SPRT_WM_MEASUREITEM                  0x002C
#define __SPRT_WM_DELETEITEM                   0x002D
#define __SPRT_WM_VKEYTOITEM                   0x002E
#define __SPRT_WM_CHARTOITEM                   0x002F
#define __SPRT_WM_SETFONT                      0x0030
#define __SPRT_WM_GETFONT                      0x0031
#define __SPRT_WM_SETHOTKEY                    0x0032
#define __SPRT_WM_GETHOTKEY                    0x0033
#define __SPRT_WM_QUERYDRAGICON                0x0037
#define __SPRT_WM_COMPAREITEM                  0x0039
#define __SPRT_WM_GETOBJECT                    0x003D
#define __SPRT_WM_COMPACTING                   0x0041
#define __SPRT_WM_COMMNOTIFY                   0x0044
#define __SPRT_WM_WINDOWPOSCHANGING            0x0046
#define __SPRT_WM_WINDOWPOSCHANGED             0x0047
#define __SPRT_WM_POWER                        0x0048
#define __SPRT_WM_COPYDATA                     0x004A
#define __SPRT_WM_CANCELJOURNAL                0x004B
#define __SPRT_WM_NOTIFY                       0x004E
#define __SPRT_WM_INPUTLANGCHANGEREQUEST       0x0050
#define __SPRT_WM_INPUTLANGCHANGE              0x0051
#define __SPRT_WM_TCARD                        0x0052
#define __SPRT_WM_HELP                         0x0053
#define __SPRT_WM_USERCHANGED                  0x0054
#define __SPRT_WM_NOTIFYFORMAT                 0x0055
#define __SPRT_WM_CONTEXTMENU                  0x007B
#define __SPRT_WM_STYLECHANGING                0x007C
#define __SPRT_WM_STYLECHANGED                 0x007D
#define __SPRT_WM_DISPLAYCHANGE                0x007E
#define __SPRT_WM_GETICON                      0x007F
#define __SPRT_WM_SETICON                      0x0080

/* wParam for WM_GETICON / WM_SETICON */
#define __SPRT_ICON_SMALL                      0
#define __SPRT_ICON_BIG                        1
#define __SPRT_ICON_SMALL2                     2

#define __SPRT_WM_NCCREATE                     0x0081
#define __SPRT_WM_NCDESTROY                    0x0082
#define __SPRT_WM_NCCALCSIZE                   0x0083
#define __SPRT_WM_NCHITTEST                    0x0084
#define __SPRT_WM_NCPAINT                      0x0085
#define __SPRT_WM_NCACTIVATE                   0x0086
#define __SPRT_WM_GETDLGCODE                   0x0087
#define __SPRT_WM_SYNCPAINT                    0x0088
#define __SPRT_WM_NCMOUSEMOVE                  0x00A0
#define __SPRT_WM_NCLBUTTONDOWN                0x00A1
#define __SPRT_WM_NCLBUTTONUP                  0x00A2
#define __SPRT_WM_NCLBUTTONDBLCLK              0x00A3
#define __SPRT_WM_NCRBUTTONDOWN                0x00A4
#define __SPRT_WM_NCRBUTTONUP                  0x00A5
#define __SPRT_WM_NCRBUTTONDBLCLK              0x00A6
#define __SPRT_WM_NCMBUTTONDOWN                0x00A7
#define __SPRT_WM_NCMBUTTONUP                  0x00A8
#define __SPRT_WM_NCMBUTTONDBLCLK              0x00A9
#define __SPRT_WM_NCXBUTTONDOWN                0x00AB
#define __SPRT_WM_NCXBUTTONUP                  0x00AC
#define __SPRT_WM_NCXBUTTONDBLCLK              0x00AD
#define __SPRT_WM_INPUT_DEVICE_CHANGE          0x00FE
#define __SPRT_WM_INPUT                        0x00FF
#define __SPRT_WM_KEYFIRST                     0x0100
#define __SPRT_WM_KEYDOWN                      0x0100
#define __SPRT_WM_KEYUP                        0x0101
#define __SPRT_WM_CHAR                         0x0102
#define __SPRT_WM_DEADCHAR                     0x0103
#define __SPRT_WM_SYSKEYDOWN                   0x0104
#define __SPRT_WM_SYSKEYUP                     0x0105
#define __SPRT_WM_SYSCHAR                      0x0106
#define __SPRT_WM_SYSDEADCHAR                  0x0107
#define __SPRT_WM_UNICHAR                      0x0109
#define __SPRT_WM_KEYLAST                      0x0109
#define __SPRT_UNICODE_NOCHAR                  0xFFFF
#define __SPRT_WM_IME_STARTCOMPOSITION         0x010D
#define __SPRT_WM_IME_ENDCOMPOSITION           0x010E
#define __SPRT_WM_IME_COMPOSITION              0x010F
#define __SPRT_WM_IME_KEYLAST                  0x010F
#define __SPRT_WM_INITDIALOG                   0x0110
#define __SPRT_WM_COMMAND                      0x0111
#define __SPRT_WM_SYSCOMMAND                   0x0112
#define __SPRT_WM_TIMER                        0x0113
#define __SPRT_WM_HSCROLL                      0x0114
#define __SPRT_WM_VSCROLL                      0x0115
#define __SPRT_WM_INITMENU                     0x0116
#define __SPRT_WM_INITMENUPOPUP                0x0117
#define __SPRT_WM_GESTURE                      0x0119
#define __SPRT_WM_GESTURENOTIFY                0x011A
#define __SPRT_WM_MENUSELECT                   0x011F
#define __SPRT_WM_MENUCHAR                     0x0120
#define __SPRT_WM_ENTERIDLE                    0x0121
#define __SPRT_WM_MENURBUTTONUP                0x0122
#define __SPRT_WM_MENUDRAG                     0x0123
#define __SPRT_WM_MENUGETOBJECT                0x0124
#define __SPRT_WM_UNINITMENUPOPUP              0x0125
#define __SPRT_WM_MENUCOMMAND                  0x0126
#define __SPRT_WM_CHANGEUISTATE                0x0127
#define __SPRT_WM_UPDATEUISTATE                0x0128
#define __SPRT_WM_QUERYUISTATE                 0x0129
#define __SPRT_WM_CTLCOLORMSGBOX               0x0132
#define __SPRT_WM_CTLCOLOREDIT                 0x0133
#define __SPRT_WM_CTLCOLORLISTBOX              0x0134
#define __SPRT_WM_CTLCOLORBTN                  0x0135
#define __SPRT_WM_CTLCOLORDLG                  0x0136
#define __SPRT_WM_CTLCOLORSCROLLBAR            0x0137
#define __SPRT_WM_CTLCOLORSTATIC               0x0138
#define __SPRT_MN_GETHMENU                     0x01E1
#define __SPRT_WM_MOUSEFIRST                   0x0200
#define __SPRT_WM_MOUSEMOVE                    0x0200
#define __SPRT_WM_LBUTTONDOWN                  0x0201
#define __SPRT_WM_LBUTTONUP                    0x0202
#define __SPRT_WM_LBUTTONDBLCLK                0x0203
#define __SPRT_WM_RBUTTONDOWN                  0x0204
#define __SPRT_WM_RBUTTONUP                    0x0205
#define __SPRT_WM_RBUTTONDBLCLK                0x0206
#define __SPRT_WM_MBUTTONDOWN                  0x0207
#define __SPRT_WM_MBUTTONUP                    0x0208
#define __SPRT_WM_MBUTTONDBLCLK                0x0209
#define __SPRT_WM_MOUSEWHEEL                   0x020A
#define __SPRT_WM_XBUTTONDOWN                  0x020B
#define __SPRT_WM_XBUTTONUP                    0x020C
#define __SPRT_WM_XBUTTONDBLCLK                0x020D
#define __SPRT_WM_MOUSEHWHEEL                  0x020E
#define __SPRT_WM_MOUSELAST                    0x020E
#define __SPRT_WM_PARENTNOTIFY                 0x0210
#define __SPRT_WM_ENTERMENULOOP                0x0211
#define __SPRT_WM_EXITMENULOOP                 0x0212
#define __SPRT_WM_NEXTMENU                     0x0213
#define __SPRT_WM_SIZING                       0x0214
#define __SPRT_WM_CAPTURECHANGED               0x0215
#define __SPRT_WM_MOVING                       0x0216
#define __SPRT_WM_POWERBROADCAST               0x0218
#define __SPRT_WM_DEVICECHANGE                 0x0219
#define __SPRT_WM_MDICREATE                    0x0220
#define __SPRT_WM_MDIDESTROY                   0x0221
#define __SPRT_WM_MDIACTIVATE                  0x0222
#define __SPRT_WM_MDIRESTORE                   0x0223
#define __SPRT_WM_MDINEXT                      0x0224
#define __SPRT_WM_MDIMAXIMIZE                  0x0225
#define __SPRT_WM_MDITILE                      0x0226
#define __SPRT_WM_MDICASCADE                   0x0227
#define __SPRT_WM_MDIICONARRANGE               0x0228
#define __SPRT_WM_MDIGETACTIVE                 0x0229
#define __SPRT_WM_MDISETMENU                   0x0230
#define __SPRT_WM_ENTERSIZEMOVE                0x0231
#define __SPRT_WM_EXITSIZEMOVE                 0x0232
#define __SPRT_WM_DROPFILES                    0x0233
#define __SPRT_WM_MDIREFRESHMENU               0x0234
#define __SPRT_WM_POINTERDEVICECHANGE          0x0238
#define __SPRT_WM_POINTERDEVICEINRANGE         0x0239
#define __SPRT_WM_POINTERDEVICEOUTOFRANGE      0x023A
#define __SPRT_WM_TOUCH                        0x0240
#define __SPRT_WM_NCPOINTERUPDATE              0x0241
#define __SPRT_WM_NCPOINTERDOWN                0x0242
#define __SPRT_WM_NCPOINTERUP                  0x0243
#define __SPRT_WM_POINTERUPDATE                0x0245
#define __SPRT_WM_POINTERDOWN                  0x0246
#define __SPRT_WM_POINTERUP                    0x0247
#define __SPRT_WM_POINTERENTER                 0x0249
#define __SPRT_WM_POINTERLEAVE                 0x024A
#define __SPRT_WM_POINTERACTIVATE              0x024B
#define __SPRT_WM_POINTERCAPTURECHANGED        0x024C
#define __SPRT_WM_TOUCHHITTESTING              0x024D
#define __SPRT_WM_POINTERWHEEL                 0x024E
#define __SPRT_WM_POINTERHWHEEL                0x024F
#define __SPRT_DM_POINTERHITTEST               0x0250
#define __SPRT_WM_POINTERROUTEDTO              0x0251
#define __SPRT_WM_POINTERROUTEDAWAY            0x0252
#define __SPRT_WM_POINTERROUTEDRELEASED        0x0253
#define __SPRT_WM_IME_SETCONTEXT               0x0281
#define __SPRT_WM_IME_NOTIFY                   0x0282
#define __SPRT_WM_IME_CONTROL                  0x0283
#define __SPRT_WM_IME_COMPOSITIONFULL          0x0284
#define __SPRT_WM_IME_SELECT                   0x0285
#define __SPRT_WM_IME_CHAR                     0x0286
#define __SPRT_WM_IME_REQUEST                  0x0288
#define __SPRT_WM_IME_KEYDOWN                  0x0290
#define __SPRT_WM_IME_KEYUP                    0x0291
#define __SPRT_WM_MOUSEHOVER                   0x02A1
#define __SPRT_WM_MOUSELEAVE                   0x02A3
#define __SPRT_WM_NCMOUSEHOVER                 0x02A0
#define __SPRT_WM_NCMOUSELEAVE                 0x02A2
#define __SPRT_WM_MOUSEHOVER                   0x02A1
#define __SPRT_WM_MOUSELEAVE                   0x02A3
#define __SPRT_WM_DPICHANGED                   0x02E0
#define __SPRT_WM_DPICHANGED_BEFOREPARENT      0x02E2
#define __SPRT_WM_DPICHANGED_AFTERPARENT       0x02E3
#define __SPRT_WM_GETDPISCALEDSIZE             0x02E4

// Clipboard. WM_RENDERFORMAT and WM_RENDERALLFORMATS are DELAYED RENDERING: an owner that put a
// format up with a null handle is asked for the bytes here, which is the shape a lazy encoder wants.
#define __SPRT_WM_RENDERFORMAT                 0x0305
#define __SPRT_WM_RENDERALLFORMATS             0x0306
#define __SPRT_WM_DESTROYCLIPBOARD             0x0307
#define __SPRT_WM_CLIPBOARDUPDATE              0x031D

#define __SPRT_WM_DWMCOMPOSITIONCHANGED        0x031E
#define __SPRT_WM_DWMNCRENDERINGCHANGED        0x031F
#define __SPRT_WM_DWMCOLORIZATIONCOLORCHANGED  0x0320
#define __SPRT_WM_DWMWINDOWMAXIMIZEDCHANGE     0x0321

#define __SPRT_WA_INACTIVE     0
#define __SPRT_WA_ACTIVE       1
#define __SPRT_WA_CLICKACTIVE  2

#define __SPRT_PBT_APMQUERYSUSPEND             0x0000
#define __SPRT_PBT_APMQUERYSTANDBY             0x0001
#define __SPRT_PBT_APMQUERYSUSPENDFAILED       0x0002
#define __SPRT_PBT_APMQUERYSTANDBYFAILED       0x0003
#define __SPRT_PBT_APMSUSPEND                  0x0004
#define __SPRT_PBT_APMSTANDBY                  0x0005
#define __SPRT_PBT_APMRESUMECRITICAL           0x0006
#define __SPRT_PBT_APMRESUMESUSPEND            0x0007
#define __SPRT_PBT_APMRESUMESTANDBY            0x0008
#define __SPRT_PBT_APMBATTERYLOW               0x0009
#define __SPRT_PBT_APMPOWERSTATUSCHANGE        0x000A
#define __SPRT_PBT_APMOEMEVENT                 0x000B
#define __SPRT_PBT_APMRESUMEAUTOMATIC          0x0012
#define __SPRT_PBT_POWERSETTINGCHANGE          0x8013

#define __SPRT_SM_CXSCREEN             0
#define __SPRT_SM_CYSCREEN             1
#define __SPRT_SM_CXVSCROLL            2
#define __SPRT_SM_CYHSCROLL            3
#define __SPRT_SM_CYCAPTION            4
#define __SPRT_SM_CXBORDER             5
#define __SPRT_SM_CYBORDER             6
#define __SPRT_SM_CXDLGFRAME           7
#define __SPRT_SM_CYDLGFRAME           8
#define __SPRT_SM_CYVTHUMB             9
#define __SPRT_SM_CXHTHUMB             10
#define __SPRT_SM_CXICON               11
#define __SPRT_SM_CYICON               12
#define __SPRT_SM_CXCURSOR             13
#define __SPRT_SM_CYCURSOR             14
#define __SPRT_SM_CYMENU               15
#define __SPRT_SM_CXFULLSCREEN         16
#define __SPRT_SM_CYFULLSCREEN         17
#define __SPRT_SM_CYKANJIWINDOW        18
#define __SPRT_SM_MOUSEPRESENT         19
#define __SPRT_SM_CYVSCROLL            20
#define __SPRT_SM_CXHSCROLL            21
#define __SPRT_SM_DEBUG                22
#define __SPRT_SM_SWAPBUTTON           23
#define __SPRT_SM_RESERVED1            24
#define __SPRT_SM_RESERVED2            25
#define __SPRT_SM_RESERVED3            26
#define __SPRT_SM_RESERVED4            27
#define __SPRT_SM_CXMIN                28
#define __SPRT_SM_CYMIN                29
#define __SPRT_SM_CXSIZE               30
#define __SPRT_SM_CYSIZE               31
#define __SPRT_SM_CXFRAME              32
#define __SPRT_SM_CYFRAME              33
#define __SPRT_SM_CXMINTRACK           34
#define __SPRT_SM_CYMINTRACK           35
#define __SPRT_SM_CXDOUBLECLK          36
#define __SPRT_SM_CYDOUBLECLK          37
#define __SPRT_SM_CXICONSPACING        38
#define __SPRT_SM_CYICONSPACING        39
#define __SPRT_SM_MENUDROPALIGNMENT    40
#define __SPRT_SM_PENWINDOWS           41
#define __SPRT_SM_DBCSENABLED          42
#define __SPRT_SM_CMOUSEBUTTONS        43
#define __SPRT_SM_CXFIXEDFRAME           __SPRT_SM_CXDLGFRAME  /* ;win40 name change */
#define __SPRT_SM_CYFIXEDFRAME           __SPRT_SM_CYDLGFRAME  /* ;win40 name change */
#define __SPRT_SM_CXSIZEFRAME            __SPRT_SM_CXFRAME     /* ;win40 name change */
#define __SPRT_SM_CYSIZEFRAME            __SPRT_SM_CYFRAME     /* ;win40 name change */
#define __SPRT_SM_SECURE               44
#define __SPRT_SM_CXEDGE               45
#define __SPRT_SM_CYEDGE               46
#define __SPRT_SM_CXMINSPACING         47
#define __SPRT_SM_CYMINSPACING         48
#define __SPRT_SM_CXSMICON             49
#define __SPRT_SM_CYSMICON             50
#define __SPRT_SM_CYSMCAPTION          51
#define __SPRT_SM_CXSMSIZE             52
#define __SPRT_SM_CYSMSIZE             53
#define __SPRT_SM_CXMENUSIZE           54
#define __SPRT_SM_CYMENUSIZE           55
#define __SPRT_SM_ARRANGE              56
#define __SPRT_SM_CXMINIMIZED          57
#define __SPRT_SM_CYMINIMIZED          58
#define __SPRT_SM_CXMAXTRACK           59
#define __SPRT_SM_CYMAXTRACK           60
#define __SPRT_SM_CXMAXIMIZED          61
#define __SPRT_SM_CYMAXIMIZED          62
#define __SPRT_SM_NETWORK              63
#define __SPRT_SM_CLEANBOOT            67
#define __SPRT_SM_CXDRAG               68
#define __SPRT_SM_CYDRAG               69
#define __SPRT_SM_SHOWSOUNDS           70
#define __SPRT_SM_CXMENUCHECK          71
#define __SPRT_SM_CYMENUCHECK          72
#define __SPRT_SM_SLOWMACHINE          73
#define __SPRT_SM_MIDEASTENABLED       74
#define __SPRT_SM_MOUSEWHEELPRESENT    75
#define __SPRT_SM_XVIRTUALSCREEN       76
#define __SPRT_SM_YVIRTUALSCREEN       77
#define __SPRT_SM_CXVIRTUALSCREEN      78
#define __SPRT_SM_CYVIRTUALSCREEN      79
#define __SPRT_SM_CMONITORS            80
#define __SPRT_SM_SAMEDISPLAYFORMAT    81
#define __SPRT_SM_IMMENABLED           82
#define __SPRT_SM_CXFOCUSBORDER        83
#define __SPRT_SM_CYFOCUSBORDER        84

#define __SPRT_SM_TABLETPC             86
#define __SPRT_SM_MEDIACENTER          87
#define __SPRT_SM_STARTER              88
#define __SPRT_SM_SERVERR2             89

#define __SPRT_SM_MOUSEHORIZONTALWHEELPRESENT    91
#define __SPRT_SM_CXPADDEDBORDER       92

#define __SPRT_SM_DIGITIZER            94
#define __SPRT_SM_MAXIMUMTOUCHES       95
#define __SPRT_SM_CMETRICS             97

#define __SPRT_PM_NOREMOVE         0x0000
#define __SPRT_PM_REMOVE           0x0001
#define __SPRT_PM_NOYIELD          0x0002

#define __SPRT_IMN_CLOSESTATUSWINDOW           0x0001
#define __SPRT_IMN_OPENSTATUSWINDOW            0x0002
#define __SPRT_IMN_CHANGECANDIDATE             0x0003
#define __SPRT_IMN_CLOSECANDIDATE              0x0004
#define __SPRT_IMN_OPENCANDIDATE               0x0005
#define __SPRT_IMN_SETCONVERSIONMODE           0x0006
#define __SPRT_IMN_SETSENTENCEMODE             0x0007
#define __SPRT_IMN_SETOPENSTATUS               0x0008
#define __SPRT_IMN_SETCANDIDATEPOS             0x0009
#define __SPRT_IMN_SETCOMPOSITIONFONT          0x000A
#define __SPRT_IMN_SETCOMPOSITIONWINDOW        0x000B
#define __SPRT_IMN_SETSTATUSWINDOWPOS          0x000C
#define __SPRT_IMN_GUIDELINE                   0x000D
#define __SPRT_IMN_PRIVATE                     0x000E

#define __SPRT_IMR_COMPOSITIONWINDOW           0x0001
#define __SPRT_IMR_CANDIDATEWINDOW             0x0002
#define __SPRT_IMR_COMPOSITIONFONT             0x0003
#define __SPRT_IMR_RECONVERTSTRING             0x0004
#define __SPRT_IMR_CONFIRMRECONVERTSTRING      0x0005
#define __SPRT_IMR_QUERYCHARPOSITION           0x0006
#define __SPRT_IMR_DOCUMENTFEED                0x0007

#define __SPRT_FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define __SPRT_FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define __SPRT_FORMAT_MESSAGE_FROM_STRING     0x00000400
#define __SPRT_FORMAT_MESSAGE_FROM_HMODULE    0x00000800
#define __SPRT_FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define __SPRT_FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000
#define __SPRT_FORMAT_MESSAGE_MAX_WIDTH_MASK  0x000000FF

#define __SPRT_LANG_NEUTRAL 0x00
#define __SPRT_SUBLANG_NEUTRAL 0x00
#define __SPRT_SUBLANG_DEFAULT 0x01

#define __SPRT_LANG_ENGLISH 0x09
#define __SPRT_SUBLANG_ENGLISH_US 0x01

#define __SPRT_MAKELANGID(p, s)       ((((WORD  )(s)) << 10) | (WORD  )(p))

/* Format selector for the MUI language-list APIs: which of the two
   representations GetUserPreferredUILanguages fills the buffer with. */
#define __SPRT_MUI_LANGUAGE_ID   0x4
#define __SPRT_MUI_LANGUAGE_NAME 0x8

#define __SPRT_MWMO_WAITALL        0x0001
#define __SPRT_MWMO_ALERTABLE      0x0002
#define __SPRT_MWMO_INPUTAVAILABLE 0x0004

#define __SPRT_QS_KEY              0x0001
#define __SPRT_QS_MOUSEMOVE        0x0002
#define __SPRT_QS_MOUSEBUTTON      0x0004
#define __SPRT_QS_POSTMESSAGE      0x0008
#define __SPRT_QS_TIMER            0x0010
#define __SPRT_QS_PAINT            0x0020
#define __SPRT_QS_SENDMESSAGE      0x0040
#define __SPRT_QS_HOTKEY           0x0080
#define __SPRT_QS_ALLPOSTMESSAGE   0x0100
#define __SPRT_QS_RAWINPUT         0x0400
#define __SPRT_QS_TOUCH            0x0800
#define __SPRT_QS_POINTER          0x1000
#define __SPRT_QS_MOUSE           (__SPRT_QS_MOUSEMOVE | __SPRT_QS_MOUSEBUTTON)
#define __SPRT_QS_INPUT           (__SPRT_QS_MOUSE | __SPRT_QS_KEY | __SPRT_QS_RAWINPUT | __SPRT_QS_TOUCH | __SPRT_QS_POINTER)
#define __SPRT_QS_ALLEVENTS       (__SPRT_QS_INPUT | __SPRT_QS_POSTMESSAGE | __SPRT_QS_TIMER | __SPRT_QS_PAINT | __SPRT_QS_HOTKEY)
#define __SPRT_QS_ALLINPUT        (__SPRT_QS_INPUT | __SPRT_QS_POSTMESSAGE | __SPRT_QS_TIMER | __SPRT_QS_PAINT | __SPRT_QS_HOTKEY | __SPRT_QS_SENDMESSAGE)

// clang-format on

typedef HANDLE HWND, HFONT;

typedef struct tagMSG {
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
	DWORD lPrivate;
} MSG, *PMSG, *NPMSG, *LPMSG;

typedef struct _SYSTEM_POWER_STATUS {
	BYTE ACLineStatus;
	BYTE BatteryFlag;
	BYTE BatteryLifePercent;
	BYTE SystemStatusFlag;
	DWORD BatteryLifeTime;
	DWORD BatteryFullLifeTime;
} SYSTEM_POWER_STATUS, *LPSYSTEM_POWER_STATUS;

typedef struct __MIDL___MIDL_itf_dimm_0000_0000_0004 {
	LONG lfHeight;
	LONG lfWidth;
	LONG lfEscapement;
	LONG lfOrientation;
	LONG lfWeight;
	BYTE lfItalic;
	BYTE lfUnderline;
	BYTE lfStrikeOut;
	BYTE lfCharSet;
	BYTE lfOutPrecision;
	BYTE lfClipPrecision;
	BYTE lfQuality;
	BYTE lfPitchAndFamily;
	WCHAR lfFaceName[32];
} LOGFONTW;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_MESSAGE_API_H_
