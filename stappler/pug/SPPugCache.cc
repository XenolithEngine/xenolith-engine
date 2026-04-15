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

#include "SPPugCache.h"
#include "SPFilepath.h"
#include "SPMemInterface.h"
#include "SPPugContext.h"
#include "SPPugTemplate.h"
#include "SPFilesystem.h"

#include <sprt/cxx/mutex>

#if 0 && LINUX
#include <sys/inotify.h>
#endif

#ifndef SP_TERMINATED_DATA
#define SP_TERMINATED_DATA(view) (view.terminated()?view.data():view.str<memory::PoolInterface>().data())
#endif

namespace STAPPLER_VERSIONIZED stappler::pug {

Rc<FileRef> CacheFile::read(memory::pool_t *p, const FileInfo &path, Template::Options opts,
		const Callback<void(const StringView &)> &cb, int watch, int wId) {
	if (filesystem::exists(path)) {
		return Rc<FileRef>::alloc(p, path, opts, cb, watch, wId);
	}

	return nullptr;
}

Rc<FileRef> CacheFile::read(memory::pool_t *p, StringView key, String &&content, bool isTemplate,
		Template::Options opts, const Callback<void(const StringView &)> &cb) {
	return Rc<FileRef>::alloc(p, key, move(content), isTemplate, opts, cb);
}

CacheFile::CacheFile(Ref *ref, memory::pool_t *pool, const FileInfo &path, Template::Options opts,
		const Callback<void(const StringView &)> &cb, int watch, int wId)
: PoolObject(ref, pool), _opts(opts) {

	filesystem::Stat stat;
	filesystem::stat(path, stat);

	_mtime = stat.mtime;
	_content.resize(stat.size);
	filesystem::readIntoBuffer((uint8_t *)_content.data(), path);

	if (_content.size() > 0) {
		if (wId < 0 && watch >= 0) {
#if 0 && LINUX
			_watch = inotify_add_watch(watch, SP_TERMINATED_DATA(fpath), s_FileNotifyMask);
			if (_watch == -1 && errno == ENOSPC) {
				cb("inotify limit is reached: fall back to timed watcher");
			}
#endif
		} else {
			_watch = wId;
		}
		_valid = true;
	}

	auto key = filepath::canonical<Interface>(path);
	_key = StringView(key).pdup(pool);

	if (_valid
			&& (path.path.ends_with(".pug") || path.path.ends_with(".stl")
					|| path.path.ends_with(".spug"))) {
		_template = Template::read(_pool, _content, opts, cb);
		if (!_template) {
			_valid = false;
		}
	}
}

CacheFile::CacheFile(Ref *ref, memory::pool_t *pool, StringView key, String &&src, bool isTemplate,
		Template::Options opts, const Callback<void(const StringView &)> &cb)
: PoolObject(ref, pool), _content(move(src)), _opts(opts) {
	if (_content.size() > 0) {
		_valid = true;
	}

	_key = key.pdup(pool);

	if (isTemplate && _valid) {
		_template = Template::read(_pool, _content, opts, cb);
		if (!_template) {
			_valid = false;
		}
	}
}

CacheFile::~CacheFile() { }

StringView CacheFile::getContent() const { return _content; }

const Template *CacheFile::getTemplate() const { return _template; }

int CacheFile::getWatch() const { return _watch; }

Time CacheFile::getMtime() const { return _mtime; }

bool CacheFile::isValid() const { return _valid; }

const Template::Options &CacheFile::getOpts() const { return _opts; }

int CacheFile::regenerate(int notify, StringView fpath) {
	if (_watch >= 0) {
#if 0 && LINUX
		inotify_rm_watch(notify, _watch);
		_watch = inotify_add_watch(notify, SP_TERMINATED_DATA(fpath), s_FileNotifyMask);
		return _watch;
#endif
	}
	return 0;
}

StringView CacheFile::getKey() const { return _key; }

Cache::Cache(Template::Options opts, const Function<void(const StringView &)> &err)
: _pool(memory::pool::acquire()), _opts(opts), _errorCallback(err) {
#if 0 && LINUX
	_inotify = inotify_init1(IN_NONBLOCK);
#endif
	if (_inotify != -1) {
		_inotifyAvailable = true;
	}
}

Cache::~Cache() {
	if (_inotify > 0) {
#if 0 && LINUX
		for (auto &it : _templates) {
			auto fd = it.second->getWatch();
			if (fd >= 0) {
				inotify_rm_watch(_inotify, fd);
			}
		}
		close(_inotify);
#endif
	}
}

void Cache::update(int watch, bool regenerate) {
	sprt::unique_lock<Mutex> lock(_mutex);
	auto it = _watches.find(watch);
	if (it != _watches.end()) {
		auto tIt = _templates.find(it->second);
		if (tIt != _templates.end()) {
			if (regenerate) {
				_watches.erase(it);
				if (auto tpl = openTemplate(FileInfo{it->second}, -1, tIt->second->getOpts())) {
					tIt->second = tpl;
					watch = tIt->second->getWatch();
					if (watch < 0) {
						_inotifyAvailable = false;
					} else {
						_watches.emplace(watch, tIt->first);
					}
				}
			} else {
				if (auto tpl = openTemplate(FileInfo{it->second}, tIt->second->getWatch(),
							tIt->second->getOpts())) {
					tIt->second = tpl;
				}
			}
		}
	}
}

void Cache::update(memory::pool_t *pool, bool force) {
	memory::context ctx(pool);
	for (auto &it : _templates) {
		if (it.second->getMtime() != Time()) {
			filesystem::Stat stat;
			filesystem::stat(FileInfo{it.first}, stat);
			if (stat.mtime != it.second->getMtime() || force) {
				if (auto tpl = openTemplate(FileInfo{it.first}, -1, it.second->getOpts())) {
					it.second = tpl;
				}
			}
		}
	}
}

int Cache::getNotify() const { return _inotify; }

bool Cache::isNotifyAvailable() {
	sprt::unique_lock<Mutex> lock(_mutex);
	return _inotifyAvailable;
}

void Cache::regenerate(StringView key) {
	if (_inotifyAvailable) {
		auto it = _templates.find(key);
		if (it != _templates.end()) {
			_watches.erase(it->second->getWatch());
			auto watch = it->second->regenerate(_inotify, key);
			_watches.emplace(watch, it->first);
		}
	}
}

void Cache::regenerate(const FileInfo &path) {
	auto key = filepath::canonical<memory::StandartInterface>(path);
	regenerate(key);
}

void Cache::drop(StringView key) {
	auto it = _templates.find(key);
	if (it != _templates.end()) {
		_templates.erase(it);
	}
}

void Cache::drop(const FileInfo &path) {
	auto key = filepath::canonical<memory::StandartInterface>(path);
	drop(key);
}

bool Cache::runTemplate(const FileInfo &ipath, const RunCallback &cb, const OutStream &out) {
	Rc<FileRef> tpl = acquireTemplate(ipath, true, _opts);
	if (!tpl) {
		tpl = acquireTemplate(ipath, false, _opts);
	}

	return runTemplate(tpl, cb, out, tpl->getTemplate()->getOptions());
}

bool Cache::runTemplate(const FileInfo &ipath, const RunCallback &cb, const OutStream &out,
		Template::Options opts) {
	Rc<FileRef> tpl = acquireTemplate(ipath, true, opts);
	if (!tpl) {
		tpl = acquireTemplate(ipath, false, opts);
	}

	return runTemplate(tpl, cb, out, opts);
}

bool Cache::runTemplate(StringView key, const RunCallback &cb, const OutStream &out) {
	Rc<FileRef> tpl = get(key);
	if (tpl) {
		return runTemplate(tpl, cb, out, tpl->getTemplate()->getOptions());
	}

	onError(mem_pool::toString("No template '", key, "' found"));
	return false;
}

bool Cache::runTemplate(StringView key, const RunCallback &cb, const OutStream &out,
		Template::Options opts) {
	Rc<FileRef> tpl = get(key);
	if (tpl) {
		return runTemplate(tpl, cb, out, opts);
	}

	onError(mem_pool::toString("No template '", key, "' found"));
	return false;
}

bool Cache::addFile(const FileInfo &path) {
	auto key = filepath::canonical<memory::StandartInterface>(path);

	sprt::unique_lock<Mutex> lock(_mutex);
	auto it = _templates.find(key);
	if (it == _templates.end()) {
		memory::context ctx(_pool);
		if (auto tpl = openTemplate(path, -1, _opts)) {
			auto it = _templates.emplace(tpl->getKey(), tpl).first;
			if (tpl->getWatch() >= 0) {
				_watches.emplace(tpl->getWatch(), it->first);
			}
			return true;
		}
	} else {
		onError(mem_pool::toString("Already added: '", path, "'"));
	}
	return false;
}

bool Cache::addContent(StringView key, String &&data) {
	sprt::unique_lock<Mutex> lock(_mutex);
	auto it = _templates.find(key);
	if (it == _templates.end()) {
		auto tpl = CacheFile::read(_pool, key, move(data), false, _opts);
		_templates.emplace(tpl->getKey(), tpl);
		return true;
	} else {
		onError(mem_pool::toString("Already added: '", key, "'"));
	}
	return false;
}

bool Cache::addTemplate(StringView key, String &&data) {
	return addTemplate(key, move(data), _opts);
}

bool Cache::addTemplate(StringView key, String &&data, Template::Options opts) {
	sprt::unique_lock<Mutex> lock(_mutex);
	auto it = _templates.find(key);
	if (it == _templates.end()) {
		auto tpl = CacheFile::read(_pool, key, move(data), true, opts,
				[&](const StringView &err) SP_COVERAGE_TRIVIAL {
			sprt::cout << key << ":\n";
			sprt::cout << err << "\n";
		});
		_templates.emplace(tpl->getKey(), tpl);
		return true;
	} else {
		onError(mem_pool::toString("Already added: '", key, "'"));
	}
	return false;
}

Rc<FileRef> Cache::get(StringView key) const {
	sprt::unique_lock<Mutex> lock(_mutex);
	auto it = _templates.find(key);
	if (it != _templates.end()) {
		return it->second;
	}
	return nullptr;
}

Rc<FileRef> Cache::get(const FileInfo &path) const {
	auto key = filepath::canonical<memory::StandartInterface>(path);
	return get(key);
}

Rc<FileRef> Cache::acquireTemplate(const FileInfo &path, bool readOnly,
		const Template::Options &opts) {
	auto key = filepath::canonical<memory::StandartInterface>(path);

	sprt::unique_lock<Mutex> lock(_mutex);
	auto it = _templates.find(key);
	if (it != _templates.end()) {
		return it->second;
	} else if (!readOnly) {
		if (auto tpl = openTemplate(path, -1, opts)) {
			auto it = _templates.emplace(tpl->getKey(), tpl).first;
			if (tpl->getWatch() >= 0) {
				_watches.emplace(tpl->getWatch(), it->first);
			}
			return tpl;
		}
	}
	return nullptr;
}

Rc<FileRef> Cache::openTemplate(const FileInfo &path, int wId, const Template::Options &opts) {
	auto ret = CacheFile::read(_pool, path, opts, [&](const StringView &err) SP_COVERAGE_TRIVIAL {
		sprt::cout << path << ":\n";
		sprt::cout << err << "\n";
	}, _inotify, wId);
	if (!ret) {
		onError(mem_pool::toString("File not found: ", path));
	} else if (ret->isValid()) {
		return ret;
	}
	return nullptr;
}

bool Cache::runTemplate(Rc<FileRef> tpl, const RunCallback &cb, const OutStream &out,
		Template::Options opts) {
	if (tpl) {
		if (auto t = tpl->getTemplate()) {
			auto iopts = tpl->getOpts();
			Context exec;
			exec.loadDefaults();
			exec.setIncludeCallback(
					[this, iopts](const StringView &path, Context &exec, const OutStream &out,
							Template::RunContext &rctx) -> bool {
				Rc<FileRef> tpl = acquireTemplate(FileInfo(path), true, iopts);
				if (!tpl) {
					tpl = acquireTemplate(FileInfo(path), false, iopts);
				}

				if (!tpl) {
					return false;
				}

				bool ret = false;
				if (const Template *t = tpl->getTemplate()) {
					ret = t->run(exec, out, rctx);
				} else {
					out << tpl->getContent();
					ret = true;
				}

				return ret;
			});
			if (cb != nullptr) {
				if (!cb(exec, *t)) {
					return false;
				}
			}
			return t->run(exec, out, opts);
		} else {
			onError(mem_pool::toString("File '", tpl->getKey(), "' is not executable"));
		}
	} else {
		onError("No template found");
	}
	return false;
}

void Cache::onError(const StringView &str) {
	if (str == "inotify limit is reached: fall back to timed watcher") {
		sprt::unique_lock<Mutex> lock(_mutex);
		_inotifyAvailable = false;
	}
	if (_errorCallback != nullptr) {
		_errorCallback(str);
	} else {
		sprt::cout << str;
	}
}

} // namespace stappler::pug
