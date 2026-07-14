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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_USER_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_USER_API_H_


#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>

// clang-format off
#define __SPRT_KF_EXTENDED       0x0100
#define __SPRT_KF_DLGMODE        0x0800
#define __SPRT_KF_MENUMODE       0x1000
#define __SPRT_KF_ALTDOWN        0x2000
#define __SPRT_KF_REPEAT         0x4000
#define __SPRT_KF_UP             0x8000

#define __SPRT_VK_LBUTTON        0x01
#define __SPRT_VK_RBUTTON        0x02
#define __SPRT_VK_CANCEL         0x03
#define __SPRT_VK_MBUTTON        0x04    /* NOT contiguous with L & RBUTTON */
#define __SPRT_VK_BACK           0x08
#define __SPRT_VK_TAB            0x09
#define __SPRT_VK_CLEAR          0x0C
#define __SPRT_VK_RETURN         0x0D
#define __SPRT_VK_SHIFT          0x10
#define __SPRT_VK_CONTROL        0x11
#define __SPRT_VK_MENU           0x12
#define __SPRT_VK_PAUSE          0x13
#define __SPRT_VK_CAPITAL        0x14
#define __SPRT_VK_KANA           0x15
#define __SPRT_VK_HANGEUL        0x15
#define __SPRT_VK_HANGUL         0x15
#define __SPRT_VK_IME_ON         0x16
#define __SPRT_VK_JUNJA          0x17
#define __SPRT_VK_FINAL          0x18
#define __SPRT_VK_HANJA          0x19
#define __SPRT_VK_KANJI          0x19
#define __SPRT_VK_IME_OFF        0x1A
#define __SPRT_VK_ESCAPE         0x1B
#define __SPRT_VK_CONVERT        0x1C
#define __SPRT_VK_NONCONVERT     0x1D
#define __SPRT_VK_ACCEPT         0x1E
#define __SPRT_VK_MODECHANGE     0x1F
#define __SPRT_VK_SPACE          0x20
#define __SPRT_VK_PRIOR          0x21
#define __SPRT_VK_NEXT           0x22
#define __SPRT_VK_END            0x23
#define __SPRT_VK_HOME           0x24
#define __SPRT_VK_LEFT           0x25
#define __SPRT_VK_UP             0x26
#define __SPRT_VK_RIGHT          0x27
#define __SPRT_VK_DOWN           0x28
#define __SPRT_VK_SELECT         0x29
#define __SPRT_VK_PRINT          0x2A
#define __SPRT_VK_EXECUTE        0x2B
#define __SPRT_VK_SNAPSHOT       0x2C
#define __SPRT_VK_INSERT         0x2D
#define __SPRT_VK_DELETE         0x2E
#define __SPRT_VK_HELP           0x2F
#define __SPRT_VK_LWIN           0x5B
#define __SPRT_VK_RWIN           0x5C
#define __SPRT_VK_APPS           0x5D
#define __SPRT_VK_SLEEP          0x5F
#define __SPRT_VK_NUMPAD0        0x60
#define __SPRT_VK_NUMPAD1        0x61
#define __SPRT_VK_NUMPAD2        0x62
#define __SPRT_VK_NUMPAD3        0x63
#define __SPRT_VK_NUMPAD4        0x64
#define __SPRT_VK_NUMPAD5        0x65
#define __SPRT_VK_NUMPAD6        0x66
#define __SPRT_VK_NUMPAD7        0x67
#define __SPRT_VK_NUMPAD8        0x68
#define __SPRT_VK_NUMPAD9        0x69
#define __SPRT_VK_MULTIPLY       0x6A
#define __SPRT_VK_ADD            0x6B
#define __SPRT_VK_SEPARATOR      0x6C
#define __SPRT_VK_SUBTRACT       0x6D
#define __SPRT_VK_DECIMAL        0x6E
#define __SPRT_VK_DIVIDE         0x6F
#define __SPRT_VK_F1             0x70
#define __SPRT_VK_F2             0x71
#define __SPRT_VK_F3             0x72
#define __SPRT_VK_F4             0x73
#define __SPRT_VK_F5             0x74
#define __SPRT_VK_F6             0x75
#define __SPRT_VK_F7             0x76
#define __SPRT_VK_F8             0x77
#define __SPRT_VK_F9             0x78
#define __SPRT_VK_F10            0x79
#define __SPRT_VK_F11            0x7A
#define __SPRT_VK_F12            0x7B
#define __SPRT_VK_F13            0x7C
#define __SPRT_VK_F14            0x7D
#define __SPRT_VK_F15            0x7E
#define __SPRT_VK_F16            0x7F
#define __SPRT_VK_F17            0x80
#define __SPRT_VK_F18            0x81
#define __SPRT_VK_F19            0x82
#define __SPRT_VK_F20            0x83
#define __SPRT_VK_F21            0x84
#define __SPRT_VK_F22            0x85
#define __SPRT_VK_F23            0x86
#define __SPRT_VK_F24            0x87
#define __SPRT_VK_NUMLOCK        0x90
#define __SPRT_VK_SCROLL         0x91
#define __SPRT_VK_LSHIFT         0xA0
#define __SPRT_VK_RSHIFT         0xA1
#define __SPRT_VK_LCONTROL       0xA2
#define __SPRT_VK_RCONTROL       0xA3
#define __SPRT_VK_LMENU          0xA4
#define __SPRT_VK_RMENU          0xA5
#define __SPRT_VK_PROCESSKEY     0xE5

#define __SPRT_XBUTTON1      0x0001
#define __SPRT_XBUTTON2      0x0002

#define __SPRT_SPI_GETBEEP                 0x0001
#define __SPRT_SPI_SETBEEP                 0x0002
#define __SPRT_SPI_GETMOUSE                0x0003
#define __SPRT_SPI_SETMOUSE                0x0004
#define __SPRT_SPI_GETBORDER               0x0005
#define __SPRT_SPI_SETBORDER               0x0006
#define __SPRT_SPI_GETKEYBOARDSPEED        0x000A
#define __SPRT_SPI_SETKEYBOARDSPEED        0x000B
#define __SPRT_SPI_LANGDRIVER              0x000C
#define __SPRT_SPI_ICONHORIZONTALSPACING   0x000D
#define __SPRT_SPI_GETSCREENSAVETIMEOUT    0x000E
#define __SPRT_SPI_SETSCREENSAVETIMEOUT    0x000F
#define __SPRT_SPI_GETSCREENSAVEACTIVE     0x0010
#define __SPRT_SPI_SETSCREENSAVEACTIVE     0x0011
#define __SPRT_SPI_GETGRIDGRANULARITY      0x0012
#define __SPRT_SPI_SETGRIDGRANULARITY      0x0013
#define __SPRT_SPI_SETDESKWALLPAPER        0x0014
#define __SPRT_SPI_SETDESKPATTERN          0x0015
#define __SPRT_SPI_GETKEYBOARDDELAY        0x0016
#define __SPRT_SPI_SETKEYBOARDDELAY        0x0017
#define __SPRT_SPI_ICONVERTICALSPACING     0x0018
#define __SPRT_SPI_GETICONTITLEWRAP        0x0019
#define __SPRT_SPI_SETICONTITLEWRAP        0x001A
#define __SPRT_SPI_GETMENUDROPALIGNMENT    0x001B
#define __SPRT_SPI_SETMENUDROPALIGNMENT    0x001C
#define __SPRT_SPI_SETDOUBLECLKWIDTH       0x001D
#define __SPRT_SPI_SETDOUBLECLKHEIGHT      0x001E
#define __SPRT_SPI_GETICONTITLELOGFONT     0x001F
#define __SPRT_SPI_SETDOUBLECLICKTIME      0x0020
#define __SPRT_SPI_SETMOUSEBUTTONSWAP      0x0021
#define __SPRT_SPI_SETICONTITLELOGFONT     0x0022
#define __SPRT_SPI_GETFASTTASKSWITCH       0x0023
#define __SPRT_SPI_SETFASTTASKSWITCH       0x0024
#define __SPRT_SPI_SETDRAGFULLWINDOWS      0x0025
#define __SPRT_SPI_GETDRAGFULLWINDOWS      0x0026
#define __SPRT_SPI_GETNONCLIENTMETRICS     0x0029
#define __SPRT_SPI_SETNONCLIENTMETRICS     0x002A
#define __SPRT_SPI_GETMINIMIZEDMETRICS     0x002B
#define __SPRT_SPI_SETMINIMIZEDMETRICS     0x002C
#define __SPRT_SPI_GETICONMETRICS          0x002D
#define __SPRT_SPI_SETICONMETRICS          0x002E
#define __SPRT_SPI_SETWORKAREA             0x002F
#define __SPRT_SPI_GETWORKAREA             0x0030
#define __SPRT_SPI_SETPENWINDOWS           0x0031
#define __SPRT_SPI_GETHIGHCONTRAST         0x0042
#define __SPRT_SPI_SETHIGHCONTRAST         0x0043
#define __SPRT_SPI_GETKEYBOARDPREF         0x0044
#define __SPRT_SPI_SETKEYBOARDPREF         0x0045
#define __SPRT_SPI_GETSCREENREADER         0x0046
#define __SPRT_SPI_SETSCREENREADER         0x0047
#define __SPRT_SPI_GETANIMATION            0x0048
#define __SPRT_SPI_SETANIMATION            0x0049
#define __SPRT_SPI_GETFONTSMOOTHING        0x004A
#define __SPRT_SPI_SETFONTSMOOTHING        0x004B
#define __SPRT_SPI_SETDRAGWIDTH            0x004C
#define __SPRT_SPI_SETDRAGHEIGHT           0x004D
#define __SPRT_SPI_SETHANDHELD             0x004E
#define __SPRT_SPI_GETLOWPOWERTIMEOUT      0x004F
#define __SPRT_SPI_GETPOWEROFFTIMEOUT      0x0050
#define __SPRT_SPI_SETLOWPOWERTIMEOUT      0x0051
#define __SPRT_SPI_SETPOWEROFFTIMEOUT      0x0052
#define __SPRT_SPI_GETLOWPOWERACTIVE       0x0053
#define __SPRT_SPI_GETPOWEROFFACTIVE       0x0054
#define __SPRT_SPI_SETLOWPOWERACTIVE       0x0055
#define __SPRT_SPI_SETPOWEROFFACTIVE       0x0056
#define __SPRT_SPI_SETCURSORS              0x0057
#define __SPRT_SPI_SETICONS                0x0058
#define __SPRT_SPI_GETDEFAULTINPUTLANG     0x0059
#define __SPRT_SPI_SETDEFAULTINPUTLANG     0x005A
#define __SPRT_SPI_SETLANGTOGGLE           0x005B
#define __SPRT_SPI_GETWINDOWSEXTENSION     0x005C
#define __SPRT_SPI_SETMOUSETRAILS          0x005D
#define __SPRT_SPI_GETMOUSETRAILS          0x005E
#define __SPRT_SPI_SETSCREENSAVERRUNNING   0x0061
#define __SPRT_SPI_SCREENSAVERRUNNING     __SPRT_SPI_SETSCREENSAVERRUNNING
#define __SPRT_SPI_GETFILTERKEYS          0x0032
#define __SPRT_SPI_SETFILTERKEYS          0x0033
#define __SPRT_SPI_GETTOGGLEKEYS          0x0034
#define __SPRT_SPI_SETTOGGLEKEYS          0x0035
#define __SPRT_SPI_GETMOUSEKEYS           0x0036
#define __SPRT_SPI_SETMOUSEKEYS           0x0037
#define __SPRT_SPI_GETSHOWSOUNDS          0x0038
#define __SPRT_SPI_SETSHOWSOUNDS          0x0039
#define __SPRT_SPI_GETSTICKYKEYS          0x003A
#define __SPRT_SPI_SETSTICKYKEYS          0x003B
#define __SPRT_SPI_GETACCESSTIMEOUT       0x003C
#define __SPRT_SPI_SETACCESSTIMEOUT       0x003D
#define __SPRT_SPI_GETSERIALKEYS          0x003E
#define __SPRT_SPI_SETSERIALKEYS          0x003F
#define __SPRT_SPI_GETSOUNDSENTRY         0x0040
#define __SPRT_SPI_SETSOUNDSENTRY         0x0041
#define __SPRT_SPI_GETSNAPTODEFBUTTON     0x005F
#define __SPRT_SPI_SETSNAPTODEFBUTTON     0x0060
#define __SPRT_SPI_GETMOUSEHOVERWIDTH     0x0062
#define __SPRT_SPI_SETMOUSEHOVERWIDTH     0x0063
#define __SPRT_SPI_GETMOUSEHOVERHEIGHT    0x0064
#define __SPRT_SPI_SETMOUSEHOVERHEIGHT    0x0065
#define __SPRT_SPI_GETMOUSEHOVERTIME      0x0066
#define __SPRT_SPI_SETMOUSEHOVERTIME      0x0067
#define __SPRT_SPI_GETWHEELSCROLLLINES    0x0068
#define __SPRT_SPI_SETWHEELSCROLLLINES    0x0069
#define __SPRT_SPI_GETMENUSHOWDELAY       0x006A
#define __SPRT_SPI_SETMENUSHOWDELAY       0x006B
#define __SPRT_SPI_GETWHEELSCROLLCHARS   0x006C
#define __SPRT_SPI_SETWHEELSCROLLCHARS   0x006D
#define __SPRT_SPI_GETSHOWIMEUI          0x006E
#define __SPRT_SPI_SETSHOWIMEUI          0x006F
#define __SPRT_SPI_GETMOUSESPEED         0x0070
#define __SPRT_SPI_SETMOUSESPEED         0x0071
#define __SPRT_SPI_GETSCREENSAVERRUNNING 0x0072
#define __SPRT_SPI_GETDESKWALLPAPER      0x0073
#define __SPRT_SPI_GETAUDIODESCRIPTION   0x0074
#define __SPRT_SPI_SETAUDIODESCRIPTION   0x0075
#define __SPRT_SPI_GETSCREENSAVESECURE   0x0076
#define __SPRT_SPI_SETSCREENSAVESECURE   0x0077
#define __SPRT_SPI_GETHUNGAPPTIMEOUT           0x0078
#define __SPRT_SPI_SETHUNGAPPTIMEOUT           0x0079
#define __SPRT_SPI_GETWAITTOKILLTIMEOUT        0x007A
#define __SPRT_SPI_SETWAITTOKILLTIMEOUT        0x007B
#define __SPRT_SPI_GETWAITTOKILLSERVICETIMEOUT 0x007C
#define __SPRT_SPI_SETWAITTOKILLSERVICETIMEOUT 0x007D
#define __SPRT_SPI_GETMOUSEDOCKTHRESHOLD       0x007E
#define __SPRT_SPI_SETMOUSEDOCKTHRESHOLD       0x007F
#define __SPRT_SPI_GETPENDOCKTHRESHOLD         0x0080
#define __SPRT_SPI_SETPENDOCKTHRESHOLD         0x0081
#define __SPRT_SPI_GETWINARRANGING             0x0082
#define __SPRT_SPI_SETWINARRANGING             0x0083
#define __SPRT_SPI_GETMOUSEDRAGOUTTHRESHOLD    0x0084
#define __SPRT_SPI_SETMOUSEDRAGOUTTHRESHOLD    0x0085
#define __SPRT_SPI_GETPENDRAGOUTTHRESHOLD      0x0086
#define __SPRT_SPI_SETPENDRAGOUTTHRESHOLD      0x0087
#define __SPRT_SPI_GETMOUSESIDEMOVETHRESHOLD   0x0088
#define __SPRT_SPI_SETMOUSESIDEMOVETHRESHOLD   0x0089
#define __SPRT_SPI_GETPENSIDEMOVETHRESHOLD     0x008A
#define __SPRT_SPI_SETPENSIDEMOVETHRESHOLD     0x008B
#define __SPRT_SPI_GETDRAGFROMMAXIMIZE         0x008C
#define __SPRT_SPI_SETDRAGFROMMAXIMIZE         0x008D
#define __SPRT_SPI_GETSNAPSIZING               0x008E
#define __SPRT_SPI_SETSNAPSIZING               0x008F
#define __SPRT_SPI_GETDOCKMOVING               0x0090
#define __SPRT_SPI_SETDOCKMOVING               0x0091
#define __SPRT_SPI_GETLOGICALDPIOVERRIDE       0x009E
#define __SPRT_SPI_SETLOGICALDPIOVERRIDE       0x009F
#define __SPRT_SPI_GETTOUCHPADPARAMETERS       0x00AE
#define __SPRT_SPI_SETTOUCHPADPARAMETERS       0x00AF
#define __SPRT_SPI_GETACTIVEWINDOWTRACKING         0x1000
#define __SPRT_SPI_SETACTIVEWINDOWTRACKING         0x1001
#define __SPRT_SPI_GETMENUANIMATION                0x1002
#define __SPRT_SPI_SETMENUANIMATION                0x1003
#define __SPRT_SPI_GETCOMBOBOXANIMATION            0x1004
#define __SPRT_SPI_SETCOMBOBOXANIMATION            0x1005
#define __SPRT_SPI_GETLISTBOXSMOOTHSCROLLING       0x1006
#define __SPRT_SPI_SETLISTBOXSMOOTHSCROLLING       0x1007
#define __SPRT_SPI_GETGRADIENTCAPTIONS             0x1008
#define __SPRT_SPI_SETGRADIENTCAPTIONS             0x1009
#define __SPRT_SPI_GETKEYBOARDCUES                 0x100A
#define __SPRT_SPI_SETKEYBOARDCUES                 0x100B
#define __SPRT_SPI_GETMENUUNDERLINES               __SPRT_SPI_GETKEYBOARDCUES
#define __SPRT_SPI_SETMENUUNDERLINES               __SPRT_SPI_SETKEYBOARDCUES
#define __SPRT_SPI_GETACTIVEWNDTRKZORDER           0x100C
#define __SPRT_SPI_SETACTIVEWNDTRKZORDER           0x100D
#define __SPRT_SPI_GETHOTTRACKING                  0x100E
#define __SPRT_SPI_SETHOTTRACKING                  0x100F
#define __SPRT_SPI_GETMENUFADE                     0x1012
#define __SPRT_SPI_SETMENUFADE                     0x1013
#define __SPRT_SPI_GETSELECTIONFADE                0x1014
#define __SPRT_SPI_SETSELECTIONFADE                0x1015
#define __SPRT_SPI_GETTOOLTIPANIMATION             0x1016
#define __SPRT_SPI_SETTOOLTIPANIMATION             0x1017
#define __SPRT_SPI_GETTOOLTIPFADE                  0x1018
#define __SPRT_SPI_SETTOOLTIPFADE                  0x1019
#define __SPRT_SPI_GETCURSORSHADOW                 0x101A
#define __SPRT_SPI_SETCURSORSHADOW                 0x101B
#define __SPRT_SPI_GETMOUSESONAR                   0x101C
#define __SPRT_SPI_SETMOUSESONAR                   0x101D
#define __SPRT_SPI_GETMOUSECLICKLOCK               0x101E
#define __SPRT_SPI_SETMOUSECLICKLOCK               0x101F
#define __SPRT_SPI_GETMOUSEVANISH                  0x1020
#define __SPRT_SPI_SETMOUSEVANISH                  0x1021
#define __SPRT_SPI_GETFLATMENU                     0x1022
#define __SPRT_SPI_SETFLATMENU                     0x1023
#define __SPRT_SPI_GETDROPSHADOW                   0x1024
#define __SPRT_SPI_SETDROPSHADOW                   0x1025
#define __SPRT_SPI_GETBLOCKSENDINPUTRESETS         0x1026
#define __SPRT_SPI_SETBLOCKSENDINPUTRESETS         0x1027
#define __SPRT_SPI_GETUIEFFECTS                    0x103E
#define __SPRT_SPI_SETUIEFFECTS                    0x103F
#define __SPRT_SPI_GETDISABLEOVERLAPPEDCONTENT     0x1040
#define __SPRT_SPI_SETDISABLEOVERLAPPEDCONTENT     0x1041
#define __SPRT_SPI_GETCLIENTAREAANIMATION          0x1042
#define __SPRT_SPI_SETCLIENTAREAANIMATION          0x1043
#define __SPRT_SPI_GETCLEARTYPE                    0x1048
#define __SPRT_SPI_SETCLEARTYPE                    0x1049
#define __SPRT_SPI_GETSPEECHRECOGNITION            0x104A
#define __SPRT_SPI_SETSPEECHRECOGNITION            0x104B
#define __SPRT_SPI_GETCARETBROWSING                0x104C
#define __SPRT_SPI_SETCARETBROWSING                0x104D
#define __SPRT_SPI_GETTHREADLOCALINPUTSETTINGS     0x104E
#define __SPRT_SPI_SETTHREADLOCALINPUTSETTINGS     0x104F
#define __SPRT_SPI_GETSYSTEMLANGUAGEBAR            0x1050
#define __SPRT_SPI_SETSYSTEMLANGUAGEBAR            0x1051
#define __SPRT_SPI_GETFOREGROUNDLOCKTIMEOUT        0x2000
#define __SPRT_SPI_SETFOREGROUNDLOCKTIMEOUT        0x2001
#define __SPRT_SPI_GETACTIVEWNDTRKTIMEOUT          0x2002
#define __SPRT_SPI_SETACTIVEWNDTRKTIMEOUT          0x2003
#define __SPRT_SPI_GETFOREGROUNDFLASHCOUNT         0x2004
#define __SPRT_SPI_SETFOREGROUNDFLASHCOUNT         0x2005
#define __SPRT_SPI_GETCARETWIDTH                   0x2006
#define __SPRT_SPI_SETCARETWIDTH                   0x2007
#define __SPRT_SPI_GETMOUSECLICKLOCKTIME           0x2008
#define __SPRT_SPI_SETMOUSECLICKLOCKTIME           0x2009
#define __SPRT_SPI_GETFONTSMOOTHINGTYPE            0x200A
#define __SPRT_SPI_SETFONTSMOOTHINGTYPE            0x200B
#define __SPRT_SPI_GETFONTSMOOTHINGCONTRAST        0x200C
#define __SPRT_SPI_SETFONTSMOOTHINGCONTRAST        0x200D
#define __SPRT_SPI_GETFOCUSBORDERWIDTH             0x200E
#define __SPRT_SPI_SETFOCUSBORDERWIDTH             0x200F
#define __SPRT_SPI_GETFOCUSBORDERHEIGHT            0x2010
#define __SPRT_SPI_SETFOCUSBORDERHEIGHT            0x2011
#define __SPRT_SPI_GETFONTSMOOTHINGORIENTATION     0x2012
#define __SPRT_SPI_SETFONTSMOOTHINGORIENTATION     0x2013
#define __SPRT_SPI_GETMINIMUMHITRADIUS             0x2014
#define __SPRT_SPI_SETMINIMUMHITRADIUS             0x2015
#define __SPRT_SPI_GETMESSAGEDURATION              0x2016
#define __SPRT_SPI_SETMESSAGEDURATION              0x2017
#define __SPRT_SPI_GETCONTACTVISUALIZATION         0x2018
#define __SPRT_SPI_SETCONTACTVISUALIZATION         0x2019
#define __SPRT_SPI_GETGESTUREVISUALIZATION         0x201A
#define __SPRT_SPI_SETGESTUREVISUALIZATION         0x201B
#define __SPRT_SPI_GETMOUSEWHEELROUTING            0x201C
#define __SPRT_SPI_SETMOUSEWHEELROUTING            0x201D
#define __SPRT_SPI_GETPENVISUALIZATION             0x201E
#define __SPRT_SPI_SETPENVISUALIZATION             0x201F

#define __SPRT_SW_HIDE             0
#define __SPRT_SW_SHOWNORMAL       1
#define __SPRT_SW_NORMAL           1
#define __SPRT_SW_SHOWMINIMIZED    2
#define __SPRT_SW_SHOWMAXIMIZED    3
#define __SPRT_SW_MAXIMIZE         3
#define __SPRT_SW_SHOWNOACTIVATE   4
#define __SPRT_SW_SHOW             5
#define __SPRT_SW_MINIMIZE         6
#define __SPRT_SW_SHOWMINNOACTIVE  7
#define __SPRT_SW_SHOWNA           8
#define __SPRT_SW_RESTORE          9
#define __SPRT_SW_SHOWDEFAULT      10
#define __SPRT_SW_FORCEMINIMIZE    11
#define __SPRT_SW_MAX              11

#define __SPRT_WA_INACTIVE     0
#define __SPRT_WA_ACTIVE       1
#define __SPRT_WA_CLICKACTIVE  2

#define __SPRT_GWL_WNDPROC         (-4)
#define __SPRT_GWL_HINSTANCE       (-6)
#define __SPRT_GWL_HWNDPARENT      (-8)
#define __SPRT_GWL_STYLE           (-16)
#define __SPRT_GWL_EXSTYLE         (-20)
#define __SPRT_GWL_USERDATA        (-21)
#define __SPRT_GWL_ID              (-12)

#define __SPRT_GWLP_WNDPROC        (-4)
#define __SPRT_GWLP_HINSTANCE      (-6)
#define __SPRT_GWLP_HWNDPARENT     (-8)
#define __SPRT_GWLP_USERDATA       (-21)
#define __SPRT_GWLP_ID             (-12)

#define __SPRT_WS_OVERLAPPED       0x00000000L
#define __SPRT_WS_POPUP            0x80000000L
#define __SPRT_WS_CHILD            0x40000000L
#define __SPRT_WS_MINIMIZE         0x20000000L
#define __SPRT_WS_VISIBLE          0x10000000L
#define __SPRT_WS_DISABLED         0x08000000L
#define __SPRT_WS_CLIPSIBLINGS     0x04000000L
#define __SPRT_WS_CLIPCHILDREN     0x02000000L
#define __SPRT_WS_MAXIMIZE         0x01000000L
#define __SPRT_WS_CAPTION          0x00C00000L
#define __SPRT_WS_BORDER           0x00800000L
#define __SPRT_WS_DLGFRAME         0x00400000L
#define __SPRT_WS_VSCROLL          0x00200000L
#define __SPRT_WS_HSCROLL          0x00100000L
#define __SPRT_WS_SYSMENU          0x00080000L
#define __SPRT_WS_THICKFRAME       0x00040000L
#define __SPRT_WS_GROUP            0x00020000L
#define __SPRT_WS_TABSTOP          0x00010000L
#define __SPRT_WS_MINIMIZEBOX      0x00020000L
#define __SPRT_WS_MAXIMIZEBOX      0x00010000L
#define __SPRT_WS_TILED            __SPRT_WS_OVERLAPPED
#define __SPRT_WS_ICONIC           __SPRT_WS_MINIMIZE
#define __SPRT_WS_SIZEBOX          __SPRT_WS_THICKFRAME
#define __SPRT_WS_OVERLAPPEDWINDOW (__SPRT_WS_OVERLAPPED     | \
                             __SPRT_WS_CAPTION        | \
                             __SPRT_WS_SYSMENU        | \
                             __SPRT_WS_THICKFRAME     | \
                             __SPRT_WS_MINIMIZEBOX    | \
                             __SPRT_WS_MAXIMIZEBOX)
#define __SPRT_WS_TILEDWINDOW      __SPRT_WS_OVERLAPPEDWINDOW

#define __SPRT_WS_POPUPWINDOW      (__SPRT_WS_POPUP          | \
                             __SPRT_WS_BORDER         | \
                             __SPRT_WS_SYSMENU)

#define __SPRT_WS_CHILDWINDOW      (__SPRT_WS_CHILD)

#define __SPRT_WS_EX_DLGMODALFRAME     0x00000001L
#define __SPRT_WS_EX_NOPARENTNOTIFY    0x00000004L
#define __SPRT_WS_EX_TOPMOST           0x00000008L
#define __SPRT_WS_EX_ACCEPTFILES       0x00000010L
#define __SPRT_WS_EX_TRANSPARENT       0x00000020L
#define __SPRT_WS_EX_MDICHILD          0x00000040L
#define __SPRT_WS_EX_TOOLWINDOW        0x00000080L
#define __SPRT_WS_EX_WINDOWEDGE        0x00000100L
#define __SPRT_WS_EX_CLIENTEDGE        0x00000200L
#define __SPRT_WS_EX_CONTEXTHELP       0x00000400L
#define __SPRT_WS_EX_RIGHT             0x00001000L
#define __SPRT_WS_EX_LEFT              0x00000000L
#define __SPRT_WS_EX_RTLREADING        0x00002000L
#define __SPRT_WS_EX_LTRREADING        0x00000000L
#define __SPRT_WS_EX_LEFTSCROLLBAR     0x00004000L
#define __SPRT_WS_EX_RIGHTSCROLLBAR    0x00000000L
#define __SPRT_WS_EX_CONTROLPARENT     0x00010000L
#define __SPRT_WS_EX_STATICEDGE        0x00020000L
#define __SPRT_WS_EX_APPWINDOW         0x00040000L
#define __SPRT_WS_EX_OVERLAPPEDWINDOW  (__SPRT_WS_EX_WINDOWEDGE | __SPRT_WS_EX_CLIENTEDGE)
#define __SPRT_WS_EX_PALETTEWINDOW     (__SPRT_WS_EX_WINDOWEDGE | __SPRT_WS_EX_TOOLWINDOW | __SPRT_WS_EX_TOPMOST)
#define __SPRT_WS_EX_LAYERED           0x00080000
#define __SPRT_WS_EX_NOINHERITLAYOUT   0x00100000L // Disable inheritence of mirroring by children
#define __SPRT_WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#define __SPRT_WS_EX_LAYOUTRTL         0x00400000L // Right to left mirroring
#define __SPRT_WS_EX_COMPOSITED        0x02000000L
#define __SPRT_WS_EX_NOACTIVATE        0x08000000L

#define __SPRT_CW_USEDEFAULT       ((int)0x80000000)

#define __SPRT_CS_VREDRAW          0x0001
#define __SPRT_CS_HREDRAW          0x0002
#define __SPRT_CS_DBLCLKS          0x0008
#define __SPRT_CS_OWNDC            0x0020
#define __SPRT_CS_CLASSDC          0x0040
#define __SPRT_CS_PARENTDC         0x0080
#define __SPRT_CS_NOCLOSE          0x0200
#define __SPRT_CS_SAVEBITS         0x0800
#define __SPRT_CS_BYTEALIGNCLIENT  0x1000
#define __SPRT_CS_BYTEALIGNWINDOW  0x2000
#define __SPRT_CS_GLOBALCLASS      0x4000

#define __SPRT_MAKEINTRESOURCE(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))

#define __SPRT_IDI_APPLICATION     __SPRT_MAKEINTRESOURCE(32512)
#define __SPRT_IDI_HAND            __SPRT_MAKEINTRESOURCE(32513)
#define __SPRT_IDI_QUESTION        __SPRT_MAKEINTRESOURCE(32514)
#define __SPRT_IDI_EXCLAMATION     __SPRT_MAKEINTRESOURCE(32515)
#define __SPRT_IDI_ASTERISK        __SPRT_MAKEINTRESOURCE(32516)

#define __SPRT_IMAGE_BITMAP        0
#define __SPRT_IMAGE_ICON          1
#define __SPRT_IMAGE_CURSOR        2
#define __SPRT_IMAGE_ENHMETAFILE   3

#define __SPRT_LR_DEFAULTCOLOR     0x00000000
#define __SPRT_LR_MONOCHROME       0x00000001
#define __SPRT_LR_COLOR            0x00000002
#define __SPRT_LR_COPYRETURNORG    0x00000004
#define __SPRT_LR_COPYDELETEORG    0x00000008
#define __SPRT_LR_LOADFROMFILE     0x00000010
#define __SPRT_LR_LOADTRANSPARENT  0x00000020
#define __SPRT_LR_DEFAULTSIZE      0x00000040
#define __SPRT_LR_VGACOLOR         0x00000080
#define __SPRT_LR_LOADMAP3DCOLORS  0x00001000
#define __SPRT_LR_CREATEDIBSECTION 0x00002000
#define __SPRT_LR_COPYFROMRESOURCE 0x00004000
#define __SPRT_LR_SHARED           0x00008000

#define __SPRT_IDC_ARROW           __SPRT_MAKEINTRESOURCE(32512)
#define __SPRT_IDC_IBEAM           __SPRT_MAKEINTRESOURCE(32513)
#define __SPRT_IDC_WAIT            __SPRT_MAKEINTRESOURCE(32514)
#define __SPRT_IDC_CROSS           __SPRT_MAKEINTRESOURCE(32515)
#define __SPRT_IDC_UPARROW         __SPRT_MAKEINTRESOURCE(32516)
#define __SPRT_IDC_SIZE            __SPRT_MAKEINTRESOURCE(32640)  /* OBSOLETE: use __SPRT_IDC_SIZEALL */
#define __SPRT_IDC_ICON            __SPRT_MAKEINTRESOURCE(32641)  /* OBSOLETE: use __SPRT_IDC_ARROW */
#define __SPRT_IDC_SIZENWSE        __SPRT_MAKEINTRESOURCE(32642)
#define __SPRT_IDC_SIZENESW        __SPRT_MAKEINTRESOURCE(32643)
#define __SPRT_IDC_SIZEWE          __SPRT_MAKEINTRESOURCE(32644)
#define __SPRT_IDC_SIZENS          __SPRT_MAKEINTRESOURCE(32645)
#define __SPRT_IDC_SIZEALL         __SPRT_MAKEINTRESOURCE(32646)
#define __SPRT_IDC_NO              __SPRT_MAKEINTRESOURCE(32648) /*not in win3.1 */
#define __SPRT_IDC_HAND            __SPRT_MAKEINTRESOURCE(32649)
#define __SPRT_IDC_APPSTARTING     __SPRT_MAKEINTRESOURCE(32650) /*not in win3.1 */
#define __SPRT_IDC_HELP            __SPRT_MAKEINTRESOURCE(32651)
#define __SPRT_IDC_PIN             __SPRT_MAKEINTRESOURCE(32671)
#define __SPRT_IDC_PERSON          __SPRT_MAKEINTRESOURCE(32672)

#define __SPRT_WHITE_BRUSH         0
#define __SPRT_LTGRAY_BRUSH        1
#define __SPRT_GRAY_BRUSH          2
#define __SPRT_DKGRAY_BRUSH        3
#define __SPRT_BLACK_BRUSH         4
#define __SPRT_NULL_BRUSH          5
#define __SPRT_HOLLOW_BRUSH        __SPRT_NULL_BRUSH
#define __SPRT_WHITE_PEN           6
#define __SPRT_BLACK_PEN           7
#define __SPRT_NULL_PEN            8
#define __SPRT_OEM_FIXED_FONT      10
#define __SPRT_ANSI_FIXED_FONT     11
#define __SPRT_ANSI_VAR_FONT       12
#define __SPRT_SYSTEM_FONT         13
#define __SPRT_DEVICE_DEFAULT_FONT 14
#define __SPRT_DEFAULT_PALETTE     15
#define __SPRT_SYSTEM_FIXED_FONT   16
#define __SPRT_DEFAULT_GUI_FONT    17

#define __SPRT_SWP_NONE            0x0000
#define __SPRT_SWP_NOSIZE          0x0001
#define __SPRT_SWP_NOMOVE          0x0002
#define __SPRT_SWP_NOZORDER        0x0004
#define __SPRT_SWP_NOREDRAW        0x0008
#define __SPRT_SWP_NOACTIVATE      0x0010
#define __SPRT_SWP_FRAMECHANGED    0x0020  /* The frame changed: send __SPRT_WM_NCCALCSIZE */
#define __SPRT_SWP_SHOWWINDOW      0x0040
#define __SPRT_SWP_HIDEWINDOW      0x0080
#define __SPRT_SWP_NOCOPYBITS      0x0100
#define __SPRT_SWP_NOOWNERZORDER   0x0200  /* Dont do owner Z ordering */
#define __SPRT_SWP_NOSENDCHANGING  0x0400  /* Dont send __SPRT_WM_WINDOWPOSCHANGING */
#define __SPRT_SWP_DRAWFRAME       __SPRT_SWP_FRAMECHANGED
#define __SPRT_SWP_NOREPOSITION    __SPRT_SWP_NOOWNERZORDER
#define __SPRT_SWP_DEFERERASE      0x2000 // same as SWP_DEFERDRAWING
#define __SPRT_SWP_ASYNCWINDOWPOS  0x4000 // same as SWP_CREATESPB

#define __SPRT_SC_SIZE         0xF000
#define __SPRT_SC_MOVE         0xF010
#define __SPRT_SC_MINIMIZE     0xF020
#define __SPRT_SC_MAXIMIZE     0xF030
#define __SPRT_SC_NEXTWINDOW   0xF040
#define __SPRT_SC_PREVWINDOW   0xF050
#define __SPRT_SC_CLOSE        0xF060
#define __SPRT_SC_VSCROLL      0xF070
#define __SPRT_SC_HSCROLL      0xF080
#define __SPRT_SC_MOUSEMENU    0xF090
#define __SPRT_SC_KEYMENU      0xF100
#define __SPRT_SC_ARRANGE      0xF110
#define __SPRT_SC_RESTORE      0xF120
#define __SPRT_SC_TASKLIST     0xF130
#define __SPRT_SC_SCREENSAVE   0xF140
#define __SPRT_SC_HOTKEY       0xF150
#define __SPRT_SC_DEFAULT      0xF160
#define __SPRT_SC_MONITORPOWER 0xF170
#define __SPRT_SC_CONTEXTHELP  0xF180
#define __SPRT_SC_SEPARATOR    0xF00F

#define __SPRT_MF_INSERT           0x00000000L
#define __SPRT_MF_CHANGE           0x00000080L
#define __SPRT_MF_APPEND           0x00000100L
#define __SPRT_MF_DELETE           0x00000200L
#define __SPRT_MF_REMOVE           0x00001000L
#define __SPRT_MF_BYCOMMAND        0x00000000L
#define __SPRT_MF_BYPOSITION       0x00000400L
#define __SPRT_MF_SEPARATOR        0x00000800L
#define __SPRT_MF_ENABLED          0x00000000L
#define __SPRT_MF_GRAYED           0x00000001L
#define __SPRT_MF_DISABLED         0x00000002L
#define __SPRT_MF_UNCHECKED        0x00000000L
#define __SPRT_MF_CHECKED          0x00000008L
#define __SPRT_MF_USECHECKBITMAPS  0x00000200L
#define __SPRT_MF_STRING           0x00000000L
#define __SPRT_MF_BITMAP           0x00000004L
#define __SPRT_MF_OWNERDRAW        0x00000100L
#define __SPRT_MF_POPUP            0x00000010L
#define __SPRT_MF_MENUBARBREAK     0x00000020L
#define __SPRT_MF_MENUBREAK        0x00000040L
#define __SPRT_MF_UNHILITE         0x00000000L
#define __SPRT_MF_HILITE           0x00000080L
#define __SPRT_MF_DEFAULT          0x00001000L
#define __SPRT_MF_SYSMENU          0x00002000L
#define __SPRT_MF_HELP             0x00004000L
#define __SPRT_MF_RIGHTJUSTIFY     0x00004000L
#define __SPRT_MF_MOUSESELECT      0x00008000L


#define __SPRT_MIIM_STATE       0x00000001
#define __SPRT_MIIM_ID          0x00000002
#define __SPRT_MIIM_SUBMENU     0x00000004
#define __SPRT_MIIM_CHECKMARKS  0x00000008
#define __SPRT_MIIM_TYPE        0x00000010
#define __SPRT_MIIM_DATA        0x00000020
#define __SPRT_MIIM_STRING      0x00000040
#define __SPRT_MIIM_BITMAP      0x00000080
#define __SPRT_MIIM_FTYPE       0x00000100

#define __SPRT_TPM_LEFTBUTTON  0x0000L
#define __SPRT_TPM_RIGHTBUTTON 0x0002L
#define __SPRT_TPM_LEFTALIGN   0x0000L
#define __SPRT_TPM_CENTERALIGN 0x0004L
#define __SPRT_TPM_RIGHTALIGN  0x0008L
#define __SPRT_TPM_TOPALIGN        0x0000L
#define __SPRT_TPM_VCENTERALIGN    0x0010L
#define __SPRT_TPM_BOTTOMALIGN     0x0020L

#define __SPRT_TPM_HORIZONTAL      0x0000L     /* Horz alignment matters more */
#define __SPRT_TPM_VERTICAL        0x0040L     /* Vert alignment matters more */
#define __SPRT_TPM_NONOTIFY        0x0080L     /* Dont send any notification msgs */
#define __SPRT_TPM_RETURNCMD       0x0100L
#define __SPRT_TPM_RECURSE         0x0001L
#define __SPRT_TPM_HORPOSANIMATION 0x0400L
#define __SPRT_TPM_HORNEGANIMATION 0x0800L
#define __SPRT_TPM_VERPOSANIMATION 0x1000L
#define __SPRT_TPM_VERNEGANIMATION 0x2000L
#define __SPRT_TPM_NOANIMATION     0x4000L
#define __SPRT_TPM_LAYOUTRTL       0x8000L
#define __SPRT_TPM_WORKAREA        0x10000L


#define __SPRT_HTERROR             (-2)
#define __SPRT_HTTRANSPARENT       (-1)
#define __SPRT_HTNOWHERE           0
#define __SPRT_HTCLIENT            1
#define __SPRT_HTCAPTION           2
#define __SPRT_HTSYSMENU           3
#define __SPRT_HTGROWBOX           4
#define __SPRT_HTSIZE              __SPRT_HTGROWBOX
#define __SPRT_HTMENU              5
#define __SPRT_HTHSCROLL           6
#define __SPRT_HTVSCROLL           7
#define __SPRT_HTMINBUTTON         8
#define __SPRT_HTMAXBUTTON         9
#define __SPRT_HTLEFT              10
#define __SPRT_HTRIGHT             11
#define __SPRT_HTTOP               12
#define __SPRT_HTTOPLEFT           13
#define __SPRT_HTTOPRIGHT          14
#define __SPRT_HTBOTTOM            15
#define __SPRT_HTBOTTOMLEFT        16
#define __SPRT_HTBOTTOMRIGHT       17
#define __SPRT_HTBORDER            18
#define __SPRT_HTREDUCE            __SPRT_HTMINBUTTON
#define __SPRT_HTZOOM              __SPRT_HTMAXBUTTON
#define __SPRT_HTSIZEFIRST         __SPRT_HTLEFT
#define __SPRT_HTSIZELAST          __SPRT_HTBOTTOMRIGHT
#define __SPRT_HTOBJECT            19
#define __SPRT_HTCLOSE             20
#define __SPRT_HTHELP              21

#define __SPRT_TME_HOVER       0x00000001
#define __SPRT_TME_LEAVE       0x00000002
#define __SPRT_TME_NONCLIENT   0x00000010
#define __SPRT_TME_QUERY       0x40000000
#define __SPRT_TME_CANCEL      0x80000000

#define __SPRT_HWND_TOP        ((HWND)0)
#define __SPRT_HWND_BOTTOM     ((HWND)1)
#define __SPRT_HWND_TOPMOST    ((HWND)-1)
#define __SPRT_HWND_NOTOPMOST  ((HWND)-2)

#define __SPRT_WMSZ_LEFT           1
#define __SPRT_WMSZ_RIGHT          2
#define __SPRT_WMSZ_TOP            3
#define __SPRT_WMSZ_TOPLEFT        4
#define __SPRT_WMSZ_TOPRIGHT       5
#define __SPRT_WMSZ_BOTTOM         6
#define __SPRT_WMSZ_BOTTOMLEFT     7
#define __SPRT_WMSZ_BOTTOMRIGHT    8

#define __SPRT_SIZE_RESTORED       0
#define __SPRT_SIZE_MINIMIZED      1
#define __SPRT_SIZE_MAXIMIZED      2
#define __SPRT_SIZE_MAXSHOW        3
#define __SPRT_SIZE_MAXHIDE        4

#define __SPRT_MAPVK_VK_TO_VSC     (0)
#define __SPRT_MAPVK_VSC_TO_VK     (1)
#define __SPRT_MAPVK_VK_TO_CHAR    (2)
#define __SPRT_MAPVK_VSC_TO_VK_EX  (3)
#define __SPRT_MAPVK_VK_TO_VSC_EX  (4)

#define __SPRT_WHEEL_DELTA                     120

#define __SPRT_MB_OK                       0x00000000L
#define __SPRT_MB_OKCANCEL                 0x00000001L
#define __SPRT_MB_ABORTRETRYIGNORE         0x00000002L
#define __SPRT_MB_YESNOCANCEL              0x00000003L
#define __SPRT_MB_YESNO                    0x00000004L
#define __SPRT_MB_RETRYCANCEL              0x00000005L
#define __SPRT_MB_CANCELTRYCONTINUE        0x00000006L
#define __SPRT_MB_ICONHAND                 0x00000010L
#define __SPRT_MB_ICONQUESTION             0x00000020L
#define __SPRT_MB_ICONEXCLAMATION          0x00000030L
#define __SPRT_MB_ICONASTERISK             0x00000040L
#define __SPRT_MB_USERICON                 0x00000080L
#define __SPRT_MB_ICONWARNING              __SPRT_MB_ICONEXCLAMATION
#define __SPRT_MB_ICONERROR                __SPRT_MB_ICONHAND
#define __SPRT_MB_ICONINFORMATION          __SPRT_MB_ICONASTERISK
#define __SPRT_MB_ICONSTOP                 __SPRT_MB_ICONHAND
#define __SPRT_MB_DEFBUTTON1               0x00000000L
#define __SPRT_MB_DEFBUTTON2               0x00000100L
#define __SPRT_MB_DEFBUTTON3               0x00000200L
#define __SPRT_MB_DEFBUTTON4               0x00000300L
#define __SPRT_MB_APPLMODAL                0x00000000L
#define __SPRT_MB_SYSTEMMODAL              0x00001000L
#define __SPRT_MB_TASKMODAL                0x00002000L
#define __SPRT_MB_HELP                     0x00004000L // Help Button
#define __SPRT_MB_NOFOCUS                  0x00008000L
#define __SPRT_MB_SETFOREGROUND            0x00010000L
#define __SPRT_MB_DEFAULT_DESKTOP_ONLY     0x00020000L
#define __SPRT_MB_TOPMOST                  0x00040000L
#define __SPRT_MB_RIGHT                    0x00080000L
#define __SPRT_MB_RTLREADING               0x00100000L
#define __SPRT_MB_SERVICE_NOTIFICATION          0x00200000L
#define __SPRT_MB_TYPEMASK                 0x0000000FL
#define __SPRT_MB_ICONMASK                 0x000000F0L
#define __SPRT_MB_DEFMASK                  0x00000F00L
#define __SPRT_MB_MODEMASK                 0x00003000L
#define __SPRT_MB_MISCMASK                 0x0000C000L

// clang-format on

typedef HANDLE HWND, HICON, HCURSOR, HBRUSH, HMENU, HBITMAP;

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSW {
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCWSTR lpszMenuName;
	LPCWSTR lpszClassName;
} WNDCLASSW, *PWNDCLASSW;

typedef struct tagSTYLESTRUCT {
	DWORD styleOld;
	DWORD styleNew;
} STYLESTRUCT, *LPSTYLESTRUCT;

typedef struct tagWINDOWPOS {
	HWND hwnd;
	HWND hwndInsertAfter;
	int x;
	int y;
	int cx;
	int cy;
	UINT flags;
} WINDOWPOS, *LPWINDOWPOS, *PWINDOWPOS;

typedef struct tagNCCALCSIZE_PARAMS {
	RECT rgrc[3];
	PWINDOWPOS lppos;
} NCCALCSIZE_PARAMS, *LPNCCALCSIZE_PARAMS;

typedef struct tagMINMAXINFO {
	POINT ptReserved;
	POINT ptMaxSize;
	POINT ptMaxPosition;
	POINT ptMinTrackSize;
	POINT ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;

typedef struct tagMENUITEMINFOW {
	UINT cbSize;
	UINT fMask;
	UINT fType; // used if MIIM_TYPE (4.0) or MIIM_FTYPE (>4.0)
	UINT fState; // used if MIIM_STATE
	UINT wID; // used if MIIM_ID
	HMENU hSubMenu; // used if MIIM_SUBMENU
	HBITMAP hbmpChecked; // used if MIIM_CHECKMARKS
	HBITMAP hbmpUnchecked; // used if MIIM_CHECKMARKS
	ULONG_PTR dwItemData; // used if MIIM_DATA
	LPWSTR dwTypeData; // used if MIIM_TYPE (4.0) or MIIM_STRING (>4.0)
	UINT cch; // used if MIIM_TYPE (4.0) or MIIM_STRING (>4.0)
	HBITMAP hbmpItem; // used if MIIM_BITMAP
} MENUITEMINFOW, *LPMENUITEMINFOW;

typedef MENUITEMINFOW const *LPCMENUITEMINFOW;

typedef struct tagWINDOWPLACEMENT {
	UINT length;
	UINT flags;
	UINT showCmd;
	POINT ptMinPosition;
	POINT ptMaxPosition;
	RECT rcNormalPosition;
	// NB: the SDK only declares a trailing `RECT rcDevice` under #ifdef _MAC; keeping it
	// out matches the Win32 layout (sizeof == 44) that callers pass as `length`.
} WINDOWPLACEMENT;
typedef WINDOWPLACEMENT *PWINDOWPLACEMENT, *LPWINDOWPLACEMENT;

typedef struct tagTRACKMOUSEEVENT {
	DWORD cbSize;
	DWORD dwFlags;
	HWND hwndTrack;
	DWORD dwHoverTime;
} TRACKMOUSEEVENT, *LPTRACKMOUSEEVENT;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_USER_API_H_
