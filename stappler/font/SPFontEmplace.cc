/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>

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

#include "SPFontEmplace.h"
#include "SPGeometry.h"

namespace STAPPLER_VERSIONIZED stappler::font {

namespace {

constexpr uint32_t LAYOUT_PADDING = 1;

struct LayoutNodeMemory;

struct LayoutNodeMemoryStorage {
	const EmplaceCharInterface *interface = nullptr;
	LayoutNodeMemory *free = nullptr;
	memory::pool_t *pool = nullptr;

	LayoutNodeMemoryStorage(const EmplaceCharInterface *, memory::pool_t *);

	LayoutNodeMemory *alloc(const geom::URect &rect);
	LayoutNodeMemory *alloc(const geom::UVec2 &origin, void *c);
	void release(LayoutNodeMemory *node);
};

struct LayoutNodeMemory : memory::AllocPool {
	LayoutNodeMemory *_child[2];
	geom::URect _rc;
	void *_char = nullptr;

	LayoutNodeMemory(const geom::URect &rect);
	LayoutNodeMemory(const LayoutNodeMemoryStorage &, const geom::UVec2 &origin, void *c);

	bool insert(LayoutNodeMemoryStorage &, void *c);
	size_t nodes() const;
	void finalize(LayoutNodeMemoryStorage &, uint8_t tex);
};

LayoutNodeMemoryStorage::LayoutNodeMemoryStorage(const EmplaceCharInterface *i, memory::pool_t *p)
: interface(i), pool(p) { }

LayoutNodeMemory *LayoutNodeMemoryStorage::alloc(const geom::URect &rect) {
	if (free) {
		auto node = free;
		free = node->_child[0];
		return new (node) LayoutNodeMemory(rect);
	}
	return new (pool) LayoutNodeMemory(rect);
}

LayoutNodeMemory *LayoutNodeMemoryStorage::alloc(const geom::UVec2 &origin, void *c) {
	if (free) {
		auto node = free;
		free = node->_child[0];
		return new (node) LayoutNodeMemory(*this, origin, c);
	}
	return new (pool) LayoutNodeMemory(*this, origin, c);
}

void LayoutNodeMemoryStorage::release(LayoutNodeMemory *node) {
	node->_child[0] = nullptr;
	node->_child[1] = nullptr;
	node->_rc = geom::URect(0, 0, 0, 0);
	node->_char = nullptr;
	if (free) {
		node->_child[0] = free;
	}
	free = node;
}

LayoutNodeMemory::LayoutNodeMemory(const geom::URect &rect) {
	_rc = rect;
	_child[0] = nullptr;
	_child[1] = nullptr;
}

LayoutNodeMemory::LayoutNodeMemory(const LayoutNodeMemoryStorage &storage,
		const geom::UVec2 &origin, void *c) {
	_child[0] = nullptr;
	_child[1] = nullptr;
	_char = c;
	_rc = geom::URect{origin.x, origin.y, storage.interface->getWidth(c),
		storage.interface->getHeight(c)};
}

bool LayoutNodeMemory::insert(LayoutNodeMemoryStorage &storage, void *c) {
	if (_child[0] && _child[1]) {
		return _child[0]->insert(storage, c) || _child[1]->insert(storage, c);
	} else {
		if (_char) {
			return false;
		}

		const auto iwidth = storage.interface->getWidth(c);
		const auto iheight = storage.interface->getHeight(c);

		if (_rc.width < iwidth || _rc.height < iheight) {
			return false;
		}

		if (_rc.width == iwidth || _rc.height == iheight) {
			if (_rc.height == iheight) {
				_child[0] =
						storage.alloc(_rc.origin(), c); // new (pool) LayoutNode(_rc.origin(), c);
				_child[1] =
						storage.alloc(geom::URect{uint32_t(_rc.x + iwidth + LAYOUT_PADDING), _rc.y,
							(_rc.width > iwidth + LAYOUT_PADDING)
									? uint32_t(_rc.width - iwidth - LAYOUT_PADDING)
									: uint32_t(0),
							_rc.height});
			} else if (_rc.width == iwidth) {
				_child[0] =
						storage.alloc(_rc.origin(), c); // new (pool) LayoutNode(_rc.origin(), c);
				_child[1] = storage.alloc(
						geom::URect{_rc.x, uint32_t(_rc.y + iheight + LAYOUT_PADDING), _rc.width,
							(_rc.height > iheight + LAYOUT_PADDING)
									? uint32_t(_rc.height - iheight - LAYOUT_PADDING)
									: uint32_t(0)});
			}
			return true;
		}

		//(decide which way to split)
		int16_t dw = _rc.width - iwidth;
		int16_t dh = _rc.height - iheight;

		if (dw > dh) {
			_child[0] = storage.alloc(geom::URect{_rc.x, _rc.y, iwidth, _rc.height});
			_child[1] = storage.alloc(geom::URect{uint32_t(_rc.x + iwidth + LAYOUT_PADDING), _rc.y,
				uint32_t(dw - LAYOUT_PADDING), _rc.height});
		} else {
			_child[0] = storage.alloc(geom::URect{_rc.x, _rc.y, _rc.width, iheight});
			_child[1] = storage.alloc(geom::URect{_rc.x, uint32_t(_rc.y + iheight + LAYOUT_PADDING),
				_rc.width, uint32_t(dh - LAYOUT_PADDING)});
		}

		_child[0]->insert(storage, c);

		return true;
	}
}

size_t LayoutNodeMemory::nodes() const {
	if (_char) {
		return 1;
	} else if (_child[0] && _child[1]) {
		return _child[0]->nodes() + _child[1]->nodes();
	} else {
		return 0;
	}
}

void LayoutNodeMemory::finalize(LayoutNodeMemoryStorage &storage, uint8_t tex) {
	if (_char) {
		storage.interface->setX(_char, _rc.x);
		storage.interface->setY(_char, _rc.y);
		storage.interface->setTex(_char, tex);
	} else {
		if (_child[0]) {
			_child[0]->finalize(storage, tex);
		}
		if (_child[1]) {
			_child[1]->finalize(storage, tex);
		}
	}
	storage.release(this);
}

} // namespace

geom::Extent2 emplaceChars(const EmplaceCharInterface &iface, const SpanView<void *> &layoutData,
		float totalSquare) {
	if (std::isnan(totalSquare)) {
		totalSquare = 0.0f;
		for (auto &it : layoutData) { totalSquare += iface.getWidth(it) * iface.getHeight(it); }
	}

	// find potential best match square
	bool s = true;
	uint32_t h = 128, w = 128;
	uint32_t sq2 = h * w;
	for (; sq2 < totalSquare; sq2 *= 2, s = !s) {
		if (s) {
			w *= 2;
		} else {
			h *= 2;
		}
	}

	LayoutNodeMemoryStorage storage(&iface, memory::pool::acquire());

	while (true) {
		auto l = storage.alloc(geom::URect{0, 0, w, h});
		for (auto &it : layoutData) {
			if (!l->insert(storage, it)) {
				break;
			}
		}

		auto nodes = l->nodes();
		if (nodes == layoutData.size()) {
			l->finalize(storage, 0);
			storage.release(l);
			break;
		} else {
			if (s) {
				w *= 2;
			} else {
				h *= 2;
			}
			sq2 *= 2;
			s = !s;
		}
		storage.release(l);
	}

	return geom::Extent2(w, h);
}

} // namespace stappler::font
