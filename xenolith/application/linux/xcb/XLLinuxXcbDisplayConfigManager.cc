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

#include "XLLinuxXcbDisplayConfigManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct XrandrOutputInfo;

struct PropertyInfo {
	xcb_atom_t atom;
	String name;
};

struct CrtcPanning {
	uint16_t left;
	uint16_t top;
	uint16_t width;
	uint16_t height;
	uint16_t trackLeft;
	uint16_t trackTop;
	uint16_t trackWidth;
	uint16_t trackHeight;
	int16_t borderLeft;
	int16_t borderTop;
	int16_t borderRight;
	int16_t borderBottom;
};

struct XrandrCrtcInfo {
	xcb_randr_crtc_t crtc;
	int16_t x;
	int16_t y;
	uint16_t width;
	uint16_t height;
	const DisplayMode *mode = nullptr;
	uint16_t rotation;
	uint16_t rotations;
	Vector<const XrandrOutputInfo *> outputs;
	Vector<const XrandrOutputInfo *> possible;

	float scaleX = 1.0f;
	float scaleY = 1.0f;

	CrtcPanning panning;
	xcb_render_transform_t transform;

	String filterName;
	Vector<xcb_render_fixed_t> filterParams;
};

struct XrandrOutputInfo {
	xcb_randr_output_t output;
	Vector<const DisplayMode *> modes;
	Vector<const XrandrCrtcInfo *> crtcs;
	const XrandrCrtcInfo *crtc = nullptr;
	const DisplayMode *preferred = nullptr;
	String name;
	Vector<PropertyInfo> properties;
	bool primary = false;
};

struct XrandrConfig : public Ref {
	Map<xcb_randr_mode_t, DisplayMode> modes;
	Map<xcb_randr_output_t, XrandrOutputInfo> outputs;
	Map<xcb_randr_crtc_t, XrandrCrtcInfo> crtcs;
};

static DisplayMode parseRandrModeInfo(xcb_randr_mode_info_t *mode, uint8_t *name) {
	double vTotal = mode->vtotal;

	if (mode->mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN) {
		/* doublescan doubles the number of lines */
		vTotal *= 2;
	}

	if (mode->mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE) {
		/* interlace splits the frame into two fields */
		/* the field rate is what is typically reported by monitors */
		vTotal /= 2;
	}

	uint32_t rate = 0;
	if (mode->htotal && vTotal) {
		rate = uint32_t(
				floor(1'000 * double(mode->dot_clock) / (double(mode->htotal) * double(vTotal))));
	}

	/*std::cout << "mode: " << String((const char *)name, mode->name_len) << "@" << rate << " "
			  << mode->mode_flags;

	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_HSYNC_POSITIVE))) {
		std::cout << " HSYNC_POSITIVE";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_HSYNC_NEGATIVE))) {
		std::cout << " HSYNC_NEGATIVE";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_VSYNC_POSITIVE))) {
		std::cout << " VSYNC_POSITIVE";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_VSYNC_NEGATIVE))) {
		std::cout << " VSYNC_NEGATIVE";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_INTERLACE))) {
		std::cout << " INTERLACE";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_DOUBLE_SCAN))) {
		std::cout << " DOUBLE_SCAN";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_CSYNC))) {
		std::cout << " CSYNC";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_CSYNC_POSITIVE))) {
		std::cout << " CSYNC_POSITIVE";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_CSYNC_NEGATIVE))) {
		std::cout << " CSYNC_NEGATIVE";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_HSKEW_PRESENT))) {
		std::cout << " HSKEW_PRESENT";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_BCAST))) {
		std::cout << " BCAST";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_PIXEL_MULTIPLEX))) {
		std::cout << " PIXEL_MULTIPLEX";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_DOUBLE_CLOCK))) {
		std::cout << " DOUBLE_CLOCK";
	}
	if (hasFlag(mode->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_HALVE_CLOCK))) {
		std::cout << " HALVE_CLOCK";
	}
	std::cout << "\n";*/

	return DisplayMode{
		mode->id,
		{
			mode->width,
			mode->height,
			rate,
		},
		String((const char *)name, mode->name_len),
	};
}

bool XcbDisplayConfigManager::init(NotNull<XcbConnection> c,
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (!DisplayConfigManager::init(sp::move(cb))) {
		return false;
	}

	_connection = c;
	_xcb = c->getXcb();
	_root = c->getDefaultScreen()->root;

	updateDisplayConfig();

	// XRandR получает заранее расширенные буферы и скейлит их непосредственно
	_scalingMode = DirectScaling;

	return true;
}

void XcbDisplayConfigManager::setCallback(Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	_onConfigChanged = sp::move(cb);
}

void XcbDisplayConfigManager::invalidate() {
	DisplayConfigManager::invalidate();
	_connection = nullptr;
	_xcb = nullptr;
	_root = 0;
}

void XcbDisplayConfigManager::update() { updateDisplayConfig(nullptr); }

String XcbDisplayConfigManager::getMonitorForPosition(int16_t x, int16_t y) {
	if (!_currentConfig) {
		return String();
	}

	auto native = _currentConfig->native.get_cast<XrandrConfig>();

	xcb_randr_crtc_t target = maxOf<xcb_randr_crtc_t>();
	int64_t distance = maxOf<int64_t>();

	for (auto &it : native->crtcs) {
		if (!it.second.mode || it.second.outputs.empty()) {
			continue;
		}
		if (x > it.second.x && y > it.second.y) {
			auto dx = it.second.x - x;
			auto dy = it.second.y - y;
			auto d = dx * dx + dy * dy;
			if (d < distance) {
				distance = d;
				target = it.second.crtc;
			}
		}
	}

	if (target == maxOf<xcb_randr_crtc_t>()) {
		return String();
	}

	auto it = native->crtcs.find(target);
	if (it != native->crtcs.end()) {
		return it->second.outputs.front()->name;
	}

	return String();
}

void XcbDisplayConfigManager::updateDisplayConfig(Function<void(DisplayConfig *)> &&cb) {
	struct OutputCookie {
		xcb_randr_get_output_info_cookie_t infoCookie;
		xcb_randr_list_output_properties_cookie_t listCookie;
		XrandrOutputInfo *info;
	};

	if (!_connection) {
		cb(nullptr);
		return;
	}

	auto ret = Rc<DisplayConfig>::create();
	auto cfg = Rc<XrandrConfig>::create();

	auto parseScreenInfo = [&](xcb_randr_get_screen_info_cookie_t cookie) {
		auto reply = _connection->perform(_xcb->xcb_randr_get_screen_info_reply, cookie);
		if (!reply) {
			return;
		}

		auto sizes = _xcb->xcb_randr_get_screen_info_sizes(reply);
		auto len = _xcb->xcb_randr_get_screen_info_sizes_length(reply);
		uint32_t sizeId = 0;
		while (len > 0) {
			if (sizeId == reply->sizeID) {
				ret->desktopRect = IRect(0, 0, sizes->width, sizes->height);
				break;
			}
			++sizes;
			++sizeId;
			--len;
		}
	};

	auto parseScreenResourcesCurrentReply =
			[&](xcb_randr_get_screen_resources_current_cookie_t cookie) {
		auto reply =
				_connection->perform(_xcb->xcb_randr_get_screen_resources_current_reply, cookie);
		if (!reply) {
			return;
		}

		ret->serial = reply->config_timestamp;

		auto names = _xcb->xcb_randr_get_screen_resources_current_names(reply);
		auto modes = _xcb->xcb_randr_get_screen_resources_current_modes(reply);
		auto nmodes = _xcb->xcb_randr_get_screen_resources_current_modes_length(reply);

		while (nmodes > 0) {
			if (!hasFlag(modes->mode_flags, uint32_t(XCB_RANDR_MODE_FLAG_INTERLACE))) {
				auto m = parseRandrModeInfo(modes, names);
				cfg->modes.emplace(m.xid, move(m));
			}

			names += modes->name_len;

			++modes;
			--nmodes;
		}

		auto outputs = _xcb->xcb_randr_get_screen_resources_current_outputs(reply);
		auto noutputs = _xcb->xcb_randr_get_screen_resources_current_outputs_length(reply);

		while (noutputs > 0) {
			cfg->outputs.emplace(*outputs, XrandrOutputInfo{*outputs});
			++outputs;
			--noutputs;
		}

		auto crtcs = _xcb->xcb_randr_get_screen_resources_current_crtcs(reply);
		auto ncrtcs = _xcb->xcb_randr_get_screen_resources_current_crtcs_length(reply);

		while (ncrtcs > 0) {
			cfg->crtcs.emplace(*crtcs, XrandrCrtcInfo{*crtcs});
			++crtcs;
			--ncrtcs;
		}
	};

	auto parseOutputInfo = [&](const OutputCookie &cookie) {
		auto reply = _connection->perform(_xcb->xcb_randr_get_output_info_reply, cookie.infoCookie);
		if (!reply) {
			return;
		}

		auto cIt = cfg->crtcs.find(reply->crtc);
		if (cIt != cfg->crtcs.end()) {
			cookie.info->crtc = &cIt->second;
		}

		auto modes = _xcb->xcb_randr_get_output_info_modes(reply);
		auto nmodes = _xcb->xcb_randr_get_output_info_modes_length(reply);

		auto preferred = reply->num_preferred;

		int idx = 0;
		while (idx < nmodes) {
			auto mIt = cfg->modes.find(*modes);

			if (mIt != cfg->modes.end()) {
				cookie.info->modes.emplace_back(&mIt->second);
				if (preferred && idx + 1 == preferred) {
					cookie.info->preferred = &mIt->second;
				}
			}

			++idx;
			++modes;
		}

		auto crtcs = _xcb->xcb_randr_get_output_info_crtcs(reply);
		auto ncrtcs = _xcb->xcb_randr_get_output_info_crtcs_length(reply);

		while (ncrtcs > 0) {
			auto cIt = cfg->crtcs.find(*crtcs);
			if (cIt != cfg->crtcs.end()) {
				cookie.info->crtcs.emplace_back(&cIt->second);
			}

			++crtcs;
			--ncrtcs;
		}

		auto name = _xcb->xcb_randr_get_output_info_name(reply);
		auto nameLen = _xcb->xcb_randr_get_output_info_name_length(reply);

		cookie.info->name = String((const char *)name, nameLen);

		auto listReply = _connection->perform(_xcb->xcb_randr_list_output_properties_reply,
				cookie.listCookie);
		if (!listReply) {
			return;
		}

		auto atoms = _xcb->xcb_randr_list_output_properties_atoms(listReply);
		auto natoms = _xcb->xcb_randr_list_output_properties_atoms_length(listReply);

		while (natoms > 0) {
			cookie.info->properties.emplace_back(PropertyInfo{*atoms});
			++atoms;
			--natoms;
		}
	};

	auto parseCrtcInfo = [&](xcb_randr_get_crtc_info_cookie_t cookie, XrandrCrtcInfo *crtc) {
		auto reply = _connection->perform(_xcb->xcb_randr_get_crtc_info_reply, cookie);
		if (!reply) {
			return;
		}

		auto outputsPtr = _xcb->xcb_randr_get_crtc_info_outputs(reply);
		auto noutputs = _xcb->xcb_randr_get_crtc_info_outputs_length(reply);

		while (noutputs) {
			auto oIt = cfg->outputs.find(*outputsPtr);
			if (oIt != cfg->outputs.end()) {
				crtc->outputs.emplace_back(&oIt->second);
			}
			++outputsPtr;
			--noutputs;
		}

		auto possiblePtr = _xcb->xcb_randr_get_crtc_info_possible(reply);
		auto npossible = _xcb->xcb_randr_get_crtc_info_possible_length(reply);

		while (npossible) {
			auto oIt = cfg->outputs.find(*possiblePtr);
			if (oIt != cfg->outputs.end()) {
				crtc->possible.emplace_back(&oIt->second);
			}
			++possiblePtr;
			--npossible;
		}

		auto mIt = cfg->modes.find(reply->mode);
		if (mIt != cfg->modes.end()) {
			crtc->mode = &mIt->second;
		}

		crtc->x = reply->x;
		crtc->y = reply->y;
		crtc->width = reply->width;
		crtc->height = reply->height;
		crtc->rotation = reply->rotation;
		crtc->rotations = reply->rotations;
	};

	auto parsePanning = [&](xcb_randr_get_panning_cookie_t cookie, XrandrCrtcInfo *crtc) {
		auto reply = _connection->perform(_xcb->xcb_randr_get_panning_reply, cookie);
		if (!reply) {
			return;
		}

		crtc->panning.left = reply->left;
		crtc->panning.top = reply->top;
		crtc->panning.width = reply->width;
		crtc->panning.height = reply->height;
		crtc->panning.trackLeft = reply->track_left;
		crtc->panning.trackTop = reply->track_top;
		crtc->panning.trackWidth = reply->track_width;
		crtc->panning.trackHeight = reply->track_height;
		crtc->panning.borderLeft = reply->border_left;
		crtc->panning.borderTop = reply->border_top;
		crtc->panning.borderRight = reply->border_right;
		crtc->panning.borderBottom = reply->border_bottom;
	};

	auto parseTransform = [&](xcb_randr_get_crtc_transform_cookie_t cookie, XrandrCrtcInfo *crtc) {
		auto reply = _connection->perform(_xcb->xcb_randr_get_crtc_transform_reply, cookie);
		if (!reply) {
			return;
		}

		crtc->transform = reply->current_transform;

		auto m11 = FixedToDouble(crtc->transform.matrix11);
		auto m12 = FixedToDouble(crtc->transform.matrix12);
		auto m21 = FixedToDouble(crtc->transform.matrix21);
		auto m22 = FixedToDouble(crtc->transform.matrix22);

		crtc->scaleX = std::sqrt(m11 * m11 + m12 * m12);
		crtc->scaleY = std::sqrt(m21 * m21 + m22 * m22);

		auto name = _xcb->xcb_randr_get_crtc_transform_current_filter_name(reply);
		auto namelen = _xcb->xcb_randr_get_crtc_transform_current_filter_name_length(reply);

		if (name && namelen) {
			crtc->filterName = String(name, namelen);
		}

		auto params = _xcb->xcb_randr_get_crtc_transform_current_params(reply);
		auto nparams = _xcb->xcb_randr_get_crtc_transform_current_params_length(reply);

		crtc->filterParams.reserve(nparams);
		while (nparams > 0) {
			crtc->filterParams.emplace_back(*params);
			++params;
		}
	};

	auto parseAtomNames = [&](xcb_get_atom_name_cookie_t cookie, String *target) {
		auto nameReply = _connection->perform(_xcb->xcb_get_atom_name_reply, cookie);
		if (nameReply) {
			auto name = _xcb->xcb_get_atom_name_name(nameReply);
			auto len = _xcb->xcb_get_atom_name_name_length(nameReply);

			*target = String(name, len);
		}
	};

	auto parseEdidReply = [&](xcb_randr_get_output_property_cookie_t cookie, PhysicalDisplay *mon) {
		auto reply = _connection->perform(_xcb->xcb_randr_get_output_property_reply, cookie);
		if (reply) {
			auto data = _xcb->xcb_randr_get_output_property_data(reply);
			auto len = _xcb->xcb_randr_get_output_property_data_length(reply);

			mon->id.edid = core::EdidInfo::parse(BytesView(data, len));
		}
	};

	auto parseMonitorsReply = [&](xcb_randr_get_monitors_cookie_t cookie) {
		auto reply = _connection->perform(_xcb->xcb_randr_get_monitors_reply, cookie);
		if (!reply) {
			return;
		}

		auto it = _xcb->xcb_randr_get_monitors_monitors_iterator(reply);
		auto nmonitors = _xcb->xcb_randr_get_monitors_monitors_length(reply);

		uint32_t index = 0;
		while (nmonitors > 0) {
			Vector<const XrandrCrtcInfo *> crtcs;
			auto m = it.data;

			auto out = _xcb->xcb_randr_monitor_info_outputs(m);
			auto outLen = _xcb->xcb_randr_monitor_info_outputs_length(m);

			while (outLen > 0) {
				auto it = cfg->outputs.find(*out);
				if (it != cfg->outputs.end()) {
					emplace_ordered(crtcs, it->second.crtc);
				}
				++out;
				--outLen;
			}

			for (auto &it : crtcs) {
				auto &log = ret->logical.emplace_back(LogicalDisplay{
					it->crtc,
					IRect{m->x, m->y, m->width, m->height},
					std::max(it->scaleX, it->scaleY),
					it->rotation,
				});
				for (auto &oIt : it->outputs) {
					for (auto &mIt : ret->monitors) {
						if (mIt.xid.xid == oIt->output) {
							mIt.index = index;
							mIt.mm = Extent2(m->width_in_millimeters, m->height_in_millimeters);
							log.monitors.emplace_back(mIt.id);
						}
					}
					if (oIt->primary) {
						log.primary = true;
					}
				}
			}

			_xcb->xcb_randr_monitor_info_next(&it);
			--nmonitors;
			++index;
		}
	};

	auto screenInfoCookie =
			_xcb->xcb_randr_get_screen_info_unchecked(_connection->getConnection(), _root);

	auto screenResCurrentCookie =
			_xcb->xcb_randr_get_screen_resources_current(_connection->getConnection(), _root);

	_xcb->xcb_flush(_connection->getConnection());

	parseScreenInfo(screenInfoCookie);

	parseScreenResourcesCurrentReply(screenResCurrentCookie);

	Vector<OutputCookie> outputCookies;
	Vector<Pair<xcb_randr_get_crtc_info_cookie_t, XrandrCrtcInfo *>> crtcCookies;
	Vector<Pair<xcb_randr_get_panning_cookie_t, XrandrCrtcInfo *>> panningCookies;
	Vector<Pair<xcb_randr_get_crtc_transform_cookie_t, XrandrCrtcInfo *>> transformCookies;

	for (auto &oit : cfg->outputs) {
		auto infoCookie = _xcb->xcb_randr_get_output_info(_connection->getConnection(),
				oit.second.output, ret->serial);
		auto listCookie = _xcb->xcb_randr_list_output_properties(_connection->getConnection(),
				oit.second.output);
		outputCookies.emplace_back(OutputCookie{infoCookie, listCookie, &oit.second});
	}

	for (auto &cit : cfg->crtcs) {
		auto infoCookie = _xcb->xcb_randr_get_crtc_info(_connection->getConnection(),
				cit.second.crtc, ret->serial);
		crtcCookies.emplace_back(infoCookie, &cit.second);

		auto panningCookie =
				_xcb->xcb_randr_get_panning(_connection->getConnection(), cit.second.crtc);
		panningCookies.emplace_back(panningCookie, &cit.second);

		auto transformCookie =
				_xcb->xcb_randr_get_crtc_transform(_connection->getConnection(), cit.second.crtc);
		transformCookies.emplace_back(transformCookie, &cit.second);
	}

	auto outputPrimaryCookie =
			_xcb->xcb_randr_get_output_primary(_connection->getConnection(), _root);

	_xcb->xcb_flush(_connection->getConnection());

	for (auto &it : outputCookies) { parseOutputInfo(it); }
	for (auto &it : crtcCookies) { parseCrtcInfo(it.first, it.second); }
	for (auto &it : panningCookies) { parsePanning(it.first, it.second); }
	for (auto &it : transformCookies) { parseTransform(it.first, it.second); }

	auto outputPrimaryReply =
			_connection->perform(_xcb->xcb_randr_get_output_primary_reply, outputPrimaryCookie);
	if (outputPrimaryReply) {
		auto it = cfg->outputs.find(outputPrimaryReply->output);
		if (it != cfg->outputs.end()) {
			it->second.primary = true;
		}
	}

	Vector<Pair<xcb_get_atom_name_cookie_t, String *>> atomCookies;

	for (auto &it : cfg->outputs) {
		for (auto &pIt : it.second.properties) {
			atomCookies.emplace_back(
					_xcb->xcb_get_atom_name(_connection->getConnection(), pIt.atom), &pIt.name);
		}
	}

	_xcb->xcb_flush(_connection->getConnection());

	for (auto &it : atomCookies) { parseAtomNames(it.first, it.second); }

	atomCookies.clear();

	Vector<Pair<xcb_randr_get_output_property_cookie_t, PhysicalDisplay *>> edidCookies;

	for (auto &oIt : cfg->outputs) {
		if (!oIt.second.modes.empty()) {
			auto &mon = ret->monitors.emplace_back(PhysicalDisplay{
				oIt.second.output,
				0,
				{
					oIt.second.name,
				},
			});
			for (auto &it : oIt.second.modes) {
				auto &mode = mon.modes.emplace_back(*it);
				if (oIt.second.crtc && oIt.second.crtc->mode == it) {
					mode.current = true;
				}
				if (it == oIt.second.preferred) {
					mode.preferred = true;
				}
			}
		}
	}

	for (auto &it : ret->monitors) {
		auto oIt = cfg->outputs.find(it.xid);
		for (auto &pIt : oIt->second.properties) {
			if (pIt.name == "EDID") {
				edidCookies.emplace_back(
						_xcb->xcb_randr_get_output_property(_connection->getConnection(),
								oIt->second.output, pIt.atom, 0, 0, 256, 0, 0),
						&it);
			}
		}
	}

	auto monitorsCookie = _xcb->xcb_randr_get_monitors(_connection->getConnection(), _root, 1);

	_xcb->xcb_flush(_connection->getConnection());

	for (auto &it : edidCookies) { parseEdidReply(it.first, it.second); }

	parseMonitorsReply(monitorsCookie);

	ret->native = cfg;

	if (cb) {
		cb(ret);
	}

	handleConfigChanged(ret);
}

void XcbDisplayConfigManager::prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&cb) {
	updateDisplayConfig(sp::move(cb));
}

void XcbDisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig> config,
		Function<void(Status)> &&cb) {
	if (!_connection) {
		cb(Status::ErrorInvalidArguemnt);
		return;
	}

	auto current = extractCurrentConfig(getCurrentConfig());

	auto size = config->getSize();
	auto sizeMM = config->getSizeMM();

	_xcb->xcb_grab_server(_connection->getConnection());

	Vector<xcb_randr_set_crtc_config_cookie_t> updateCookies;

	_xcb->xcb_randr_set_screen_size(_connection->getConnection(),
			_connection->getDefaultScreen()->root, size.width, size.height, sizeMM.width,
			sizeMM.height);

	auto native = config->native.get_cast<XrandrConfig>();

	for (auto &it : config->logical) {
		Vector<uint32_t> outputs;
		uint32_t modeId = 0;

		for (auto &mId : it.monitors) {
			auto mon = config->getMonitor(mId);
			if (mon) {
				outputs.emplace_back(mon->xid);
				modeId = mon->getCurrent().xid;
			}
		}

		auto crtcIt = native->crtcs.find(it.xid);
		if (crtcIt != native->crtcs.end()) {
			auto &crtc = crtcIt->second;

			updateCookies.emplace_back(_xcb->xcb_randr_set_crtc_config(_connection->getConnection(),
					it.xid, XCB_CURRENT_TIME, XCB_CURRENT_TIME, it.rect.x, it.rect.y, modeId,
					it.transform, outputs.size(), outputs.data()));

			_xcb->xcb_randr_set_crtc_transform(_connection->getConnection(), it.xid, crtc.transform,
					crtc.filterName.size(),
					crtc.filterName.empty() ? nullptr : crtc.filterName.data(),
					crtc.filterParams.size(),
					crtc.filterParams.empty() ? nullptr : crtc.filterParams.data());
		}

		log::source().debug("XcbDisplayConfigManager", "Update: ", it.monitors.front().name, " ",
				it.rect.x, " ", it.rect.y);
	}

	Status status = Status::Ok;

	for (auto &it : updateCookies) {
		auto reply = _connection->perform(_xcb->xcb_randr_set_crtc_config_reply, it);
		if (!reply) {
			log::source().error("XcbDisplayConfigManager", "Fail to update CRTC");
			status = Status::ErrorInvalidArguemnt;
		}
	}

	_xcb->xcb_ungrab_server(_connection->getConnection());

	_waitForConfigNotification.emplace_back([cb = sp::move(cb), status] {
		if (cb) {
			cb(status);
		}
	});
}

} // namespace stappler::xenolith::platform
