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

#include "XLWindowsMessageWindow.h"

#include <uxtheme.h>
#include <winrt/windows.foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/windows.ui.viewmanagement.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.Streams.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

namespace {

enum class ConnectivityType {
	Disconnected,
	Normal,
	Local,
	CaptivePortal,
};

static StringView getUiAction(DWORD key) {
	switch (key) {
	case SPI_GETACCESSTIMEOUT: return StringView("SPI_GETACCESSTIMEOUT"); break;
	case SPI_GETAUDIODESCRIPTION: return StringView("SPI_GETAUDIODESCRIPTION"); break;
	case SPI_GETCLIENTAREAANIMATION: return StringView("SPI_GETCLIENTAREAANIMATION"); break;
	case SPI_GETDISABLEOVERLAPPEDCONTENT:
		return StringView("SPI_GETDISABLEOVERLAPPEDCONTENT");
		break;
	case SPI_GETFILTERKEYS: return StringView("SPI_GETFILTERKEYS"); break;
	case SPI_GETFOCUSBORDERHEIGHT: return StringView("SPI_GETFOCUSBORDERHEIGHT"); break;
	case SPI_GETFOCUSBORDERWIDTH: return StringView("SPI_GETFOCUSBORDERWIDTH"); break;
	case SPI_GETHIGHCONTRAST: return StringView("SPI_GETHIGHCONTRAST"); break;
	case SPI_GETLOGICALDPIOVERRIDE: return StringView("SPI_GETLOGICALDPIOVERRIDE"); break;
	case SPI_GETMESSAGEDURATION: return StringView("SPI_GETMESSAGEDURATION"); break;
	case SPI_GETMOUSECLICKLOCK: return StringView("SPI_GETMOUSECLICKLOCK"); break;
	case SPI_GETMOUSECLICKLOCKTIME: return StringView("SPI_GETMOUSECLICKLOCKTIME"); break;
	case SPI_GETMOUSEKEYS: return StringView("SPI_GETMOUSEKEYS"); break;
	case SPI_GETMOUSESONAR: return StringView("SPI_GETMOUSESONAR"); break;
	case SPI_GETMOUSEVANISH: return StringView("SPI_GETMOUSEVANISH"); break;
	case SPI_GETSCREENREADER: return StringView("SPI_GETSCREENREADER"); break;
	case SPI_GETSERIALKEYS: return StringView("SPI_GETSERIALKEYS"); break;
	case SPI_GETSHOWSOUNDS: return StringView("SPI_GETSHOWSOUNDS"); break;
	case SPI_GETSOUNDSENTRY: return StringView("SPI_GETSOUNDSENTRY"); break;
	case SPI_GETSTICKYKEYS: return StringView("SPI_GETSTICKYKEYS"); break;
	case SPI_GETTOGGLEKEYS: return StringView("SPI_GETTOGGLEKEYS"); break;
	case SPI_SETACCESSTIMEOUT: return StringView("SPI_SETACCESSTIMEOUT"); break;
	case SPI_SETAUDIODESCRIPTION: return StringView("SPI_SETAUDIODESCRIPTION"); break;
	case SPI_SETCLIENTAREAANIMATION: return StringView("SPI_SETCLIENTAREAANIMATION"); break;
	case SPI_SETDISABLEOVERLAPPEDCONTENT:
		return StringView("SPI_SETDISABLEOVERLAPPEDCONTENT");
		break;
	case SPI_SETFILTERKEYS: return StringView("SPI_SETFILTERKEYS"); break;
	case SPI_SETFOCUSBORDERHEIGHT: return StringView("SPI_SETFOCUSBORDERHEIGHT"); break;
	case SPI_SETFOCUSBORDERWIDTH: return StringView("SPI_SETFOCUSBORDERWIDTH"); break;
	case SPI_SETHIGHCONTRAST: return StringView("SPI_SETHIGHCONTRAST"); break;
	case SPI_SETLOGICALDPIOVERRIDE: return StringView("SPI_SETLOGICALDPIOVERRIDE"); break;
	case SPI_SETMESSAGEDURATION: return StringView("SPI_SETMESSAGEDURATION"); break;
	case SPI_SETMOUSECLICKLOCK: return StringView("SPI_SETMOUSECLICKLOCK"); break;
	case SPI_SETMOUSECLICKLOCKTIME: return StringView("SPI_SETMOUSECLICKLOCKTIME"); break;
	case SPI_SETMOUSEKEYS: return StringView("SPI_SETMOUSEKEYS"); break;
	case SPI_SETMOUSESONAR: return StringView("SPI_SETMOUSESONAR"); break;
	case SPI_SETMOUSEVANISH: return StringView("SPI_SETMOUSEVANISH"); break;
	case SPI_SETSCREENREADER: return StringView("SPI_SETSCREENREADER"); break;
	case SPI_SETSERIALKEYS: return StringView("SPI_SETSERIALKEYS"); break;
	case SPI_SETSHOWSOUNDS: return StringView("SPI_SETSHOWSOUNDS"); break;
	case SPI_SETSOUNDSENTRY: return StringView("SPI_SETSOUNDSENTRY"); break;
	case SPI_SETSTICKYKEYS: return StringView("SPI_SETSTICKYKEYS"); break;
	case SPI_SETTOGGLEKEYS: return StringView("SPI_SETTOGGLEKEYS"); break;
	case SPI_GETCLEARTYPE: return StringView("SPI_GETCLEARTYPE"); break;
	case SPI_GETDESKWALLPAPER: return StringView("SPI_GETDESKWALLPAPER"); break;
	case SPI_GETDROPSHADOW: return StringView("SPI_GETDROPSHADOW"); break;
	case SPI_GETFLATMENU: return StringView("SPI_GETFLATMENU"); break;
	case SPI_GETFONTSMOOTHING: return StringView("SPI_GETFONTSMOOTHING"); break;
	case SPI_GETFONTSMOOTHINGCONTRAST: return StringView("SPI_GETFONTSMOOTHINGCONTRAST"); break;
	case SPI_GETFONTSMOOTHINGORIENTATION:
		return StringView("SPI_GETFONTSMOOTHINGORIENTATION");
		break;
	case SPI_GETFONTSMOOTHINGTYPE: return StringView("SPI_GETFONTSMOOTHINGTYPE"); break;
	case SPI_GETWORKAREA: return StringView("SPI_GETWORKAREA"); break;
	case SPI_SETCLEARTYPE: return StringView("SPI_SETCLEARTYPE"); break;
	case SPI_SETCURSORS: return StringView("SPI_SETCURSORS"); break;
	case SPI_SETDESKPATTERN: return StringView("SPI_SETDESKPATTERN"); break;
	case SPI_SETDESKWALLPAPER: return StringView("SPI_SETDESKWALLPAPER"); break;
	case SPI_SETDROPSHADOW: return StringView("SPI_SETDROPSHADOW"); break;
	case SPI_SETFLATMENU: return StringView("SPI_SETFLATMENU"); break;
	case SPI_SETFONTSMOOTHING: return StringView("SPI_SETFONTSMOOTHING"); break;
	case SPI_SETFONTSMOOTHINGCONTRAST: return StringView("SPI_SETFONTSMOOTHINGCONTRAST"); break;
	case SPI_SETFONTSMOOTHINGTYPE: return StringView("SPI_SETFONTSMOOTHINGTYPE"); break;
	case SPI_SETFONTSMOOTHINGORIENTATION:
		return StringView("SPI_SETFONTSMOOTHINGORIENTATION");
		break;
	case SPI_SETWORKAREA: return StringView("SPI_SETWORKAREA"); break;
	case SPI_GETICONMETRICS: return StringView("SPI_GETICONMETRICS"); break;
	case SPI_GETICONTITLELOGFONT: return StringView("SPI_GETICONTITLELOGFONT"); break;
	case SPI_GETICONTITLEWRAP: return StringView("SPI_GETICONTITLEWRAP"); break;
	case SPI_ICONHORIZONTALSPACING: return StringView("SPI_ICONHORIZONTALSPACING"); break;
	case SPI_ICONVERTICALSPACING: return StringView("SPI_ICONVERTICALSPACING"); break;
	case SPI_SETICONMETRICS: return StringView("SPI_SETICONMETRICS"); break;
	case SPI_SETICONS: return StringView("SPI_SETICONS"); break;
	case SPI_SETICONTITLELOGFONT: return StringView("SPI_SETICONTITLELOGFONT"); break;
	case SPI_SETICONTITLEWRAP: return StringView("SPI_SETICONTITLEWRAP"); break;
	case SPI_GETBEEP: return StringView("SPI_GETBEEP"); break;
	case SPI_GETBLOCKSENDINPUTRESETS: return StringView("SPI_GETBLOCKSENDINPUTRESETS"); break;
	case SPI_GETCONTACTVISUALIZATION: return StringView("SPI_GETCONTACTVISUALIZATION"); break;
	case SPI_GETDEFAULTINPUTLANG: return StringView("SPI_GETDEFAULTINPUTLANG"); break;
	case SPI_GETGESTUREVISUALIZATION: return StringView("SPI_GETGESTUREVISUALIZATION"); break;
	case SPI_GETKEYBOARDCUES: return StringView("SPI_GETKEYBOARDCUES"); break;
	case SPI_GETKEYBOARDDELAY: return StringView("SPI_GETKEYBOARDDELAY"); break;
	case SPI_GETKEYBOARDPREF: return StringView("SPI_GETKEYBOARDPREF"); break;
	case SPI_GETKEYBOARDSPEED: return StringView("SPI_GETKEYBOARDSPEED"); break;
	case SPI_GETMOUSE: return StringView("SPI_GETMOUSE"); break;
	case SPI_GETMOUSEHOVERHEIGHT: return StringView("SPI_GETMOUSEHOVERHEIGHT"); break;
	case SPI_GETMOUSEHOVERTIME: return StringView("SPI_GETMOUSEHOVERTIME"); break;
	case SPI_GETMOUSEHOVERWIDTH: return StringView("SPI_GETMOUSEHOVERWIDTH"); break;
	case SPI_GETMOUSESPEED: return StringView("SPI_GETMOUSESPEED"); break;
	case SPI_GETMOUSETRAILS: return StringView("SPI_GETMOUSETRAILS"); break;
	case SPI_GETMOUSEWHEELROUTING: return StringView("SPI_GETMOUSEWHEELROUTING"); break;
	case SPI_GETPENVISUALIZATION: return StringView("SPI_GETPENVISUALIZATION"); break;
	case SPI_GETSNAPTODEFBUTTON: return StringView("SPI_GETSNAPTODEFBUTTON"); break;
	case SPI_GETSYSTEMLANGUAGEBAR: return StringView("SPI_GETSYSTEMLANGUAGEBAR"); break;
	case SPI_GETTHREADLOCALINPUTSETTINGS:
		return StringView("SPI_GETTHREADLOCALINPUTSETTINGS");
		break;
	case SPI_GETTOUCHPADPARAMETERS: return StringView("SPI_GETTOUCHPADPARAMETERS"); break;
	case SPI_GETWHEELSCROLLCHARS: return StringView("SPI_GETWHEELSCROLLCHARS"); break;
	case SPI_GETWHEELSCROLLLINES: return StringView("SPI_GETWHEELSCROLLLINES"); break;
	case SPI_SETBEEP: return StringView("SPI_SETBEEP"); break;
	case SPI_SETBLOCKSENDINPUTRESETS: return StringView("SPI_SETBLOCKSENDINPUTRESETS"); break;
	case SPI_SETCONTACTVISUALIZATION: return StringView("SPI_SETCONTACTVISUALIZATION"); break;
	case SPI_SETDEFAULTINPUTLANG: return StringView("SPI_SETDEFAULTINPUTLANG"); break;
	case SPI_SETDOUBLECLICKTIME: return StringView("SPI_SETDOUBLECLICKTIME"); break;
	case SPI_SETDOUBLECLKHEIGHT: return StringView("SPI_SETDOUBLECLKHEIGHT"); break;
	case SPI_SETDOUBLECLKWIDTH: return StringView("SPI_SETDOUBLECLKWIDTH"); break;
	case SPI_SETGESTUREVISUALIZATION: return StringView("SPI_SETGESTUREVISUALIZATION"); break;
	case SPI_SETKEYBOARDCUES: return StringView("SPI_SETKEYBOARDCUES"); break;
	case SPI_SETKEYBOARDDELAY: return StringView("SPI_SETKEYBOARDDELAY"); break;
	case SPI_SETKEYBOARDPREF: return StringView("SPI_SETKEYBOARDPREF"); break;
	case SPI_SETKEYBOARDSPEED: return StringView("SPI_SETKEYBOARDSPEED"); break;
	case SPI_SETLANGTOGGLE: return StringView("SPI_SETLANGTOGGLE"); break;
	case SPI_SETMOUSE: return StringView("SPI_SETMOUSE"); break;
	case SPI_SETMOUSEBUTTONSWAP: return StringView("SPI_SETMOUSEBUTTONSWAP"); break;
	case SPI_SETMOUSEHOVERHEIGHT: return StringView("SPI_SETMOUSEHOVERHEIGHT"); break;
	case SPI_SETMOUSEHOVERTIME: return StringView("SPI_SETMOUSEHOVERTIME"); break;
	case SPI_SETMOUSEHOVERWIDTH: return StringView("SPI_SETMOUSEHOVERWIDTH"); break;
	case SPI_SETMOUSESPEED: return StringView("SPI_SETMOUSESPEED"); break;
	case SPI_SETMOUSETRAILS: return StringView("SPI_SETMOUSETRAILS"); break;
	case SPI_SETMOUSEWHEELROUTING: return StringView("SPI_SETMOUSEWHEELROUTING"); break;
	case SPI_SETPENVISUALIZATION: return StringView("SPI_SETPENVISUALIZATION"); break;
	case SPI_SETSNAPTODEFBUTTON: return StringView("SPI_SETSNAPTODEFBUTTON"); break;
	case SPI_SETSYSTEMLANGUAGEBAR: return StringView("SPI_SETSYSTEMLANGUAGEBAR"); break;
	case SPI_SETTHREADLOCALINPUTSETTINGS:
		return StringView("SPI_SETTHREADLOCALINPUTSETTINGS");
		break;
	case SPI_SETTOUCHPADPARAMETERS: return StringView("SPI_SETTOUCHPADPARAMETERS"); break;
	case SPI_SETWHEELSCROLLCHARS: return StringView("SPI_SETWHEELSCROLLCHARS"); break;
	case SPI_SETWHEELSCROLLLINES: return StringView("SPI_SETWHEELSCROLLLINES"); break;
	case SPI_GETMENUDROPALIGNMENT: return StringView("SPI_GETMENUDROPALIGNMENT"); break;
	case SPI_GETMENUFADE: return StringView("SPI_GETMENUFADE"); break;
	case SPI_GETMENUSHOWDELAY: return StringView("SPI_GETMENUSHOWDELAY"); break;
	case SPI_SETMENUDROPALIGNMENT: return StringView("SPI_SETMENUDROPALIGNMENT"); break;
	case SPI_SETMENUFADE: return StringView("SPI_SETMENUFADE"); break;
	case SPI_SETMENUSHOWDELAY: return StringView("SPI_SETMENUSHOWDELAY"); break;
	case SPI_GETLOWPOWERACTIVE: return StringView("SPI_GETLOWPOWERACTIVE"); break;
	case SPI_GETLOWPOWERTIMEOUT: return StringView("SPI_GETLOWPOWERTIMEOUT"); break;
	case SPI_GETPOWEROFFACTIVE: return StringView("SPI_GETPOWEROFFACTIVE"); break;
	case SPI_GETPOWEROFFTIMEOUT: return StringView("SPI_GETPOWEROFFTIMEOUT"); break;
	case SPI_SETLOWPOWERACTIVE: return StringView("SPI_SETLOWPOWERACTIVE"); break;
	case SPI_SETLOWPOWERTIMEOUT: return StringView("SPI_SETLOWPOWERTIMEOUT"); break;
	case SPI_SETPOWEROFFACTIVE: return StringView("SPI_SETPOWEROFFACTIVE"); break;
	case SPI_SETPOWEROFFTIMEOUT: return StringView("SPI_SETPOWEROFFTIMEOUT"); break;
	case SPI_GETSCREENSAVEACTIVE: return StringView("SPI_GETSCREENSAVEACTIVE"); break;
	case SPI_GETSCREENSAVERRUNNING: return StringView("SPI_GETSCREENSAVERRUNNING"); break;
	case SPI_GETSCREENSAVESECURE: return StringView("SPI_GETSCREENSAVESECURE"); break;
	case SPI_GETSCREENSAVETIMEOUT: return StringView("SPI_GETSCREENSAVETIMEOUT"); break;
	case SPI_SETSCREENSAVEACTIVE: return StringView("SPI_SETSCREENSAVEACTIVE"); break;
	case SPI_SETSCREENSAVESECURE: return StringView("SPI_SETSCREENSAVESECURE"); break;
	case SPI_SETSCREENSAVETIMEOUT: return StringView("SPI_SETSCREENSAVETIMEOUT"); break;
	case SPI_GETHUNGAPPTIMEOUT: return StringView("SPI_GETHUNGAPPTIMEOUT"); break;
	case SPI_GETWAITTOKILLTIMEOUT: return StringView("SPI_GETWAITTOKILLTIMEOUT"); break;
	case SPI_GETWAITTOKILLSERVICETIMEOUT:
		return StringView("SPI_GETWAITTOKILLSERVICETIMEOUT");
		break;
	case SPI_SETHUNGAPPTIMEOUT: return StringView("SPI_SETHUNGAPPTIMEOUT"); break;
	case SPI_SETWAITTOKILLTIMEOUT: return StringView("SPI_SETWAITTOKILLTIMEOUT"); break;
	case SPI_SETWAITTOKILLSERVICETIMEOUT:
		return StringView("SPI_SETWAITTOKILLSERVICETIMEOUT");
		break;
	case SPI_GETCOMBOBOXANIMATION: return StringView("SPI_GETCOMBOBOXANIMATION"); break;
	case SPI_GETCURSORSHADOW: return StringView("SPI_GETCURSORSHADOW"); break;
	case SPI_GETGRADIENTCAPTIONS: return StringView("SPI_GETGRADIENTCAPTIONS"); break;
	case SPI_GETHOTTRACKING: return StringView("SPI_GETHOTTRACKING"); break;
	case SPI_GETLISTBOXSMOOTHSCROLLING: return StringView("SPI_GETLISTBOXSMOOTHSCROLLING"); break;
	case SPI_GETMENUANIMATION: return StringView("SPI_GETMENUANIMATION"); break;
	case SPI_GETSELECTIONFADE: return StringView("SPI_GETSELECTIONFADE"); break;
	case SPI_GETTOOLTIPANIMATION: return StringView("SPI_GETTOOLTIPANIMATION"); break;
	case SPI_GETTOOLTIPFADE: return StringView("SPI_GETTOOLTIPFADE"); break;
	case SPI_GETUIEFFECTS: return StringView("SPI_GETUIEFFECTS"); break;
	case SPI_SETCOMBOBOXANIMATION: return StringView("SPI_SETCOMBOBOXANIMATION"); break;
	case SPI_SETCURSORSHADOW: return StringView("SPI_SETCURSORSHADOW"); break;
	case SPI_SETGRADIENTCAPTIONS: return StringView("SPI_SETGRADIENTCAPTIONS"); break;
	case SPI_SETHOTTRACKING: return StringView("SPI_SETHOTTRACKING"); break;
	case SPI_SETLISTBOXSMOOTHSCROLLING: return StringView("SPI_SETLISTBOXSMOOTHSCROLLING"); break;
	case SPI_SETMENUANIMATION: return StringView("SPI_SETMENUANIMATION"); break;
	case SPI_SETSELECTIONFADE: return StringView("SPI_SETSELECTIONFADE"); break;
	case SPI_SETTOOLTIPANIMATION: return StringView("SPI_SETTOOLTIPANIMATION"); break;
	case SPI_SETTOOLTIPFADE: return StringView("SPI_SETTOOLTIPFADE"); break;
	case SPI_SETUIEFFECTS: return StringView("SPI_SETUIEFFECTS"); break;
	case SPI_GETACTIVEWINDOWTRACKING: return StringView("SPI_GETACTIVEWINDOWTRACKING"); break;
	case SPI_GETACTIVEWNDTRKZORDER: return StringView("SPI_GETACTIVEWNDTRKZORDER"); break;
	case SPI_GETACTIVEWNDTRKTIMEOUT: return StringView("SPI_GETACTIVEWNDTRKTIMEOUT"); break;
	case SPI_GETANIMATION: return StringView("SPI_GETANIMATION"); break;
	case SPI_GETBORDER: return StringView("SPI_GETBORDER"); break;
	case SPI_GETCARETWIDTH: return StringView("SPI_GETCARETWIDTH"); break;
	case SPI_GETDOCKMOVING: return StringView("SPI_GETDOCKMOVING"); break;
	case SPI_GETDRAGFROMMAXIMIZE: return StringView("SPI_GETDRAGFROMMAXIMIZE"); break;
	case SPI_GETDRAGFULLWINDOWS: return StringView("SPI_GETDRAGFULLWINDOWS"); break;
	case SPI_GETFOREGROUNDFLASHCOUNT: return StringView("SPI_GETFOREGROUNDFLASHCOUNT"); break;
	case SPI_GETFOREGROUNDLOCKTIMEOUT: return StringView("SPI_GETFOREGROUNDLOCKTIMEOUT"); break;
	case SPI_GETMINIMIZEDMETRICS: return StringView("SPI_GETMINIMIZEDMETRICS"); break;
	case SPI_GETMOUSEDOCKTHRESHOLD: return StringView("SPI_GETMOUSEDOCKTHRESHOLD"); break;
	case SPI_GETMOUSEDRAGOUTTHRESHOLD: return StringView("SPI_GETMOUSEDRAGOUTTHRESHOLD"); break;
	case SPI_GETMOUSESIDEMOVETHRESHOLD: return StringView("SPI_GETMOUSESIDEMOVETHRESHOLD"); break;
	case SPI_GETNONCLIENTMETRICS: return StringView("SPI_GETNONCLIENTMETRICS"); break;
	case SPI_GETPENDOCKTHRESHOLD: return StringView("SPI_GETPENDOCKTHRESHOLD"); break;
	case SPI_GETPENDRAGOUTTHRESHOLD: return StringView("SPI_GETPENDRAGOUTTHRESHOLD"); break;
	case SPI_GETPENSIDEMOVETHRESHOLD: return StringView("SPI_GETPENSIDEMOVETHRESHOLD"); break;
	case SPI_GETSHOWIMEUI: return StringView("SPI_GETSHOWIMEUI"); break;
	case SPI_GETSNAPSIZING: return StringView("SPI_GETSNAPSIZING"); break;
	case SPI_GETWINARRANGING: return StringView("SPI_GETWINARRANGING"); break;
	case SPI_SETACTIVEWINDOWTRACKING: return StringView("SPI_SETACTIVEWINDOWTRACKING"); break;
	case SPI_SETACTIVEWNDTRKZORDER: return StringView("SPI_SETACTIVEWNDTRKZORDER"); break;
	case SPI_SETACTIVEWNDTRKTIMEOUT: return StringView("SPI_SETACTIVEWNDTRKTIMEOUT"); break;
	case SPI_SETANIMATION: return StringView("SPI_SETANIMATION"); break;
	case SPI_SETBORDER: return StringView("SPI_SETBORDER"); break;
	case SPI_SETCARETWIDTH: return StringView("SPI_SETCARETWIDTH"); break;
	case SPI_SETDOCKMOVING: return StringView("SPI_SETDOCKMOVING"); break;
	case SPI_SETDRAGFROMMAXIMIZE: return StringView("SPI_SETDRAGFROMMAXIMIZE"); break;
	case SPI_SETDRAGFULLWINDOWS: return StringView("SPI_SETDRAGFULLWINDOWS"); break;
	case SPI_SETDRAGHEIGHT: return StringView("SPI_SETDRAGHEIGHT"); break;
	case SPI_SETDRAGWIDTH: return StringView("SPI_SETDRAGWIDTH"); break;
	case SPI_SETFOREGROUNDFLASHCOUNT: return StringView("SPI_SETFOREGROUNDFLASHCOUNT"); break;
	case SPI_SETFOREGROUNDLOCKTIMEOUT: return StringView("SPI_SETFOREGROUNDLOCKTIMEOUT"); break;
	case SPI_SETMINIMIZEDMETRICS: return StringView("SPI_SETMINIMIZEDMETRICS"); break;
	case SPI_SETMOUSEDOCKTHRESHOLD: return StringView("SPI_SETMOUSEDOCKTHRESHOLD"); break;
	case SPI_SETMOUSEDRAGOUTTHRESHOLD: return StringView("SPI_SETMOUSEDRAGOUTTHRESHOLD"); break;
	case SPI_SETMOUSESIDEMOVETHRESHOLD: return StringView("SPI_SETMOUSESIDEMOVETHRESHOLD"); break;
	case SPI_SETNONCLIENTMETRICS: return StringView("SPI_SETNONCLIENTMETRICS"); break;
	case SPI_SETPENDOCKTHRESHOLD: return StringView("SPI_SETPENDOCKTHRESHOLD"); break;
	case SPI_SETPENDRAGOUTTHRESHOLD: return StringView("SPI_SETPENDRAGOUTTHRESHOLD"); break;
	case SPI_SETPENSIDEMOVETHRESHOLD: return StringView("SPI_SETPENSIDEMOVETHRESHOLD"); break;
	case SPI_SETSHOWIMEUI: return StringView("SPI_SETSHOWIMEUI"); break;
	case SPI_SETSNAPSIZING: return StringView("SPI_SETSNAPSIZING"); break;
	case SPI_SETWINARRANGING: return StringView("SPI_SETWINARRANGING"); break;
	default: return StringView(); break;
	}
	return StringView();
}

} // namespace

using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::Networking::Connectivity;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows;

struct MessageWindow::WinRtAdapter {
	MessageWindow *_message = nullptr;
	UISettings _settings;
	winrt::event_token _textScaleFactorToken;
	winrt::event_token _colorChangedToken;
	winrt::event_token _networkStatusChangedToken;
	winrt::event_token _clipboardChangedToken;

	WinRtAdapter(MessageWindow *w) : _message(w) {
		winrt::init_apartment(winrt::apartment_type::single_threaded);

		_textScaleFactorToken = _settings.TextScaleFactorChanged(
				[this](const UISettings &sender, const Foundation::IInspectable &args) {
			updateTheme();
		});
		_colorChangedToken = _settings.ColorValuesChanged(
				[this](const UISettings &sender, const Foundation::IInspectable &args) {
			updateTheme();
		});

		_networkStatusChangedToken = NetworkInformation::NetworkStatusChanged(
				[this](const winrt::Windows::Foundation::IInspectable &sender) {
			_message->handleNetworkStateChanged(getNetworkFlags());
		});

		_clipboardChangedToken =
				Clipboard::ContentChanged([](const auto &sender, const auto &args) { });
	}

	~WinRtAdapter() {
		_settings.TextScaleFactorChanged(_textScaleFactorToken);
		_settings.ColorValuesChanged(_colorChangedToken);
		NetworkInformation::NetworkStatusChanged(_networkStatusChangedToken);
		Clipboard::ContentChanged(_clipboardChangedToken);

		winrt::uninit_apartment();
	}

	void updateTheme() { _message->handleSettingsChanged(); }

	float getTextScaleFactor() const { return _settings.TextScaleFactor(); }

	bool isDarkTheme() const {
		auto color = _settings.GetColorValue(
				winrt::Windows::UI::ViewManagement::UIColorType::Foreground);
		auto c = Color(Color3B(color.R, color.G, color.B));

		return c.text() == Color::Black;
	}

	uint32_t getCursorSize() const { return static_cast<uint32_t>(_settings.CursorSize().Height); }

	uint32_t getDoubleClickInterval() const { return _settings.DoubleClickTime(); }

	NetworkFlags getNetworkFlags() const {
		NetworkFlags flags = NetworkFlags::None;

		auto profile = NetworkInformation::GetInternetConnectionProfile();
		if (profile) {
			if (profile.IsWlanConnectionProfile()) {
				flags |= NetworkFlags::WLAN;
			}
			if (profile.IsWwanConnectionProfile()) {
				flags |= NetworkFlags::WWAN;
				if (auto state = profile.WwanConnectionProfileDetails()) {
					auto reg = state.GetNetworkRegistrationState();
					switch (reg) {
					case WwanNetworkRegistrationState::None: break;
					case WwanNetworkRegistrationState::Deregistered: break;
					case WwanNetworkRegistrationState::Searching: break;
					case WwanNetworkRegistrationState::Home: break;
					case WwanNetworkRegistrationState::Roaming:
						flags |= NetworkFlags::Roaming;
						break;
					case WwanNetworkRegistrationState::Partner:
						flags |= NetworkFlags::Roaming;
						break;
					case WwanNetworkRegistrationState::Denied: break;
					}
				}
			}
			if (auto adapter = profile.NetworkAdapter()) {
				switch (adapter.IanaInterfaceType()) {
				case 6: flags |= NetworkFlags::Wired; break; // ethernet - assume wired
				case 23: flags |= NetworkFlags::Wired; break; // PPP - assume wired
				case 131: flags |= NetworkFlags::Vpn; break; // tunnel - assume VPN
				}
				if (auto item = adapter.NetworkItem()) {
					auto types = item.GetNetworkTypes();
					if ((toInt(types) & toInt(NetworkTypes::Internet)) != 0) {
						flags |= NetworkFlags::Internet;
					}
					if ((toInt(types) & toInt(NetworkTypes::PrivateNetwork)) != 0) {
						flags |= NetworkFlags::Local;
					}
				}
			}
			if (auto cost = profile.GetConnectionCost()) {
				if (cost.BackgroundDataUsageRestricted()) {
					flags |= NetworkFlags::Restricted;
				}
				if (cost.OverDataLimit()) {
					flags |= NetworkFlags::Suspended;
				}
				if (cost.Roaming()) {
					flags |= NetworkFlags::Roaming;
				}
				switch (cost.NetworkCostType()) {
				case NetworkCostType::Unknown: break;
				case NetworkCostType::Unrestricted: break;
				case NetworkCostType::Fixed: break;
				case NetworkCostType::Variable: flags |= NetworkFlags::Metered; break;
				}
			}
			if (auto dataPlan = profile.GetDataPlanStatus()) {
				if (auto usage = dataPlan.DataPlanUsage()) {
					if (usage.MegabytesUsed() > 0) {
						flags |= NetworkFlags::Validated;
					}
				}
			}
			switch (profile.GetNetworkConnectivityLevel()) {
			case NetworkConnectivityLevel::None: break;
			case NetworkConnectivityLevel::LocalAccess: flags |= NetworkFlags::Local; break;
			case NetworkConnectivityLevel::ConstrainedInternetAccess:
				flags |= NetworkFlags::Internet | NetworkFlags::CaptivePortal;
				break;
			case NetworkConnectivityLevel::InternetAccess: flags |= NetworkFlags::Internet; break;
			}
		}

		return flags;
	}

	bool hasDataType(const DataPackageView &data, StringView type) const {
		auto t = string::toUtf16<memory::PoolInterface>(type);
		return data.Contains(winrt::hstring((wchar_t *)t.data(), t.size()));
	}

	bool hasDataType(const DataPackageView &data, WideStringView type) const {
		return data.Contains(winrt::hstring((wchar_t *)type.data(), type.size()));
	}

	bool hasDataType(const DataPackageView &data, const winrt::hstring &type) const {
		return data.Contains(type);
	}

	StringView getFormatType(const DataPackageView &data, const winrt::hstring &fmt) {
		if (fmt == StandardDataFormats::ApplicationLink()) {
			if (!hasDataType(data, StandardDataFormats::WebLink())
					&& !hasDataType(data, StandardDataFormats::Uri())
					&& !hasDataType(data, "text/uri-list")) {
				return StringView("text/uri-list");
			}
		} else if (fmt == StandardDataFormats::Bitmap()) {
			// no known MIME
		} else if (fmt == StandardDataFormats::Html()) {
			if (!hasDataType(data, "text/html")) {
				return StringView("text/html");
			}
		} else if (fmt == StandardDataFormats::Rtf()) {
			if (!hasDataType(data, "application/rtf")) {
				return StringView("application/rtf");
			}
		} else if (fmt == StandardDataFormats::StorageItems()) {
			// no known MIME
		} else if (fmt == StandardDataFormats::Text()) {
			if (!hasDataType(data, "text/plain")) {
				return StringView("text/plain");
			}
		} else if (fmt == StandardDataFormats::Uri()) {
			if (!hasDataType(data, StandardDataFormats::ApplicationLink())
					&& !hasDataType(data, StandardDataFormats::WebLink())
					&& !hasDataType(data, "text/uri-list")) {
				return StringView("text/uri-list");
			}
		} else if (fmt == StandardDataFormats::UserActivityJsonArray()) {
			// no known MIME
		} else if (fmt == StandardDataFormats::WebLink()) {
			if (!hasDataType(data, StandardDataFormats::ApplicationLink())
					&& !hasDataType(data, StandardDataFormats::Uri())
					&& !hasDataType(data, "text/uri-list")) {
				return StringView("text/uri-list");
			}
		} else if (fmt == L"PNG") {
			if (!hasDataType(data, "image/png")) {
				return StringView("image/png");
			}
		}
		return StringView(string::toUtf8<memory::PoolInterface>(fmt.data(), fmt.size())).pdup();
	}

	static void readClipboardValue(ClipboardRequest *req, StringView type,
			const winrt::hstring &s) {
		auto uri = string::toUtf8<memory::StandartInterface>(s.data(), s.size());
		req->dataCallback(Status::Ok, BytesView(uri), type);
	}

	static void readClipboardValue(ClipboardRequest *req, StringView type,
			const Foundation::Uri &u) {
		readClipboardValue(req, type, u.ToString());
	}

	struct ClipboardReadRequest : Ref {
		Rc<ClipboardRequest> request;
		String type;
		Storage::Streams::IInputStream stream;
		Vector<Storage::Streams::Buffer> readBuffers;

		void cancel() {
			if (request) {
				request->dataCallback(Status::ErrorCancelled, BytesView(), type);
				request = nullptr;
			}
		}

		void finalize() {
			size_t bufSize = 0;
			for (auto &it : readBuffers) { bufSize += it.Length(); }

			Bytes data;
			data.resize(bufSize);

			bufSize = 0;
			for (auto &it : readBuffers) {
				memcpy(data.data() + bufSize, it.data(), it.Length());
				bufSize += it.Length();
			}

			request->dataCallback(Status::Ok, data, type);
			readBuffers.clear();
			request = nullptr;
		}

		void step() {
			if (!readBuffers.empty()) {
				auto &back = readBuffers.back();

				if (back.Length() != back.Capacity()) {
					finalize();
					return;
				}
			}

			auto &buf = readBuffers.emplace_back(Storage::Streams::Buffer(8_KiB));
			stream.ReadAsync(buf, buf.Capacity(), Storage::Streams::InputStreamOptions::ReadAhead)
					.Completed([req = Rc<ClipboardReadRequest>(this)](auto &&sender,
									   Foundation::AsyncStatus const status) {
				if (status == Foundation::AsyncStatus::Completed) {
					req->step();
				} else {
					req->cancel();
				}
			});
		}
	};

	static void readClipboardValue(ClipboardRequest *req, StringView type,
			const Storage::Streams::RandomAccessStreamReference &s) {
		s.OpenReadAsync().Completed([req = Rc<ClipboardRequest>(req), type = type.str<Interface>()](
											auto &&sender, Foundation::AsyncStatus const status) {
			if (status == Foundation::AsyncStatus::Completed) {
				auto readReq = Rc<ClipboardReadRequest>::create();
				readReq->request = req;
				readReq->type = type;
				readReq->stream = sp::move(sender.GetResults());
				readReq->step();
			} else {
				req->dataCallback(Status::ErrorCancelled, BytesView(), type);
			}
		});
	}

	static void readClipboardValue(ClipboardRequest *req, StringView type,
			const Foundation::Collections::IVectorView<Storage::IStorageItem> &s) {
		Value data;

		for (size_t i = 0; i < s.Size(); ++i) {
			auto v = s.GetAt(i);
			if (v) {
				auto &itemValue = data.emplace();
				auto name = v.Name();
				auto path = v.Path();
				auto date =
						std::chrono::time_point_cast<std::chrono::microseconds>(v.DateCreated());
				auto epoch = date.time_since_epoch();
				auto value = std::chrono::duration_cast<std::chrono::microseconds>(epoch);

				itemValue.setString(string::toUtf8<Interface>(name.data(), name.size()), "Name");
				itemValue.setString(string::toUtf8<Interface>(path.data(), path.size()), "Path");
				itemValue.setInteger(value.count(), "DateCreated");

				auto attr = v.Attributes();
				for (auto v : flags(attr)) {
					switch (v) {
					case Storage::FileAttributes::Normal: break;
					case Storage::FileAttributes::ReadOnly:
						itemValue.emplace("Attributes").addString("ReadOnly");
						break;
					case Storage::FileAttributes::Directory:
						itemValue.emplace("Attributes").addString("Directory");
						break;
					case Storage::FileAttributes::Archive:
						itemValue.emplace("Attributes").addString("Archive");
						break;
					case Storage::FileAttributes::Temporary:
						itemValue.emplace("Attributes").addString("Temporary");
						break;
					case Storage::FileAttributes::LocallyIncomplete:
						itemValue.emplace("Attributes").addString("LocallyIncomplete");
						break;
					}
				}
			}
		}

		auto d = data::write(data, data::EncodeFormat::Cbor);
		req->dataCallback(Status::Ok, d, type);
	}

	static void readClipboardValue(ClipboardRequest *req, StringView type,
			const Foundation::IInspectable &s) {

		if (auto stream = s.try_as<Storage::Streams::IInputStream>()) {
			//readClipboardValue(req, type, stream);
			auto readReq = Rc<ClipboardReadRequest>::create();
			readReq->request = req;
			readReq->type = type.str<Interface>();
			readReq->stream = sp::move(stream);
			readReq->step();
		} else {
			req->dataCallback(Status::ErrorCancelled, BytesView(), type);
		}
	}

	template <typename T>
	static void readFromClipboard(Foundation::IAsyncOperation<T> &&async,
			Rc<ClipboardRequest> &&req, StringView type) {
		async.Completed([req = sp::move(req), type = type.str<Interface>()](auto &&sender,
								Foundation::AsyncStatus const status) {
			if (status == Foundation::AsyncStatus::Completed) {
				readClipboardValue(req, type, sender.GetResults());
			} else {
				req->dataCallback(Status::ErrorCancelled, BytesView(), type);
			}
		});
	}

	Status readFromClipboard(Rc<ClipboardRequest> &&req) {
		auto content = Clipboard::GetContent();
		if (!content) {
			return Status::ErrorNotImplemented;
		}

		memory::map<StringView, winrt::hstring> formatsMap;
		memory::vector<StringView> formatsVec;
		auto formats = content.AvailableFormats();
		for (size_t i = 0; i < formats.Size(); ++i) {
			auto v = formats.GetAt(i);
			auto str = getFormatType(content, v);

			formatsVec.emplace_back(str);
			formatsMap.emplace(str, v);

			XL_WIN32_LOG("Clipboard type: ", str);
		}

		auto type = req->typeCallback(formatsVec);

		auto it = formatsMap.find(type);
		if (it == formatsMap.end()) {
			return Status::ErrorInvalidArguemnt;
		}

		if (it->second == StandardDataFormats::ApplicationLink()) {
			readFromClipboard(content.GetApplicationLinkAsync(), sp::move(req), it->first);
		} else if (it->second == StandardDataFormats::Bitmap()) {
			readFromClipboard(content.GetBitmapAsync(), sp::move(req), it->first);
		} else if (it->second == StandardDataFormats::Html()) {
			readFromClipboard(content.GetHtmlFormatAsync(), sp::move(req), it->first);
		} else if (it->second == StandardDataFormats::Rtf()) {
			readFromClipboard(content.GetRtfAsync(), sp::move(req), it->first);
		} else if (it->second == StandardDataFormats::StorageItems()) {
			readFromClipboard(content.GetStorageItemsAsync(), sp::move(req), it->first);
		} else if (it->second == StandardDataFormats::Text()) {
			readFromClipboard(content.GetTextAsync(), sp::move(req), it->first);
		} else if (it->second == StandardDataFormats::Uri()) {
			readFromClipboard(content.GetUriAsync(), sp::move(req), it->first);
		} else if (it->second == StandardDataFormats::WebLink()) {
			readFromClipboard(content.GetWebLinkAsync(), sp::move(req), it->first);
		} else {
			readFromClipboard(content.GetDataAsync(it->second), sp::move(req), it->first);
		}

		return Status::ErrorNotImplemented;
	}

	struct ClipboardDataProvider : Ref {
		Rc<ClipboardData> data;

		void setProvider(DataPackage &dataPackage, const winrt::hstring &key, StringView type,
				bool transcode = false) {
			dataPackage.SetDataProvider(key,
					DataProviderHandler([type, dataProvider = Rc<ClipboardDataProvider>(this),
												transcode](auto &&request) {
				auto inMemoryStream = Storage::Streams::InMemoryRandomAccessStream();
				auto bytes = dataProvider->data->encodeCallback(type);
				if (transcode && type.starts_with("text/")) {
					StringView utf8_str((const char *)bytes.data(), bytes.size());
					const auto size = string::getUtf16Length(utf8_str);
					Storage::Streams::Buffer buffer((size + 1) * sizeof(wchar_t));

					char16_t *d = (char16_t *)buffer.data();

					uint8_t offset = 0;
					auto ptr = utf8_str.data();
					auto len = utf8_str.size();
					auto end = ptr + utf8_str.size();
					while (ptr < end) {
						auto c = unicode::utf8Decode32(ptr, len, offset);
						d += unicode::utf16EncodeBuf(d, c);
						ptr += offset;
						len -= offset;
					}
					d += unicode::utf16EncodeBuf(d, char32_t(0));

					buffer.Length((size + 1) * sizeof(wchar_t));
					inMemoryStream.WriteAsync(buffer);
				} else {
					Storage::Streams::Buffer buffer(bytes.size());
					memcpy(buffer.data(), bytes.data(), bytes.size());
					buffer.Length(bytes.size());
					inMemoryStream.WriteAsync(buffer);
				}
				request.SetData(inMemoryStream);
			}));
		}
	};

	Status writeToClipboard(Rc<ClipboardData> &&data) {
		DataPackage dataPackage;
		auto dataProvider = Rc<ClipboardDataProvider>::create();
		dataProvider->data = sp::move(data);

		for (auto &type : dataProvider->data->types) {
			auto wType = string::toUtf16<Interface>(type);

			if (type == "text/plain") {
				dataProvider->setProvider(dataPackage, StandardDataFormats::Text(), type, true);
			} else if (type == "text/html") {
				dataProvider->setProvider(dataPackage, StandardDataFormats::Html(), type, true);
			} else if (type == "image/png") {
				dataProvider->setProvider(dataPackage, L"PNG", type);
			}

			dataProvider->setProvider(dataPackage,
					winrt::hstring((wchar_t *)wType.data(), wType.size()), type);
		};

		Clipboard::SetContent(dataPackage);
		return Status::Ok;
	}
};

MessageWindow::~MessageWindow() {
	_networkConnectivity = nullptr;

	if (_window) {
		SetWindowLongPtrW(_window, GWLP_USERDATA, 0);
		DestroyWindow(_window);
		_window = nullptr;
	}
	::UnregisterClassW(_windowClass.lpszClassName, _module);

	if (_adapter) {
		delete _adapter;
	}
}

bool MessageWindow::init(NotNull<WindowsContextController> c) {
	_controller = c;

	_adapter = new (std::nothrow) WinRtAdapter(this);

	_module = GetModuleHandleW(nullptr);

	_windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	_windowClass.lpfnWndProc = &wndProc;
	_windowClass.cbClsExtra = 0;
	_windowClass.cbWndExtra = 0;
	_windowClass.hInstance = _module;
	_windowClass.hIcon = (HICON)LoadImage(nullptr, IDI_APPLICATION, IMAGE_ICON, 0, 0,
			LR_DEFAULTSIZE | LR_SHARED);
	_windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	_windowClass.hbrBackground = nullptr;
	_windowClass.lpszMenuName = nullptr;
	_windowClass.lpszClassName = ClassName;

	RegisterClassW(&_windowClass);

	_window = CreateWindowExW(0, // Optional window styles.
			ClassName, // Window class
			ClassName, // Window text
			0, // Window style

			// Size and position
			0, 0, 0, 0,

			NULL, // Parent window
			NULL, // Menu
			_module, // Instance handle
			nullptr);

	if (_window) {
		SetWindowLongPtrW(_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	}

	handleSettingsChanged();
	handleNetworkStateChanged(_adapter->getNetworkFlags());

	return true;
}

void MessageWindow::setWindowRect(IRect) { }

Status MessageWindow::handleDisplayChanged(Extent2 ex) {
	return _controller->handleDisplayChanged(ex);
}

Status MessageWindow::handleNetworkStateChanged(NetworkFlags flags) {
	_controller->handleNetworkStateChanged(flags);
	return Status::Ok;
}

Status MessageWindow::handleSystemNotification(SystemNotification notification) {
	_controller->handleSystemNotification(notification);
	return Status::Propagate;
}

Status MessageWindow::handleSettingsChanged() {
	_controller->getLooper()->performOnThread(
			[this] { _controller->handleThemeInfoChanged(getThemeInfo()); }, this);
	return Status::Propagate;
}

Status MessageWindow::readFromClipboard(Rc<ClipboardRequest> &&req) {
	if (_adapter) {
		return _adapter->readFromClipboard(sp::move(req));
	}
	return Status::ErrorNotImplemented;
}
Status MessageWindow::writeToClipboard(Rc<ClipboardData> &&data) {
	if (_adapter) {
		return _adapter->writeToClipboard(sp::move(data));
	}
	return Status::ErrorNotImplemented;
}

LRESULT MessageWindow::wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	auto getResultForStatus = [&](StringView event, Status st, LRESULT okStatus = 0) -> LRESULT {
		switch (st) {
		case Status::Ok: return okStatus; break;
		case Status::Declined: return -1; break;
		case Status::Propagate: return ::DefWindowProcW(hwnd, uMsg, wParam, lParam); break;
		default:
			if (toInt(st) > 0) {
				return toInt(st);
			}
			break;
		}
		log::error("WindowClass", "Fail to process event ", event, " with Status: ", st);
		return -1;
	};

	auto handleDefault = [&]() { return ::DefWindowProcW(hwnd, uMsg, wParam, lParam); };

	auto win = reinterpret_cast<MessageWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (!win) {
		return handleDefault();
	}

	switch (uMsg) {
	case WM_DEVICECHANGE:
		XL_WIN32_LOG("Event: WM_DEVICECHANGE");
		return getResultForStatus("WM_DEVICECHANGE ",
				win->handleDisplayChanged(Extent2(LOWORD(lParam), HIWORD(lParam))));
		break;

	case WM_DISPLAYCHANGE:
		XL_WIN32_LOG("Event: WM_DISPLAYCHANGE");
		return getResultForStatus("WM_DISPLAYCHANGE ",
				win->handleDisplayChanged(Extent2(LOWORD(lParam), HIWORD(lParam))));
		break;

	case WM_SETTINGCHANGE: {
		auto action = getUiAction(wParam);
		if (!action.empty()) {
			XL_WIN32_LOG("Event: WM_SETTINGCHANGE: ", action, " ",
					string::toUtf8<Interface>((char16_t *)lParam));
		} else {
			XL_WIN32_LOG("Event: WM_SETTINGCHANGE: ", std::hex, wParam, " ",
					string::toUtf8<Interface>((char16_t *)lParam));
		}
		win->handleSettingsChanged();
		return handleDefault();
		break;
	}

	case WM_POWERBROADCAST:
		switch (wParam) {
		case PBT_APMPOWERSTATUSCHANGE: {
			XL_WIN32_LOG("Event: WM_POWERBROADCAST PBT_APMPOWERSTATUSCHANGE");
			SYSTEM_POWER_STATUS status;
			if (GetSystemPowerStatus(&status)) {
				if (status.SystemStatusFlag != 0 && status.BatteryFlag != 255
						&& (hasFlag(status.BatteryFlag, BYTE(2))
								|| hasFlag(status.BatteryFlag, BYTE(4)))) {
					return getResultForStatus("WM_POWERBROADCAST PBT_APMPOWERSTATUSCHANGE ",
							win->handleSystemNotification(SystemNotification::LowMemory));
				}
			}
			break;
		}
		case PBT_APMRESUMEAUTOMATIC:
			XL_WIN32_LOG("Event: WM_POWERBROADCAST PBT_APMRESUMEAUTOMATIC");
			return getResultForStatus("WM_POWERBROADCAST PBT_APMRESUMEAUTOMATIC ",
					win->handleSystemNotification(SystemNotification::Resume));
			break;
		case PBT_APMRESUMESUSPEND:
			XL_WIN32_LOG("Event: WM_POWERBROADCAST PBT_APMRESUMESUSPEND");
			return getResultForStatus("WM_POWERBROADCAST PBT_APMRESUMESUSPEND ",
					win->handleSystemNotification(SystemNotification::Resume));
			break;
		case PBT_APMQUERYSUSPEND:
			XL_WIN32_LOG("Event: WM_POWERBROADCAST PBT_APMQUERYSUSPEND");
			return getResultForStatus("WM_POWERBROADCAST PBT_APMQUERYSUSPEND ",
					win->handleSystemNotification(SystemNotification::QuerySuspend));
			break;
		case PBT_APMSUSPEND:
			XL_WIN32_LOG("Event: WM_POWERBROADCAST PBT_APMSUSPEND");
			return getResultForStatus("WM_POWERBROADCAST PBT_APMSUSPEND",
					win->handleSystemNotification(SystemNotification::Suspending));
			break;
		case PBT_POWERSETTINGCHANGE:
			XL_WIN32_LOG("Event: WM_POWERBROADCAST PBT_POWERSETTINGCHANGE");
			return handleDefault();
			break;
		default:
			XL_WIN32_LOG("Event: WM_POWERBROADCAST: ", std::hex, wParam);
			return handleDefault();
			break;
		}
		break;

	case WM_COMPACTING:
		XL_WIN32_LOG("Event: WM_COMPACTING");
		return getResultForStatus("WM_COMPACTING ",
				win->handleSystemNotification(SystemNotification::LowMemory));
		break;

	default: return handleDefault(); break;
	}
	return 0;
}

ThemeInfo MessageWindow::getThemeInfo() {
	ThemeInfo ret;
	ret.colorScheme = ThemeInfo::SchemePreferLight.str<Interface>();
	if (_adapter->isDarkTheme()) {
		ret.colorScheme = ThemeInfo::SchemePreferDark.str<Interface>();
	}

	ret.decorations.resizeInset = 6.0f;

	ret.cursorSize = _adapter->getCursorSize();
	ret.doubleClickInterval = _adapter->getDoubleClickInterval();
	ret.textScaling = _adapter->getTextScaleFactor();

	static constexpr uint32_t ValueDataSize = 64;
	static constexpr uint32_t ValueNameSize = 512;

	//uint8_t valueData[ValueDataSize];
	WCHAR valueName[ValueNameSize];
	WCHAR valueColor[ValueDataSize];
	WCHAR valueSize[ValueDataSize];

	if (::GetCurrentThemeName(valueName, ValueNameSize, valueColor, ValueDataSize, valueSize,
				ValueDataSize)
			== S_OK) {
		auto path = filesystem::native::nativeToPosix<Interface>(
				string::toUtf8<Interface>(WideStringView((char16_t *)valueName)));
		ret.systemTheme = filepath::name(path).str<Interface>();
	}

	ret.leftHandedMouse = (GetSystemMetrics(SM_SWAPBUTTON) != 0);

	UINT ulScrollLines;
	if (SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0)) {
		ret.scrollModifier = ulScrollLines;
	}

	HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	LOGFONTW lf;

	if (GetObjectW(hFont, sizeof(LOGFONT), &lf) > 0) {
		ret.systemFontName = string::toUtf8<Interface>(WideStringView((char16_t *)lf.lfFaceName));
	}

	return ret;
}

} // namespace stappler::xenolith::platform
