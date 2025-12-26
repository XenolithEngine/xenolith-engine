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

#ifndef XENOLITH_APPLICATION_RESOURCES_XLRESOURCEOBJECT_H_
#define XENOLITH_APPLICATION_RESOURCES_XLRESOURCEOBJECT_H_

#include "XLResourceOwner.h"
#include "XLCoreResource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class TemporaryResource;

enum class ResourceType {
	Texture,
	MeshIndex
};

class SP_PUBLIC ResourceObject : public NamedRef {
public:
	virtual ~ResourceObject();

	virtual bool init(ResourceType);
	virtual bool init(ResourceType, const Rc<core::Resource> &);
	virtual bool init(ResourceType, const Rc<TemporaryResource> &);

	virtual void invalidate();

	virtual StringView getName() const;

	virtual bool isLoaded() const;

	virtual void handleEnter(ResourceOwner *);
	virtual void handleExit(ResourceOwner *);

	ResourceType getType() const { return _type; }
	Rc<TemporaryResource> getTemporary() const;

protected:
	ResourceType _type = ResourceType::Texture;
	Rc<core::Resource> _resource;
	Rc<TemporaryResource> _temporary;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_CORE_BASE_RESOURCES_XLRESOURCEOBJECT_H_ */
