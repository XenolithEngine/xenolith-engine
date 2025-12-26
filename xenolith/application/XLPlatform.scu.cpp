/**
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

#include "XLCommon.h" // IWYU pragma: keep

#include "platform/XLContextController.cc"
#include "platform/XLContextNativeWindow.cc"
#include "platform/XLDisplayConfigManager.cc"

#if LINUX
#include "linux/thirdparty/glfw/xkb_unicode.cc"
#include "linux/XLLinux.cc"
#include "linux/XLLinuxXkbLibrary.cc"
#include "linux/dbus/XLLinuxDBusLibrary.cc"
#include "linux/dbus/XLLinuxDBusController.cc"
#include "linux/dbus/XLLinuxDBusGnome.cc"
#include "linux/dbus/XLLinuxDBusKde.cc"
#include "linux/xcb/XLLinuxXcbLibrary.cc"
#include "linux/xcb/XLLinuxXcbSupportWindow.cc"
#include "linux/xcb/XLLinuxXcbConnection.cc"
#include "linux/xcb/XLLinuxXcbDisplayConfigManager.cc"
#include "linux/xcb/XLLinuxXcbWindow.cc"
#include "linux/wayland/XLLinuxWaylandProtocol.cc"
#include "linux/wayland/XLLinuxWaylandLibrary.cc"
#include "linux/wayland/XLLinuxWaylandSeat.cc"
#include "linux/wayland/XLLinuxWaylandDataDevice.cc"
#include "linux/wayland/XLLinuxWaylandDisplay.cc"
#include "linux/wayland/XLLinuxWaylandKdeDisplayConfigManager.cc"
#include "linux/wayland/XLLinuxWaylandWindow.cc"
#include "linux/XLLinuxContextController.cc"
#endif

#if WIN32
#include "windows/XLWindows.cc"
#include "windows/XLWindowsMessageWindow.cc"
#include "windows/XLWindowsWindowClass.cc"
#include "windows/XLWindowsWindow.cc"
#include "windows/XLWindowsDisplayConfigManager.cc"
#include "windows/XLWindowsContextController.cc"
#endif

#if ANDROID
#include "android/XLAndroid.cc"
#include "android/XLAndroidInput.cc"
#include "android/XLAndroidWindow.cc"
#include "android/XLAndroidActivity.cc"
#include "android/XLAndroidDisplayConfigManager.cc"
#include "android/XLAndroidNetworkConnectivity.cc"
#include "android/XLAndroidClipboardListener.cc"
#include "android/XLAndroidContextController.cc"
#endif
