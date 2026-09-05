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

#include "XLUiPanelRegistry.h"
#include "XLUiPanelHost.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool PanelRegistry::init() { return true; }

// --- descriptors -----------------------------------------------------------

void PanelRegistry::registerPanel(DockPanelDescriptor &&desc) {
	if (desc.id.empty()) {
		log::source().error("ui::PanelRegistry", "a panel descriptor needs a non-empty id");
		return;
	}
	auto id = desc.id;
	_descriptors.insert_or_assign(sp::move(id), sp::move(desc));
}

void PanelRegistry::unregisterPanel(StringView id) {
	auto key = id.str<Interface>();

	// Close it where it is FIRST: the host has to take it out of its own structure while the
	// descriptor is still here to be read (a dock frame re-measures itself from the descriptors of
	// what it holds). Only then may the entry go.
	if (auto it = _hosts.find(key); it != _hosts.end()) {
		auto host = it->second;
		_hosts.erase(it);
		host->closePanel(id);
	}

	_descriptors.erase(key);
	_content.erase(key);
}

const DockPanelDescriptor *PanelRegistry::getPanelDescriptor(StringView id) const {
	auto it = _descriptors.find(id.str<Interface>());
	return (it != _descriptors.end()) ? &it->second : nullptr;
}

// --- content and the host claim --------------------------------------------

bool PanelRegistry::isInSubtree(const Node *node, const Node *container) {
	if (!node || !container) {
		return false;
	}
	for (auto p = container; p != nullptr; p = p->getParent()) {
		if (p == node) {
			return true;
		}
	}
	return false;
}

Node *PanelRegistry::acquireContent(StringView panelId, NotNull<PanelHost> forHost) {
	auto key = panelId.str<Interface>();

	sprt_passert(_releasing != key,
			"PanelHost::releasePanel must not acquire content for the panel it is releasing");

	auto desc = getPanelDescriptor(panelId);
	if (!desc) {
		return nullptr;
	}

	Node *raw = nullptr;
	if (auto it = _content.find(key); it != _content.end()) {
		raw = it->second.get();
	} else {
		if (!desc->builder) {
			return nullptr;
		}
		auto node = desc->builder();
		if (!node) {
			log::source().error("ui::PanelRegistry", "the builder of panel '", panelId,
					"' returned nothing");
			return nullptr;
		}
		raw = node.get();
		_content.emplace(key, sp::move(node));
	}

	// A container registered as one of its own panels would be parented into its own descendant:
	// an infinite tree, and a crash the moment anything walks it. Refuse instead - and say so, since
	// nothing else about the resulting empty body would explain itself.
	if (isInSubtree(raw, forHost->getPanelDecoratorParent())) {
		log::source().error("ui::PanelRegistry", "panel '", panelId,
				"' cannot be parked inside itself");
		return nullptr;
	}

	auto it = _hosts.find(key);
	if (it != _hosts.end() && it->second == forHost.get()) {
		return raw; // already ours; nothing to negotiate
	}

	if (it != _hosts.end()) {
		auto prev = it->second;

		// Record the new owner BEFORE calling out: the release will restructure the old host, and
		// anything that asks who holds this panel while that runs must already see the answer.
		it->second = forHost;

		auto releasing = sp::move(_releasing);
		_releasing = key;
		prev->releasePanel(panelId);
		_releasing = sp::move(releasing);
	} else {
		_hosts.emplace(sp::move(key), forHost.get());
	}

	return raw;
}

void PanelRegistry::foreachContent(const Callback<void(StringView, Node *)> &cb) const {
	for (auto &[id, node] : _content) {
		if (node) {
			cb(id, node.get());
		}
	}
}

PanelHost *PanelRegistry::getHost(StringView panelId) const {
	auto it = _hosts.find(panelId.str<Interface>());
	return (it != _hosts.end()) ? it->second : nullptr;
}

void PanelRegistry::addHost(NotNull<PanelHost> host) {
	if (sprt::find(_attached.begin(), _attached.end(), host.get()) == _attached.end()) {
		_attached.emplace_back(host);
	}
}

void PanelRegistry::removeHost(NotNull<PanelHost> host) {
	if (auto it = sprt::find(_attached.begin(), _attached.end(), host.get());
			it != _attached.end()) {
		_attached.erase(it);
	}
}

void PanelRegistry::releaseHost(NotNull<PanelHost> host) {
	// Erase-while-iterating over a Map is fine node-by-node, but the host is gone either way, so
	// collecting first keeps this readable and immune to whatever the erase invalidates.
	Vector<String> claimed;
	for (auto &[id, it] : _hosts) {
		if (it == host.get()) {
			claimed.emplace_back(id);
		}
	}
	for (auto &id : claimed) { _hosts.erase(id); }

	removeHost(host);
}

} // namespace stappler::xenolith::ui
