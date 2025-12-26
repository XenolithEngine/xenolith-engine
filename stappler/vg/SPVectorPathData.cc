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

#include "SPVectorPathData.h"
#include "SPFilesystem.h"

namespace STAPPLER_VERSIONIZED stappler::vg {

#define SP_PATH_LOG(...)
//#define SP_PATH_LOG(...) stappler::log::format(log::Debug, "Path Debug", __VA_ARGS__)

#define SP_PATH_LOG_TEXT(...)
//#define SP_PATH_LOG_TEXT(...) stappler::log::text(log::Debug, "Path Debug", __VA_ARGS__)


// to prevent math errors on relative values we use double for SVG reader
// Path itself uses single-word float for performance
class SVGPathReader {
public:
	static bool readFileContent(PathWriter *p, StringView content) {
		StringView r(content);
		r.skipUntilString("<path ");
		if (!r.is("<path ")) {
			return false;
		}

		r.skipString("<path ");
		StringView pathContent = r.readUntil<StringView::Chars<'>'>>();
		pathContent.skipUntilString("d=\"");
		if (pathContent.is("d=\"")) {
			pathContent.skipString("d=\"");
			return readPath(p, pathContent.readUntil<StringView::Chars<'"'>>());
		}
		return false;
	}

	static bool readFile(PathWriter *p, const FileInfo &str) {
		if (!str.path.empty()) {
			auto content = filesystem::readTextFile<memory::StandartInterface>(str);
			return readFileContent(p, content);
		}
		return false;
	}

	static bool readPath(PathWriter *p, const StringView &r) {
		if (r.size() > 0) {
			SVGPathReader reader(p, r);
			return reader.parse();
		}
		return false;
	}

protected:
	bool parse() {
		while (!reader.empty()) {
			if (!readCmdGroup()) {
				return false;
			}
		}

		return true;
	}

	bool readCmdGroup() {
		readWhitespace();
		while (!reader.empty()) {
			if (!readCmd()) {
				return false;
			}
		}
		return true;
	}

	bool readCmd() {
		if (!readMoveTo()) {
			return false;
		}
		readWhitespace();

		bool readNext = true;
		while (readNext) {
			readNext = readDrawTo();
			if (readNext) {
				readWhitespace();
			}
		}

		return true;
	}

	bool readMoveTo() {
		if (reader >= 1) {
			readWhitespace();
			bool relative = true;
			if (reader.is('M')) {
				relative = false;
				++reader;
			} else if (reader.is('m')) {
				relative = true;
				++reader;
			} else {
				return false;
			}

			readWhitespace();
			return readMoveToArgs(relative);
		}
		return false;
	}

	bool readDrawTo() {
		if (reader >= 1) {
			auto c = reader[0];
			++reader;

			readWhitespace();
			if (c == 'M' || c == 'm') {
				return readMoveToArgs(c == 'm');
			} else if (c == 'Z' || c == 'z') {
				SP_PATH_LOG("Z");
				if (_pathStarted) {
					_x = _sx;
					_y = _sy;
					_pathStarted = false;
				}
				path->closePath();
				return true;
			} else if (c == 'L' || c == 'l') {
				return readLineToArgs(c == 'l');
			} else if (c == 'H' || c == 'h') {
				return readHorizontalLineTo(c == 'h');
			} else if (c == 'V' || c == 'v') {
				return readVerticalLineTo(c == 'v');
			} else if (c == 'C' || c == 'c') {
				return readCubicBezier(c == 'c');
			} else if (c == 'S' || c == 's') {
				return readCubicBezierShort(c == 's');
			} else if (c == 'Q' || c == 'q') {
				return readQuadraticBezier(c == 'q');
			} else if (c == 'T' || c == 't') {
				return readQuadraticBezierShort(c == 't');
			} else if (c == 'A' || c == 'a') {
				return readEllipticalArc(c == 'a');
			}
		}
		return false;
	}

	bool readLineToArgs(bool relative) {
		double x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readCoordPair(x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) {
					ret = true;
					first = false;
				}
				if (relative) {
					x = _x + x;
					y = _y + y;
				}

				SP_PATH_LOG("L %f %f (%f %f)", x, y, x - _x, y - _y);
				_x = x;
				_y = y;
				_b = false;
				path->lineTo(x, y);
			}
		}
		return ret;
	}

	bool readHorizontalLineTo(bool relative) {
		double x;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readNumber(x);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) {
					ret = true;
					first = false;
				}
				if (relative) {
					x = _x + x;
				}

				SP_PATH_LOG("H %f (%f)", x, x - _x);
				_x = x;
				_b = false;
				path->lineTo(x, _y);
			}
		}
		return ret;
	}

	bool readVerticalLineTo(bool relative) {
		double y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readNumber(y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) {
					ret = true;
					first = false;
				}
				if (relative) {
					y = _y + y;
				}

				SP_PATH_LOG("V %f (%f)", y, y - _y);
				_y = y;
				_b = false;
				path->lineTo(_x, y);
			}
		}
		return ret;
	}

	bool readCubicBezier(bool relative) {
		double x1, y1, x2, y2, x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readCurveToArg(x1, y1, x2, y2, x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) {
					ret = true;
					first = false;
				}
				if (relative) {
					x1 = _x + x1;
					y1 = _y + y1;
					x2 = _x + x2;
					y2 = _y + y2;
					x = _x + x;
					y = _y + y;
				}
				_x = x;
				_y = y;
				_bx = x2;
				_by = y2;
				_b = true;
				SP_PATH_LOG("C %f %f %f %f %f %f", x1, y1, x2, y2, x, y);
				path->cubicTo(x1, y1, x2, y2, x, y);
			}
		}
		return ret;
	}

	bool readCubicBezierShort(bool relative) {
		double x1, y1, x2, y2, x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readSmoothCurveToArg(x2, y2, x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) {
					ret = true;
					first = false;
				}

				getNewBezierParams(x1, y1);
				if (relative) {
					x2 = _x + x2;
					y2 = _y + y2;
					x = _x + x;
					y = _y + y;
				}
				_x = x;
				_y = y;
				_bx = x2;
				_by = y2;
				_b = true;
				SP_PATH_LOG("S (%f %f) %f %f %f %f", x1, y1, x2, y2, x, y);
				path->cubicTo(x1, y1, x2, y2, x, y);
			}
		}
		return ret;
	}

	bool readQuadraticBezier(bool relative) {
		double x1, y1, x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readQuadraticCurveToArg(x1, y1, x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) {
					ret = true;
					first = false;
				}
				if (relative) {
					x1 = _x + x1;
					y1 = _y + y1;
					x = _x + x;
					y = _y + y;
				}
				_x = x;
				_y = y;
				_bx = x1;
				_by = y1;
				_b = true;
				path->quadTo(x1, y1, x, y);
			}
		}
		return ret;
	}

	bool readQuadraticBezierShort(bool relative) {
		double x1, y1, x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readSmoothQuadraticCurveToArg(x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) {
					ret = true;
					first = false;
				}
				getNewBezierParams(x1, y1);
				if (relative) {
					x = _x + x;
					y = _y + y;
				}
				_x = x;
				_y = y;
				_bx = x1;
				_by = y1;
				_b = true;
				path->quadTo(x1, y1, x, y);
			}
		}
		return ret;
	}

	bool readEllipticalArc(bool relative) {
		double rx, ry, xAxisRotation, x, y;
		bool largeArc, sweep;

		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readEllipticalArcArg(rx, ry, xAxisRotation, largeArc, sweep, x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) {
					ret = true;
					first = false;
				}
				if (relative) {
					x = _x + x;
					y = _y + y;
				}

				if (rx == 0 || ry == 0) {
					_x = x;
					_y = y;
					_b = false;
					path->lineTo(x, y);
				} else {
					_x = x;
					_y = y;
					_b = false;
					path->arcTo(rx, ry, xAxisRotation, largeArc, sweep, x, y);
				}
			}
		}
		return ret;
	}

	bool readMoveToArgs(bool relative) {
		double x = 0.0f, y = 0.0f;
		if (!readCoordPair(x, y)) {
			return false;
		}

		if (relative) {
			x = _x + x;
			y = _y + y;
		}

		_b = false;
		_x = x;
		_y = y;
		//if (!_pathStarted) {
		_sx = _x;
		_sy = _y;
		_pathStarted = true;
		//}

		SP_PATH_LOG("M %f %f", _x, _y);
		path->moveTo(x, y);
		readCommaWhitespace();
		readLineToArgs(relative);

		return true;
	}

	bool readCurveToArg(double &x1, double &y1, double &x2, double &y2, double &x, double &y) {
		if (!readCoordPair(x1, y1)) {
			return false;
		}
		readCommaWhitespace();
		if (!readCoordPair(x2, y2)) {
			return false;
		}
		readCommaWhitespace();
		if (!readCoordPair(x, y)) {
			return false;
		}
		return true;
	}

	bool readSmoothCurveToArg(double &x2, double &y2, double &x, double &y) {
		return readQuadraticCurveToArg(x2, y2, x, y);
	}

	bool readQuadraticCurveToArg(double &x1, double &y1, double &x, double &y) {
		if (!readCoordPair(x1, y1)) {
			return false;
		}
		readCommaWhitespace();
		if (!readCoordPair(x, y)) {
			return false;
		}
		return true;
	}

	bool readEllipticalArcArg(double &_rx, double &_ry, double &_xAxisRotation, bool &_largeArc,
			bool &_sweep, double &_dx, double &_dy) {
		double rx, ry, xAxisRotation, x, y;
		bool largeArc, sweep;

		if (!readCoordPair(rx, ry)) {
			return false;
		}
		readCommaWhitespace();
		if (!readNumber(xAxisRotation)) {
			return false;
		}

		if (!readCommaWhitespace()) {
			return false;
		}

		if (!readFlag(largeArc)) {
			return false;
		}

		readCommaWhitespace();
		if (!readFlag(sweep)) {
			return false;
		}

		readCommaWhitespace();
		if (!readCoordPair(x, y)) {
			return false;
		}

		_rx = rx;
		_ry = ry;
		_xAxisRotation = xAxisRotation;
		_largeArc = largeArc;
		_sweep = sweep;
		_dx = x;
		_dy = y;

		return true;
	}

	bool readSmoothQuadraticCurveToArg(double &x, double &y) { return readCoordPair(x, y); }

	bool readCoordPair(double &x, double &y) {
		double value1 = 0.0f, value2 = 0.0f;
		if (!readNumber(value1)) {
			return false;
		}
		readCommaWhitespace();
		if (!readNumber(value2)) {
			return false;
		}

		x = value1;
		y = value2;
		return true;
	}

	bool readWhitespace() { return reader.readChars<StringView::WhiteSpace>().size() != 0; }

	bool readCommaWhitespace() {
		if (reader >= 1) {
			bool ws = readWhitespace();
			if (reader.is(',')) {
				++reader;
			} else {
				return ws;
			}
			readWhitespace();
			return true;
		}
		return false;
	}

	bool readNumber(double &val) {
		if (!reader.empty()) {
			if (!reader.readDouble().grab(val)) {
				return false;
			}
			return true;
		}
		return false;
	}

	bool readFlag(bool &flag) {
		if (reader >= 1) {
			if (reader.is('0') || reader.is('1')) {
				flag = (reader.is('1'));
				++reader;
				return true;
			}
		}
		return false;
	}

	void getNewBezierParams(double &bx, double &by) {
		if (_b) {
			bx = _x * 2 - _bx;
			by = _y * 2 - _by;
		} else {
			bx = _x;
			by = _y;
		}
	}

	SVGPathReader(PathWriter *d, const StringView &r) : path(d), reader(r) { }

	double _x = 0.0f, _y = 0.0f;

	bool _b = false;
	double _bx = 0.0f, _by = 0.0f;

	double _sx = 0.0f, _sy = 0.0f;
	bool _pathStarted = false;
	PathWriter *path = nullptr;
	StringView reader;
};

template <typename Interface>
class PathBinaryEncoder {
public: // utility
	PathBinaryEncoder(typename Interface::BytesType *b) : buffer(b) { }

	void emplace(uint8_t c) { buffer->emplace_back(c); }

	void emplace(const uint8_t *buf, size_t size) {
		size_t tmpSize = buffer->size();
		buffer->resize(tmpSize + size);
		memcpy(buffer->data() + tmpSize, buf, size);
	}

private:
	typename Interface::BytesType *buffer;
};

template <typename Interface, typename Source>
auto encodePath(const PathData<Source> &source) -> typename Interface::BytesType {
	size_t bufferSize = source.commands.size() * sizeof(Command)
			+ source.points.size() * sizeof(CommandData) + 2 * (sizeof(size_t) + 1);

	bool hasUV = false;
	if ((source.params.style & DrawFlags::UV) != DrawFlags::None) {
		bufferSize += source.uv.size() * sizeof(Vec2);
		hasUV = true;
	}

	typename Interface::BytesType ret;
	ret.reserve(bufferSize);

	PathBinaryEncoder<Interface> enc(&ret);

	data::cbor::_writeInt(enc, hasUV ? 2 : 1); // version
	data::cbor::_writeInt(enc, source.commands.size());
	data::cbor::_writeInt(enc, source.points.size());
	auto d = source.points.data();
	auto uv = source.uv.data();
	for (auto &it : source.commands) {
		data::cbor::_writeInt(enc, toInt(it));
		if (hasUV) {
			data::cbor::_writeNumber(enc, uv->x);
			data::cbor::_writeNumber(enc, uv->y);
			++uv;
		}

		switch (it) {
		case Command::MoveTo:
		case Command::LineTo:
			data::cbor::_writeNumber(enc, d[0].p.x);
			data::cbor::_writeNumber(enc, d[0].p.y);
			SP_PATH_LOG_TEXT("L ", d[0].p.x, " ", d[0].p.y);
			++d;
			break;
		case Command::QuadTo:
			data::cbor::_writeNumber(enc, d[0].p.x);
			data::cbor::_writeNumber(enc, d[0].p.y);
			data::cbor::_writeNumber(enc, d[1].p.x);
			data::cbor::_writeNumber(enc, d[1].p.y);
			SP_PATH_LOG_TEXT("Q ", d[0].p.x, " ", d[0].p.y, " ", d[1].p.x, " ", d[1].p.y);
			d += 2;
			break;
		case Command::CubicTo:
			data::cbor::_writeNumber(enc, d[0].p.x);
			data::cbor::_writeNumber(enc, d[0].p.y);
			data::cbor::_writeNumber(enc, d[1].p.x);
			data::cbor::_writeNumber(enc, d[1].p.y);
			data::cbor::_writeNumber(enc, d[2].p.x);
			data::cbor::_writeNumber(enc, d[2].p.y);
			SP_PATH_LOG_TEXT("C ", d[0].p.x, " ", d[0].p.y, " ", d[1].p.x, " ", d[1].p.y, " ",
					d[2].p.x, " ", d[2].p.y);
			d += 3;
			break;
		case Command::ArcTo:
			data::cbor::_writeNumber(enc, d[0].p.x);
			data::cbor::_writeNumber(enc, d[0].p.y);
			data::cbor::_writeNumber(enc, d[1].p.x);
			data::cbor::_writeNumber(enc, d[1].p.y);
			data::cbor::_writeNumber(enc, d[2].f.v);
			data::cbor::_writeInt(enc,
					(uint32_t(d[2].f.a ? 1 : 0) << 1) | uint32_t(d[2].f.b ? 1 : 0));
			SP_PATH_LOG_TEXT("A ", d[0].p.x, " ", d[0].p.y, " ", d[1].p.x, " ", d[1].p.y,
					" " << d[2].f.v, " ",
					((uint32_t(d[2].f.a ? 1 : 0) << 1) | uint32_t(d[2].f.b ? 1 : 0)), " ", d[2].f.a,
					" ", d[2].f.b);
			d += 3;
			break;
		case Command::ClosePath: SP_PATH_LOG_TEXT("Z"); break;
		default: break;
		}
	}

	return ret;
}

template <typename Interface, typename Source>
auto pathToString(const Source &source, bool newline) -> typename Interface::StringType {
	BufferTemplate<Interface> buffer;

	//stream << std::setprecision(std::numeric_limits<double>::max_digits10);
	auto d = source.points.data();
	for (auto &it : source.commands) {
		switch (it) {
		case Command::MoveTo:
			if (newline && d != source.points.data()) {
				buffer.putc('\n');
			}
			buffer.putStrings("M ", d[0].p.x, ",", d[0].p.y, " ");
			++d;
			break;
		case Command::LineTo:
			buffer.putStrings("L ", d[0].p.x, ",", d[0].p.y, " ");
			++d;
			break;
		case Command::QuadTo:
			buffer.putStrings("Q ", d[0].p.x, ",", d[0].p.y, " ", d[1].p.x, ",", d[1].p.y, " ");
			d += 2;
			break;
		case Command::CubicTo:
			buffer.putStrings("C ", d[0].p.x, ",", d[0].p.y, " ", d[1].p.x, ",", d[1].p.y, " ",
					d[2].p.x, ",", d[2].p.y, " ");
			d += 3;
			break;
		case Command::ArcTo:
			buffer.putStrings("A ", d[0].p.x, ",", d[0].p.y, " ", d[2].f.v, " ", int32_t(d[2].f.a),
					" ", int32_t(d[2].f.b), " ", d[1].p.x, ",", d[1].p.y, " ");
			d += 3;
			break;
		case Command::ClosePath: buffer.put("Z ", 2); break;
		default: break;
		}
	}

	if (newline) {
		buffer.putc('\n');
	}

	return buffer.str();
}

template <>
void PathData<memory::PoolInterface>::clear() {
	points.clear();
	commands.clear();
}

template <>
void PathData<memory::StandartInterface>::clear() {
	points.clear();
	commands.clear();
}

template <>
PathWriter PathData<memory::PoolInterface>::getWriter() {
	return PathWriter(*this);
}

template <>
PathWriter PathData<memory::StandartInterface>::getWriter() {
	return PathWriter(*this);
}

template <>
template <>
auto PathData<memory::PoolInterface>::encode<memory::PoolInterface>() const
		-> memory::PoolInterface::BytesType {
	return encodePath<memory::PoolInterface>(*this);
}

template <>
template <>
auto PathData<memory::PoolInterface>::encode<memory::StandartInterface>() const
		-> memory::StandartInterface::BytesType {
	return encodePath<memory::StandartInterface>(*this);
}

template <>
template <>
auto PathData<memory::StandartInterface>::encode<memory::PoolInterface>() const
		-> memory::PoolInterface::BytesType {
	return encodePath<memory::PoolInterface>(*this);
}

template <>
template <>
auto PathData<memory::StandartInterface>::encode<memory::StandartInterface>() const
		-> memory::StandartInterface::BytesType {
	return encodePath<memory::StandartInterface>(*this);
}

template <>
template <>
auto PathData<memory::PoolInterface>::toString<memory::PoolInterface>(bool newline) const
		-> memory::PoolInterface::StringType {
	return pathToString<memory::PoolInterface>(*this, newline);
}

template <>
template <>
auto PathData<memory::PoolInterface>::toString<memory::StandartInterface>(bool newline) const
		-> memory::StandartInterface::StringType {
	return pathToString<memory::StandartInterface>(*this, newline);
}

template <>
template <>
auto PathData<memory::StandartInterface>::toString<memory::PoolInterface>(bool newline) const
		-> memory::PoolInterface::StringType {
	return pathToString<memory::PoolInterface>(*this, newline);
}

template <>
template <>
auto PathData<memory::StandartInterface>::toString<memory::StandartInterface>(bool newline) const
		-> memory::StandartInterface::StringType {
	return pathToString<memory::StandartInterface>(*this, newline);
}

PathWriter::PathWriter(PathData<mem_std::Interface> &d)
: points(d.points), commands(d.commands), uvPoints(d.uv) { }

PathWriter::PathWriter(PathData<mem_pool::Interface> &d)
: points(d.points), commands(d.commands), uvPoints(d.uv) { }

PathWriter::operator bool() const { return points && commands; }

bool PathWriter::empty() const { return commands.empty(); }

void PathWriter::reserve(size_t size) {
	commands.reserve(size);
	uvPoints.reserve(size);
	points.reserve(size * 3);
}

bool PathWriter::readFromPathString(StringView str) {
	commands.clear();
	uvPoints.clear();
	points.clear();

	if (!SVGPathReader::readPath(this, str)) {
		return false;
	}
	return true;
}

bool PathWriter::readFromFileContent(StringView str) {
	commands.clear();
	uvPoints.clear();
	points.clear();

	if (!SVGPathReader::readFileContent(this, str)) {
		return false;
	}
	return true;
}

bool PathWriter::readFromFile(const FileInfo &str) {
	commands.clear();
	uvPoints.clear();
	points.clear();

	if (!SVGPathReader::readFile(this, str)) {
		return false;
	}
	return true;
}

bool PathWriter::readFromBytes(BytesView bytes) {
	commands.clear();
	uvPoints.clear();
	points.clear();

	return addPath(bytes);
}

PathWriter &PathWriter::moveTo(float x, float y, float u, float v) {
	commands.emplace_back(Command::MoveTo);
	uvPoints.emplace_back(Vec2(u, v));
	points.emplace_back(CommandData(x, y));
	return *this;
}

PathWriter &PathWriter::moveTo(const Vec2 &point, const Vec2 &uv) {
	return this->moveTo(point.x, point.y, uv.x, uv.y);
}

PathWriter &PathWriter::lineTo(float x, float y, float u, float v) {
	commands.emplace_back((commands.empty() || commands.back() == Command::ClosePath)
					? Command::MoveTo
					: Command::LineTo);
	uvPoints.emplace_back(Vec2(u, v));
	points.emplace_back(CommandData(x, y));
	return *this;
}
PathWriter &PathWriter::lineTo(const Vec2 &point, const Vec2 &uv) {
	this->lineTo(point.x, point.y, uv.x, uv.y);
	return *this;
}

PathWriter &PathWriter::quadTo(float x1, float y1, float x2, float y2, float u, float v) {
	commands.emplace_back(Command::QuadTo);
	uvPoints.emplace_back(Vec2(u, v));
	points.emplace_back(CommandData(x1, y1));
	points.emplace_back(CommandData(x2, y2));
	return *this;
}
PathWriter &PathWriter::quadTo(const Vec2 &p1, const Vec2 &p2, const Vec2 &uv) {
	this->quadTo(p1.x, p1.y, p2.x, p2.y, uv.x, uv.y);
	return *this;
}

PathWriter &PathWriter::cubicTo(float x1, float y1, float x2, float y2, float x3, float y3, float u,
		float v) {
	commands.emplace_back(Command::CubicTo);
	uvPoints.emplace_back(Vec2(u, v));
	points.emplace_back(CommandData(x1, y1));
	points.emplace_back(CommandData(x2, y2));
	points.emplace_back(CommandData(x3, y3));
	return *this;
}
PathWriter &PathWriter::cubicTo(const Vec2 &p1, const Vec2 &p2, const Vec2 &p3, const Vec2 &uv) {
	this->cubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, uv.x, uv.y);
	return *this;
}

// use _to_rad user suffix to convert from degrees to radians
PathWriter &PathWriter::arcTo(float rx, float ry, float rotation, bool largeFlag, bool sweepFlag,
		float x, float y, float u, float v) {
	commands.emplace_back(Command::ArcTo);
	uvPoints.emplace_back(Vec2(u, v));
	points.emplace_back(CommandData(rx, ry));
	points.emplace_back(CommandData(x, y));
	points.emplace_back(CommandData(rotation, largeFlag, sweepFlag));
	return *this;
}
PathWriter &PathWriter::arcTo(const Vec2 &r, float rotation, bool largeFlag, bool sweepFlag,
		const Vec2 &target, const Vec2 &uv) {
	this->arcTo(r.x, r.y, rotation, largeFlag, sweepFlag, target.x, target.y, uv.x, uv.y);
	return *this;
}

PathWriter &PathWriter::closePath() {
	commands.emplace_back(Command::ClosePath);
	uvPoints.emplace_back(Vec2(nan(), nan()));
	return *this;
}

PathWriter &PathWriter::addRect(const Rect &rect) {
	addRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
	return *this;
}

PathWriter &PathWriter::addRect(const Rect &rect, float rx, float ry) {
	addRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height, rx, ry);
	return *this;
}

PathWriter &PathWriter::addRect(float x, float y, float width, float height) {
	moveTo(x, y);
	lineTo(x + width, y);
	lineTo(x + width, y + height);
	lineTo(x, y + height);
	closePath();
	return *this;
}

PathWriter &PathWriter::addOval(const Rect &oval) {
	addEllipse(oval.getMidX(), oval.getMidY(), oval.size.width / 2.0f, oval.size.height / 2.0f);
	return *this;
}

PathWriter &PathWriter::addCircle(float x, float y, float radius) {
	moveTo(x + radius, y);
	arcTo(radius, radius, 0, false, false, x, y - radius);
	arcTo(radius, radius, 0, false, false, x - radius, y);
	arcTo(radius, radius, 0, false, false, x, y + radius);
	arcTo(radius, radius, 0, false, false, x + radius, y);
	closePath();
	return *this;
}

PathWriter &PathWriter::addEllipse(float x, float y, float rx, float ry) {
	moveTo(x + rx, y);
	arcTo(rx, ry, 0, false, false, x, y - ry);
	arcTo(rx, ry, 0, false, false, x - rx, y);
	arcTo(rx, ry, 0, false, false, x, y + ry);
	arcTo(rx, ry, 0, false, false, x + rx, y);
	closePath();
	return *this;
}

PathWriter &PathWriter::addArc(const Rect &oval, float startAngleInRadians,
		float sweepAngleInRadians) {
	const auto rx = oval.size.width / 2;
	const auto ry = oval.size.height / 2;

	const auto x = rx * cosf(startAngleInRadians);
	const auto y = ry * sinf(startAngleInRadians);

	const auto sx = rx * cosf(startAngleInRadians + sweepAngleInRadians);
	const auto sy = ry * sinf(startAngleInRadians + sweepAngleInRadians);

	moveTo(oval.origin.x + rx + x, oval.origin.y + ry + y);
	arcTo(rx, ry, 0.0f, (sweepAngleInRadians > numbers::pi) ? true : false, true,
			oval.origin.x + rx + sx, oval.origin.y + ry + sy);
	return *this;
}

PathWriter &PathWriter::addRect(float x, float y, float width, float height, float rx, float ry) {
	if (isnan(rx)) {
		rx = 0.0f;
	}
	if (isnan(ry)) {
		ry = 0.0f;
	}

	if (rx == 0.0f && ry == 0.0f) {
		return addRect(x, y, width, height);
	} else if (rx == 0.0f) {
		rx = ry;
	} else if (ry == 0.0f) {
		ry = rx;
	}

	rx = std::min(width / 2.0f, rx);
	ry = std::min(height / 2.0f, ry);

	moveTo(x + width - rx, y);
	arcTo(rx, ry, 0, false, true, x + width, y + ry);
	lineTo(x + width, y + height - ry);
	arcTo(rx, ry, 0, false, true, x + width - rx, y + height);
	lineTo(x + rx, y + height);
	arcTo(rx, ry, 0, false, true, x, y + height - ry);
	lineTo(x, y + ry);
	arcTo(rx, ry, 0, false, true, x + rx, y);
	closePath();
	return *this;
}

bool PathWriter::addPath(const PathData<memory::StandartInterface> &d) {
	commands.reserve(commands.size() + d.commands.size());
	for (auto &it : d.commands) { commands.emplace_back(Command(it)); }

	uvPoints.reserve(uvPoints.size() + d.uv.size());
	for (auto &it : d.uv) { uvPoints.emplace_back(Vec2(it)); }

	points.reserve(points.size() + d.points.size());
	for (auto &it : d.points) { points.emplace_back(CommandData(it)); }

	return true;
}

bool PathWriter::addPath(const PathData<memory::PoolInterface> &d) {
	commands.reserve(commands.size() + d.commands.size());
	for (auto &it : d.commands) { commands.emplace_back(Command(it)); }

	uvPoints.reserve(uvPoints.size() + d.uv.size());
	for (auto &it : d.uv) { uvPoints.emplace_back(Vec2(it)); }

	points.reserve(points.size() + d.points.size());
	for (auto &it : d.points) { points.emplace_back(CommandData(it)); }

	return true;
}

bool PathWriter::addPath(BytesView data) {
	float x1, y1, x2, y2, x3, y3, u = nan(), v = nan();
	uint32_t tmp;

	BytesViewNetwork reader(data);

	auto version = data::cbor::_readInt(reader);
	if (version != 1 && version != 2) {
		log::source().error("vg::PathWriter", "Unsupported binary encoding version: ", version);
		return false; // unsupported version
	}

	auto ncommands = data::cbor::_readInt(reader);
	auto npoints = data::cbor::_readInt(reader);
	commands.reserve(ncommands);
	uvPoints.reserve(ncommands);
	points.reserve(npoints);
	for (; ncommands != 0; --ncommands) {
		auto cmd = data::cbor::_readInt(reader);

		if (version == 2) {
			u = data::cbor::_readNumber(reader);
			v = data::cbor::_readNumber(reader);
		}

		switch (uint8_t(cmd)) {
		case toInt(Command::MoveTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			SP_PATH_LOG_TEXT("L ", x1, " ", y1);
			moveTo(x1, y1, u, v);
			break;
		case toInt(Command::LineTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			SP_PATH_LOG_TEXT("L ", x1, " ", y1);
			lineTo(x1, y1, u, v);
			break;
		case toInt(Command::QuadTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			x2 = data::cbor::_readNumber(reader);
			y2 = data::cbor::_readNumber(reader);
			SP_PATH_LOG_TEXT("Q ", x1, " ", y1, " ", x2, " ", y2);
			quadTo(x1, y1, x2, y2, u, v);
			break;
		case toInt(Command::CubicTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			x2 = data::cbor::_readNumber(reader);
			y2 = data::cbor::_readNumber(reader);
			x3 = data::cbor::_readNumber(reader);
			y3 = data::cbor::_readNumber(reader);
			SP_PATH_LOG_TEXT("C ", x1, " ", y1, " ", x2, " ", y2, " ", x3, " ", y3);
			cubicTo(x1, y1, x2, y2, x3, y3, u, v);
			break;
		case toInt(Command::ArcTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			x2 = data::cbor::_readNumber(reader);
			y2 = data::cbor::_readNumber(reader);
			x3 = data::cbor::_readNumber(reader);
			tmp = uint32_t(data::cbor::_readInt(reader));
			SP_PATH_LOG_TEXT("A ", x1, " ", y1, " ", x2, " ", y2, " ", x3, " ", tmp, " ",
					((tmp & 2) != 0), " ", ((tmp & 1) != 0));
			arcTo(x1, y1, x3, (tmp & 2) != 0, (tmp & 1) != 0, x2, y2, u, v);
			break;
		case toInt(Command::ClosePath):
			closePath();
			SP_PATH_LOG_TEXT("Z");
			break;
		default: break;
		}
	}
	return true;
}

bool PathWriter::addPath(StringView str) { return SVGPathReader::readPath(this, str); }

} // namespace stappler::vg
