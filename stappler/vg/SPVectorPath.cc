/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "SPVectorPath.h"
#include "SPData.h"
#include "SPFilepath.h"
#include "SPMemory.h"

namespace STAPPLER_VERSIONIZED stappler::vg {

VectorPath::VectorPath() { }

VectorPath::VectorPath(size_t count) {
	_data.getWriter().reserve(count);
}

bool VectorPath::init() {
	return true;
}

bool VectorPath::init(StringView path) {
	_data.clear();

	return _data.getWriter().readFromPathString(path);
}

bool VectorPath::init(const FileInfo &str) {
	_data.clear();

	return _data.getWriter().readFromFile(str);
}

bool VectorPath::init(BytesView data) {
	_data.clear();

	return _data.getWriter().readFromBytes(data);
}

bool VectorPath::init(const PathData<memory::StandartInterface> &data) {
	_data.clear();
	_data = data;
	return true;
}

bool VectorPath::init(const PathData<memory::PoolInterface> &data) {
	_data.clear();
	_data.params = data.params;
	_data.points = makeSpanView(data.points).vec<Interface>();
	_data.commands = makeSpanView(data.commands).vec<Interface>();
	return true;
}

VectorPath::VectorPath(const VectorPath &path) : _data(path._data) { }

VectorPath &VectorPath::operator=(const VectorPath &path) {
	_data = path._data;
	return *this;
}

VectorPath::VectorPath(VectorPath &&path) : _data(move(path._data)) { }

VectorPath &VectorPath::operator=(VectorPath &&path) {
	_data = move(path._data);
	return *this;
}

VectorPath & VectorPath::addPath(const VectorPath &path) {
	_data.getWriter().addPath(path._data);
	return *this;
}

VectorPath & VectorPath::addPath(StringView str) {
	_data.getWriter().addPath(str);
	return *this;
}

VectorPath & VectorPath::addPath(BytesView data) {
	_data.getWriter().addPath(data);
	return *this;
}

size_t VectorPath::count() const {
	return _data.commands.size();
}

VectorPath & VectorPath::openForWriting(const Callback<void(PathWriter &)> &cb) {
	auto writer = _data.getWriter();
	cb(writer);
	return *this;
}

VectorPath & VectorPath::setFillColor(const Color4B &color) {
	_data.params.fillColor = color;
	return *this;
}
VectorPath & VectorPath::setFillColor(const Color3B &color, bool preserveOpacity) {
	_data.params.fillColor = Color4B(color, preserveOpacity?_data.params.fillColor.a:255);
	return *this;
}
VectorPath & VectorPath::setFillColor(const Color &color, bool preserveOpacity) {
	_data.params.fillColor = Color4B(color, preserveOpacity?_data.params.fillColor.a:255);
	return *this;
}
const Color4B &VectorPath::getFillColor() const {
	return _data.params.fillColor;
}

VectorPath & VectorPath::setStrokeColor(const Color4B &color) {
	_data.params.strokeColor = color;
	return *this;
}
VectorPath & VectorPath::setStrokeColor(const Color3B &color, bool preserveOpacity) {
	_data.params.strokeColor = Color4B(color, preserveOpacity?_data.params.fillColor.a:255);
	return *this;
}
VectorPath & VectorPath::setStrokeColor(const Color &color, bool preserveOpacity) {
	_data.params.strokeColor = Color4B(color, preserveOpacity?_data.params.fillColor.a:255);
	return *this;
}
const Color4B &VectorPath::getStrokeColor() const {
	return _data.params.strokeColor;
}

VectorPath & VectorPath::setFillOpacity(uint8_t value) {
	_data.params.fillColor.a = value;
	return *this;
}
uint8_t VectorPath::getFillOpacity() const {
	return _data.params.fillColor.a;
}

VectorPath & VectorPath::setStrokeOpacity(uint8_t value) {
	_data.params.strokeColor.a = value;
	return *this;
}
uint8_t VectorPath::getStrokeOpacity() const {
	return _data.params.strokeColor.a;
}

VectorPath & VectorPath::setStrokeWidth(float width) {
	_data.params.strokeWidth = width;
	return *this;
}

float VectorPath::getStrokeWidth() const {
	return _data.params.strokeWidth;
}

VectorPath &VectorPath::setWindingRule(Winding value) {
	_data.params.winding = value;
	return *this;
}
vg::Winding VectorPath::getWindingRule() const {
	return _data.params.winding;
}

VectorPath &VectorPath::setLineCup(LineCup value) {
	_data.params.lineCup = value;
	return *this;
}
vg::LineCup VectorPath::getLineCup() const {
	return _data.params.lineCup;
}

VectorPath &VectorPath::setLineJoin(LineJoin value) {
	_data.params.lineJoin = value;
	return *this;
}
vg::LineJoin VectorPath::getLineJoin() const {
	return _data.params.lineJoin;
}

VectorPath &VectorPath::setMiterLimit(float value) {
	_data.params.miterLimit = value;
	return *this;
}
float VectorPath::getMiterLimit() const {
	return _data.params.miterLimit;
}

VectorPath & VectorPath::setStyle(DrawStyle s) {
	_data.params.style = s;
	return *this;
}

vg::DrawFlags VectorPath::getStyle() const {
	return _data.params.style;
}

VectorPath &VectorPath::setAntialiased(bool val) {
	_data.params.isAntialiased = val;
	return *this;
}
bool VectorPath::isAntialiased() const {
	return _data.params.isAntialiased;
}

VectorPath & VectorPath::setTransform(const Mat4 &t) {
	_data.params.transform = t;
	return *this;
}
VectorPath & VectorPath::applyTransform(const Mat4 &t) {
	_data.params.transform *= t;
	return *this;
}
const Mat4 &VectorPath::getTransform() const {
	return _data.params.transform;
}

VectorPath & VectorPath::clear() {
	if (!empty()) {
		_data.clear();
	}
	return *this;
}

VectorPath & VectorPath::setParams(const PathParams &p) {
	_data.params = p;
	return *this;
}

PathParams VectorPath::getParams() const {
	return _data.params;
}

bool VectorPath::empty() const {
	return _data.commands.empty();
}

void VectorPath::reserve(size_t s) {
	_data.getWriter().reserve(s);
}

const Interface::VectorType<Command> &VectorPath::getCommands() const {
	return _data.commands;
}

const Interface::VectorType<CommandData> &VectorPath::getPoints() const {
	return _data.points;
}

Interface::BytesType VectorPath::encode() const {
	return _data.encode<Interface>();
}

Interface::StringType VectorPath::toString(bool newline) const {
	return _data.toString<Interface>();
}

size_t VectorPath::commandsCount() const {
	return _data.commands.size();
}
size_t VectorPath::dataCount() const {
	return _data.points.size();
}

PathWriter VectorPath::getWriter() {
	return PathWriter(_data);
}

}
