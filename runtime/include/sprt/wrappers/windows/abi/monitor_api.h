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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_MONITOR_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_MONITOR_API_H_


#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>

// clang-format off
#define __SPRT_DPI_AWARENESS_CONTEXT_UNAWARE               ((DPI_AWARENESS_CONTEXT)-1)
#define __SPRT_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE          ((DPI_AWARENESS_CONTEXT)-2)
#define __SPRT_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE     ((DPI_AWARENESS_CONTEXT)-3)
#define __SPRT_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2  ((DPI_AWARENESS_CONTEXT)-4)
#define __SPRT_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED     ((DPI_AWARENESS_CONTEXT)-5)

#define __SPRT_MONITOR_DEFAULTTONULL       0x00000000
#define __SPRT_MONITOR_DEFAULTTOPRIMARY    0x00000001
#define __SPRT_MONITOR_DEFAULTTONEAREST    0x00000002
#define __SPRT_MONITORINFOF_PRIMARY        0x00000001

#define __SPRT_PHYSICAL_MONITOR_DESCRIPTION_SIZE                   128

#define __SPRT_CCHFORMNAME 32
#define __SPRT_CCHDEVICENAME 32

#define __SPRT_USER_DEFAULT_SCREEN_DPI 96

#define __SPRT_EDD_GET_DEVICE_INTERFACE_NAME 0x00000001

#define __SPRT_DISPLAY_DEVICE_ACTIVE              0x00000001
#define __SPRT_DISPLAY_DEVICE_ATTACHED            0x00000002

#define __SPRT_DRIVERVERSION 0     /* Device driver version                    */
#define __SPRT_TECHNOLOGY    2     /* Device classification                    */
#define __SPRT_HORZSIZE      4     /* Horizontal size in millimeters           */
#define __SPRT_VERTSIZE      6     /* Vertical size in millimeters             */
#define __SPRT_HORZRES       8     /* Horizontal width in pixels               */
#define __SPRT_VERTRES       10    /* Vertical height in pixels                */
#define __SPRT_BITSPIXEL     12    /* Number of bits per pixel                 */
#define __SPRT_PLANES        14    /* Number of planes                         */
#define __SPRT_NUMBRUSHES    16    /* Number of brushes the device has         */
#define __SPRT_NUMPENS       18    /* Number of pens the device has            */
#define __SPRT_NUMMARKERS    20    /* Number of markers the device has         */
#define __SPRT_NUMFONTS      22    /* Number of fonts the device has           */
#define __SPRT_NUMCOLORS     24    /* Number of colors the device supports     */
#define __SPRT_PDEVICESIZE   26    /* Size required for device descriptor      */
#define __SPRT_CURVECAPS     28    /* Curve capabilities                       */
#define __SPRT_LINECAPS      30    /* Line capabilities                        */
#define __SPRT_POLYGONALCAPS 32    /* Polygonal capabilities                   */
#define __SPRT_TEXTCAPS      34    /* Text capabilities                        */
#define __SPRT_CLIPCAPS      36    /* Clipping capabilities                    */
#define __SPRT_RASTERCAPS    38    /* Bitblt capabilities                      */
#define __SPRT_ASPECTX       40    /* Length of the X leg                      */
#define __SPRT_ASPECTY       42    /* Length of the Y leg                      */
#define __SPRT_ASPECTXY      44    /* Length of the hypotenuse                 */
#define __SPRT_LOGPIXELSX    88    /* Logical pixels/inch in X                 */
#define __SPRT_LOGPIXELSY    90    /* Logical pixels/inch in Y                 */
#define __SPRT_SIZEPALETTE  104    /* Number of entries in physical palette    */
#define __SPRT_NUMRESERVED  106    /* Number of reserved entries in palette    */
#define __SPRT_COLORRES     108    /* Actual color resolution                  */
#define __SPRT_PHYSICALWIDTH   110 /* Physical Width in device units           */
#define __SPRT_PHYSICALHEIGHT  111 /* Physical Height in device units          */
#define __SPRT_PHYSICALOFFSETX 112 /* Physical Printable Area x margin         */
#define __SPRT_PHYSICALOFFSETY 113 /* Physical Printable Area y margin         */
#define __SPRT_SCALINGFACTORX  114 /* Scaling factor x                         */
#define __SPRT_SCALINGFACTORY  115 /* Scaling factor y                         */
#define __SPRT_VREFRESH        116  /* Current vertical refresh rate of the    */
#define __SPRT_DESKTOPVERTRES  117  /* Vertical height of entire desktop in pixels */
#define __SPRT_DESKTOPHORZRES  118  /* Horizontal width of entire desktop in pixels */
#define __SPRT_BLTALIGNMENT    119  /* Preferred blt alignment                 */
#define __SPRT_SHADEBLENDCAPS  120  /* Shading and blending caps               */
#define __SPRT_COLORMGMTCAPS   121  /* Color Management caps                   */

#define __SPRT_ENUM_CURRENT_SETTINGS       ((DWORD)-1)
#define __SPRT_ENUM_REGISTRY_SETTINGS      ((DWORD)-2)

#define __SPRT_SM_XVIRTUALSCREEN       76
#define __SPRT_SM_YVIRTUALSCREEN       77
#define __SPRT_SM_CXVIRTUALSCREEN      78
#define __SPRT_SM_CYVIRTUALSCREEN      79
#define __SPRT_SM_CMONITORS            80
#define __SPRT_SM_SAMEDISPLAYFORMAT    81

#define __SPRT_DM_ORIENTATION          0x00000001L
#define __SPRT_DM_PAPERSIZE            0x00000002L
#define __SPRT_DM_PAPERLENGTH          0x00000004L
#define __SPRT_DM_PAPERWIDTH           0x00000008L
#define __SPRT_DM_SCALE                0x00000010L
#define __SPRT_DM_POSITION             0x00000020L
#define __SPRT_DM_NUP                  0x00000040L
#define __SPRT_DM_DISPLAYORIENTATION   0x00000080L
#define __SPRT_DM_COPIES               0x00000100L
#define __SPRT_DM_DEFAULTSOURCE        0x00000200L
#define __SPRT_DM_PRINTQUALITY         0x00000400L
#define __SPRT_DM_COLOR                0x00000800L
#define __SPRT_DM_DUPLEX               0x00001000L
#define __SPRT_DM_YRESOLUTION          0x00002000L
#define __SPRT_DM_TTOPTION             0x00004000L
#define __SPRT_DM_COLLATE              0x00008000L
#define __SPRT_DM_FORMNAME             0x00010000L
#define __SPRT_DM_LOGPIXELS            0x00020000L
#define __SPRT_DM_BITSPERPEL           0x00040000L
#define __SPRT_DM_PELSWIDTH            0x00080000L
#define __SPRT_DM_PELSHEIGHT           0x00100000L
#define __SPRT_DM_DISPLAYFLAGS         0x00200000L
#define __SPRT_DM_DISPLAYFREQUENCY     0x00400000L
#define __SPRT_DM_ICMMETHOD            0x00800000L
#define __SPRT_DM_ICMINTENT            0x01000000L
#define __SPRT_DM_MEDIATYPE            0x02000000L
#define __SPRT_DM_DITHERTYPE           0x04000000L
#define __SPRT_DM_PANNINGWIDTH         0x08000000L
#define __SPRT_DM_PANNINGHEIGHT        0x10000000L
#define __SPRT_DM_DISPLAYFIXEDOUTPUT   0x20000000L

#define __SPRT_DIREG_DEV       0x00000001
#define __SPRT_DIREG_DRV       0x00000002
#define __SPRT_DIREG_BOTH      0x00000004

#define __SPRT_DICS_FLAG_GLOBAL         0x00000001
#define __SPRT_DICS_FLAG_CONFIGSPECIFIC 0x00000002
#define __SPRT_DICS_FLAG_CONFIGGENERAL  0x00000004

#define __SPRT_DIGCF_DEFAULT           0x00000001
#define __SPRT_DIGCF_PRESENT           0x00000002
#define __SPRT_DIGCF_ALLCLASSES        0x00000004
#define __SPRT_DIGCF_PROFILE           0x00000008
#define __SPRT_DIGCF_DEVICEINTERFACE   0x00000010

#define __SPRT_CDS_UPDATEREGISTRY           0x00000001
#define __SPRT_CDS_TEST                     0x00000002
#define __SPRT_CDS_FULLSCREEN               0x00000004
#define __SPRT_CDS_GLOBAL                   0x00000008
#define __SPRT_CDS_SET_PRIMARY              0x00000010
#define __SPRT_CDS_VIDEOPARAMETERS          0x00000020
#define __SPRT_CDS_RESET                    0x40000000
#define __SPRT_CDS_RESET_EX                 0x20000000
#define __SPRT_CDS_NORESET                  0x10000000

#define __SPRT_DISP_CHANGE_SUCCESSFUL       0
#define __SPRT_DISP_CHANGE_RESTART          1
#define __SPRT_DISP_CHANGE_FAILED          -1
#define __SPRT_DISP_CHANGE_BADMODE         -2
#define __SPRT_DISP_CHANGE_NOTUPDATED      -3
#define __SPRT_DISP_CHANGE_BADFLAGS        -4
#define __SPRT_DISP_CHANGE_BADPARAM        -5
#define __SPRT_DISP_CHANGE_BADDUALVIEW     -6
// clang-format on

typedef enum PROCESS_DPI_AWARENESS {
	PROCESS_DPI_UNAWARE = 0,
	PROCESS_SYSTEM_DPI_AWARE = 1,
	PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE {
	MDT_EFFECTIVE_DPI = 0,
	MDT_ANGULAR_DPI = 1,
	MDT_RAW_DPI = 2,
	MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

typedef enum DPI_AWARENESS {
	DPI_AWARENESS_INVALID = -1,
	DPI_AWARENESS_UNAWARE = 0,
	DPI_AWARENESS_SYSTEM_AWARE = 1,
	DPI_AWARENESS_PER_MONITOR_AWARE = 2
} DPI_AWARENESS;

typedef HANDLE DPI_AWARENESS_CONTEXT, HMONITOR, HDC, HDEVINFO, HWND;

typedef struct _PHYSICAL_MONITOR {
	HANDLE hPhysicalMonitor;
	WCHAR szPhysicalMonitorDescription[__SPRT_PHYSICAL_MONITOR_DESCRIPTION_SIZE];
} PHYSICAL_MONITOR, *LPPHYSICAL_MONITOR;

typedef struct tagMONITORINFO {
	DWORD cbSize;
	RECT rcMonitor;
	RECT rcWork;
	DWORD dwFlags;
} MONITORINFO, *LPMONITORINFO;

typedef struct tagMONITORINFOEXW : public MONITORINFO {
	WCHAR szDevice[__SPRT_CCHDEVICENAME];
} MONITORINFOEXW, *LPMONITORINFOEXW;

typedef struct _DISPLAY_DEVICEW {
	DWORD cb;
	WCHAR DeviceName[32];
	WCHAR DeviceString[128];
	DWORD StateFlags;
	WCHAR DeviceID[128];
	WCHAR DeviceKey[128];
} DISPLAY_DEVICEW, *PDISPLAY_DEVICEW, *LPDISPLAY_DEVICEW;

typedef struct _devicemodeW {
	WCHAR dmDeviceName[__SPRT_CCHDEVICENAME];
	WORD dmSpecVersion;
	WORD dmDriverVersion;
	WORD dmSize;
	WORD dmDriverExtra;
	DWORD dmFields;
	union {
		/* printer only fields */
		struct {
			short dmOrientation;
			short dmPaperSize;
			short dmPaperLength;
			short dmPaperWidth;
			short dmScale;
			short dmCopies;
			short dmDefaultSource;
			short dmPrintQuality;
		};
		/* display only fields */
		struct {
			POINTL dmPosition;
			DWORD dmDisplayOrientation;
			DWORD dmDisplayFixedOutput;
		};
	};
	short dmColor;
	short dmDuplex;
	short dmYResolution;
	short dmTTOption;
	short dmCollate;
	WCHAR dmFormName[__SPRT_CCHFORMNAME];
	WORD dmLogPixels;
	DWORD dmBitsPerPel;
	DWORD dmPelsWidth;
	DWORD dmPelsHeight;
	union {
		DWORD dmDisplayFlags;
		DWORD dmNup;
	};
	DWORD dmDisplayFrequency;
	DWORD dmICMMethod;
	DWORD dmICMIntent;
	DWORD dmMediaType;
	DWORD dmDitherType;
	DWORD dmReserved1;
	DWORD dmReserved2;
	DWORD dmPanningWidth;
	DWORD dmPanningHeight;
} DEVMODEW, *PDEVMODEW, *NPDEVMODEW, *LPDEVMODEW;

typedef struct _SP_DEVINFO_DATA {
	DWORD cbSize;
	GUID ClassGuid;
	DWORD DevInst; // DEVINST handle
	ULONG_PTR Reserved;
} SP_DEVINFO_DATA, *PSP_DEVINFO_DATA;

typedef struct _SP_DEVICE_INTERFACE_DATA {
	DWORD cbSize;
	GUID InterfaceClassGuid;
	DWORD Flags;
	ULONG_PTR Reserved;
} SP_DEVICE_INTERFACE_DATA, *PSP_DEVICE_INTERFACE_DATA;

typedef struct _SP_DEVICE_INTERFACE_DETAIL_DATA_W {
	DWORD cbSize;
	WCHAR DevicePath[__SPRT_ANYSIZE_ARRAY];
} SP_DEVICE_INTERFACE_DETAIL_DATA_W, *PSP_DEVICE_INTERFACE_DETAIL_DATA_W;

typedef BOOL (*MONITORENUMPROC)(HMONITOR, HDC, LPRECT, LPARAM);


#endif // SPRT_WRAPPERS_WINDOWS_ABI_MONITOR_API_H_
