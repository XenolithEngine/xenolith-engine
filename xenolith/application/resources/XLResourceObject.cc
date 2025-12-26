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

#include "XLResourceObject.h"
#include "XLTemporaryResource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

ResourceObject::~ResourceObject() { }

bool ResourceObject::init(ResourceType type) {
	_type = type;
	return true;
}

bool ResourceObject::init(ResourceType type, const Rc<core::Resource> &res) {
	_type = type;
	_resource = res;
	return true;
}

bool ResourceObject::init(ResourceType type, const Rc<TemporaryResource> &tmp) {
	_type = type;
	_temporary = tmp;
	return true;
}

void ResourceObject::invalidate() {
	//_temporary = nullptr;
}

StringView ResourceObject::getName() const { return StringView(); }

bool ResourceObject::isLoaded() const {
	return !_temporary || (_temporary && _temporary->isLoaded());
}

void ResourceObject::handleEnter(ResourceOwner *scene) {
	if (_temporary) {
		_temporary->handleEnter(scene, this);
	}
}

void ResourceObject::handleExit(ResourceOwner *scene) {
	if (_temporary) {
		_temporary->handleExit(scene, this);
	}
}

Rc<TemporaryResource> ResourceObject::getTemporary() const { return _temporary; }

} // namespace stappler::xenolith
