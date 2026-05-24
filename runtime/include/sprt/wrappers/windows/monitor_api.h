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

#ifndef SPRT_WRAPPERS_WINDOWS_MONITOR_API_H_
#define SPRT_WRAPPERS_WINDOWS_MONITOR_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>

// clang-format off
#define DPI_AWARENESS_CONTEXT_UNAWARE               ((DPI_AWARENESS_CONTEXT)-1)
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE          ((DPI_AWARENESS_CONTEXT)-2)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE     ((DPI_AWARENESS_CONTEXT)-3)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2  ((DPI_AWARENESS_CONTEXT)-4)
#define DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED     ((DPI_AWARENESS_CONTEXT)-5)

#define MONITOR_DEFAULTTONULL       0x00000000
#define MONITOR_DEFAULTTOPRIMARY    0x00000001
#define MONITOR_DEFAULTTONEAREST    0x00000002
#define MONITORINFOF_PRIMARY        0x00000001

#define PHYSICAL_MONITOR_DESCRIPTION_SIZE                   128

#define CCHFORMNAME 32
#define CCHDEVICENAME 32

#define USER_DEFAULT_SCREEN_DPI 96

#define EDD_GET_DEVICE_INTERFACE_NAME 0x00000001

#define DISPLAY_DEVICE_ACTIVE              0x00000001
#define DISPLAY_DEVICE_ATTACHED            0x00000002

#define DRIVERVERSION 0     /* Device driver version                    */
#define TECHNOLOGY    2     /* Device classification                    */
#define HORZSIZE      4     /* Horizontal size in millimeters           */
#define VERTSIZE      6     /* Vertical size in millimeters             */
#define HORZRES       8     /* Horizontal width in pixels               */
#define VERTRES       10    /* Vertical height in pixels                */
#define BITSPIXEL     12    /* Number of bits per pixel                 */
#define PLANES        14    /* Number of planes                         */
#define NUMBRUSHES    16    /* Number of brushes the device has         */
#define NUMPENS       18    /* Number of pens the device has            */
#define NUMMARKERS    20    /* Number of markers the device has         */
#define NUMFONTS      22    /* Number of fonts the device has           */
#define NUMCOLORS     24    /* Number of colors the device supports     */
#define PDEVICESIZE   26    /* Size required for device descriptor      */
#define CURVECAPS     28    /* Curve capabilities                       */
#define LINECAPS      30    /* Line capabilities                        */
#define POLYGONALCAPS 32    /* Polygonal capabilities                   */
#define TEXTCAPS      34    /* Text capabilities                        */
#define CLIPCAPS      36    /* Clipping capabilities                    */
#define RASTERCAPS    38    /* Bitblt capabilities                      */
#define ASPECTX       40    /* Length of the X leg                      */
#define ASPECTY       42    /* Length of the Y leg                      */
#define ASPECTXY      44    /* Length of the hypotenuse                 */
#define LOGPIXELSX    88    /* Logical pixels/inch in X                 */
#define LOGPIXELSY    90    /* Logical pixels/inch in Y                 */
#define SIZEPALETTE  104    /* Number of entries in physical palette    */
#define NUMRESERVED  106    /* Number of reserved entries in palette    */
#define COLORRES     108    /* Actual color resolution                  */
#define PHYSICALWIDTH   110 /* Physical Width in device units           */
#define PHYSICALHEIGHT  111 /* Physical Height in device units          */
#define PHYSICALOFFSETX 112 /* Physical Printable Area x margin         */
#define PHYSICALOFFSETY 113 /* Physical Printable Area y margin         */
#define SCALINGFACTORX  114 /* Scaling factor x                         */
#define SCALINGFACTORY  115 /* Scaling factor y                         */
#define VREFRESH        116  /* Current vertical refresh rate of the    */
#define DESKTOPVERTRES  117  /* Horizontal width of entire desktop in   */                              */
#define DESKTOPHORZRES  118  /* Vertical height of entire desktop in    */                                */
#define BLTALIGNMENT    119  /* Preferred blt alignment                 */
#define SHADEBLENDCAPS  120  /* Shading and blending caps               */
#define COLORMGMTCAPS   121  /* Color Management caps                   */

#define ENUM_CURRENT_SETTINGS       ((DWORD)-1)
#define ENUM_REGISTRY_SETTINGS      ((DWORD)-2)

#define SM_XVIRTUALSCREEN       76
#define SM_YVIRTUALSCREEN       77
#define SM_CXVIRTUALSCREEN      78
#define SM_CYVIRTUALSCREEN      79
#define SM_CMONITORS            80
#define SM_SAMEDISPLAYFORMAT    81

#define DM_ORIENTATION          0x00000001L
#define DM_PAPERSIZE            0x00000002L
#define DM_PAPERLENGTH          0x00000004L
#define DM_PAPERWIDTH           0x00000008L
#define DM_SCALE                0x00000010L
#define DM_POSITION             0x00000020L
#define DM_NUP                  0x00000040L
#define DM_DISPLAYORIENTATION   0x00000080L
#define DM_COPIES               0x00000100L
#define DM_DEFAULTSOURCE        0x00000200L
#define DM_PRINTQUALITY         0x00000400L
#define DM_COLOR                0x00000800L
#define DM_DUPLEX               0x00001000L
#define DM_YRESOLUTION          0x00002000L
#define DM_TTOPTION             0x00004000L
#define DM_COLLATE              0x00008000L
#define DM_FORMNAME             0x00010000L
#define DM_LOGPIXELS            0x00020000L
#define DM_BITSPERPEL           0x00040000L
#define DM_PELSWIDTH            0x00080000L
#define DM_PELSHEIGHT           0x00100000L
#define DM_DISPLAYFLAGS         0x00200000L
#define DM_DISPLAYFREQUENCY     0x00400000L
#define DM_ICMMETHOD            0x00800000L
#define DM_ICMINTENT            0x01000000L
#define DM_MEDIATYPE            0x02000000L
#define DM_DITHERTYPE           0x04000000L
#define DM_PANNINGWIDTH         0x08000000L
#define DM_PANNINGHEIGHT        0x10000000L
#define DM_DISPLAYFIXEDOUTPUT   0x20000000L

#define DIREG_DEV       0x00000001
#define DIREG_DRV       0x00000002
#define DIREG_BOTH      0x00000004

#define DICS_FLAG_GLOBAL         0x00000001
#define DICS_FLAG_CONFIGSPECIFIC 0x00000002
#define DICS_FLAG_CONFIGGENERAL  0x00000004

#define DIGCF_DEFAULT           0x00000001
#define DIGCF_PRESENT           0x00000002
#define DIGCF_ALLCLASSES        0x00000004
#define DIGCF_PROFILE           0x00000008
#define DIGCF_DEVICEINTERFACE   0x00000010

#define CDS_UPDATEREGISTRY           0x00000001
#define CDS_TEST                     0x00000002
#define CDS_FULLSCREEN               0x00000004
#define CDS_GLOBAL                   0x00000008
#define CDS_SET_PRIMARY              0x00000010
#define CDS_VIDEOPARAMETERS          0x00000020
#define CDS_RESET                    0x40000000
#define CDS_RESET_EX                 0x20000000
#define CDS_NORESET                  0x10000000

#define DISP_CHANGE_SUCCESSFUL       0
#define DISP_CHANGE_RESTART          1
#define DISP_CHANGE_FAILED          -1
#define DISP_CHANGE_BADMODE         -2
#define DISP_CHANGE_NOTUPDATED      -3
#define DISP_CHANGE_BADFLAGS        -4
#define DISP_CHANGE_BADPARAM        -5
#define DISP_CHANGE_BADDUALVIEW     -6
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
	WCHAR szPhysicalMonitorDescription[PHYSICAL_MONITOR_DESCRIPTION_SIZE];
} PHYSICAL_MONITOR, *LPPHYSICAL_MONITOR;

typedef struct tagMONITORINFO {
	DWORD cbSize;
	RECT rcMonitor;
	RECT rcWork;
	DWORD dwFlags;
} MONITORINFO, *LPMONITORINFO;

typedef struct tagMONITORINFOEXW : public MONITORINFO {
	WCHAR szDevice[CCHDEVICENAME];
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
	WCHAR dmDeviceName[CCHDEVICENAME];
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
	WCHAR dmFormName[CCHFORMNAME];
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
	WCHAR DevicePath[ANYSIZE_ARRAY];
} SP_DEVICE_INTERFACE_DETAIL_DATA_W, *PSP_DEVICE_INTERFACE_DETAIL_DATA_W;

typedef BOOL (*MONITORENUMPROC)(HMONITOR, HDC, LPRECT, LPARAM);

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI BOOL GetMonitorInfoW(HMONITOR hMonitor, LPMONITORINFO lpmi);

__SPRT_WIN_IMPORT WINAPI BOOL GetNumberOfPhysicalMonitorsFromHMONITOR(HMONITOR hMonitor,
		LPDWORD pdwNumberOfPhysicalMonitors);

__SPRT_WIN_IMPORT WINAPI BOOL GetPhysicalMonitorsFromHMONITOR(HMONITOR hMonitor,
		DWORD dwPhysicalMonitorArraySize, LPPHYSICAL_MONITOR pPhysicalMonitorArray);

__SPRT_WIN_IMPORT WINAPI HRESULT SetProcessDpiAwareness(PROCESS_DPI_AWARENESS value);

__SPRT_WIN_IMPORT WINAPI HRESULT GetProcessDpiAwareness(HANDLE hprocess,
		PROCESS_DPI_AWARENESS *value);

__SPRT_WIN_IMPORT WINAPI HRESULT GetDpiForMonitor(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType,
		UINT *dpiX, UINT *dpiY);

__SPRT_WIN_IMPORT WINAPI BOOL EnumDisplayDevicesW(LPCWSTR lpDevice, DWORD iDevNum,
		PDISPLAY_DEVICEW lpDisplayDevice, DWORD dwFlags);

__SPRT_WIN_IMPORT WINAPI HDC CreateDCW(LPCWSTR pwszDriver, LPCWSTR pwszDevice, LPCWSTR pszPort,
		const DEVMODEW *pdm);

__SPRT_WIN_IMPORT WINAPI int GetDeviceCaps(HDC hdc, int index);

__SPRT_WIN_IMPORT WINAPI BOOL EnumDisplayMonitors(HDC hdc, LPCRECT lprcClip,
		MONITORENUMPROC lpfnEnum, LPARAM dwData);

__SPRT_WIN_IMPORT WINAPI BOOL DeleteDC(HDC hdc);

__SPRT_WIN_IMPORT WINAPI BOOL EnumDisplaySettingsW(LPCWSTR lpszDeviceName, DWORD iModeNum,
		DEVMODEW *lpDevMode);

__SPRT_WIN_IMPORT WINAPI LONG ChangeDisplaySettingsExW(LPCWSTR lpszDeviceName, DEVMODEW *lpDevMode,
		HWND hwnd, DWORD dwflags, LPVOID lParam);

__SPRT_WIN_IMPORT WINAPI BOOL DestroyPhysicalMonitor(HANDLE hMonitor);

__SPRT_WIN_IMPORT WINAPI BOOL DestroyPhysicalMonitors(DWORD dwPhysicalMonitorArraySize,
		LPPHYSICAL_MONITOR pPhysicalMonitorArray);

__SPRT_WIN_IMPORT WINAPI BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT value);

__SPRT_WIN_IMPORT WINAPI int GetSystemMetricsForDpi(int nIndex, UINT dpi);

__SPRT_WIN_IMPORT WINAPI HKEY SetupDiOpenDevRegKey(HDEVINFO DeviceInfoSet,
		PSP_DEVINFO_DATA DeviceInfoData, DWORD Scope, DWORD HwProfile, DWORD KeyType,
		REGSAM samDesired);

__SPRT_WIN_IMPORT WINAPI HDEVINFO SetupDiGetClassDevsExW(const GUID *ClassGuid, PCWSTR Enumerator,
		HWND hwndParent, DWORD Flags, HDEVINFO DeviceInfoSet, PCWSTR MachineName, PVOID Reserved);

__SPRT_WIN_IMPORT WINAPI BOOL SetupDiEnumDeviceInterfaces(HDEVINFO DeviceInfoSet,
		PSP_DEVINFO_DATA DeviceInfoData, const GUID *InterfaceClassGuid, DWORD MemberIndex,
		PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData);

__SPRT_WIN_IMPORT WINAPI BOOL SetupDiGetDeviceInterfaceDetailW(HDEVINFO DeviceInfoSet,
		PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
		PSP_DEVICE_INTERFACE_DETAIL_DATA_W DeviceInterfaceDetailData,
		DWORD DeviceInterfaceDetailDataSize, PDWORD RequiredSize, PSP_DEVINFO_DATA DeviceInfoData);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_MONITOR_API_H_
