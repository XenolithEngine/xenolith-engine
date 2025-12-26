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

#include "SPWasm.h"
#include "SPFilesystem.h"
#include "SPPlatformUnistd.h"

namespace stappler::wasm {

// Temporary disabled
#if 0

static uint32_t StapplerFilesystemExists(wasm_exec_env_t exec_env, char *ptr, uint32_t size) {
	return filesystem::exists(StringView(ptr, size)) ? 1 : 0;
}

struct StatResult {
	uint64_t success;
	uint64_t size;
	uint64_t atime;
	uint64_t ctime;
	uint64_t mtime;
	uint64_t type;
};

static void StapplerFilesystemStat(wasm_exec_env_t exec_env, char *ptr, uint32_t size, StatResult *res) {
	filesystem::Stat stat;
	if (filesystem::stat(StringView(ptr, size), stat)) {
		res->success = false;
		res->size = stat.size;
		res->atime = stat.atime.toMicros();
		res->ctime = stat.ctime.toMicros();
		res->mtime = stat.mtime.toMicros();
		res->type = toInt(stat.type);
	} else {
		res->success = false;
	}
}

static void StapplerFilesystemWritablePath(wasm_exec_env_t exec_env, char *ptr, uint32_t size, bool rel, bool readOnly, ListOutput *target) {
	auto env = ExecEnv::get(exec_env);

	String result;
	if (readOnly) {
		result = filesystem::writablePathReadOnly<Interface>(StringView(ptr, size), rel);
	} else {
		result = filesystem::writablePath<Interface>(StringView(ptr, size), rel);
	}

	char *outStringBuffer = nullptr;
	auto outOffset = env->allocate(uint32_t(result.size() * sizeof(char)), &outStringBuffer);

	memcpy(outStringBuffer, result.data(), result.size() * sizeof(char));

	target->ptr = outOffset;
	target->len = uint32_t(result.size());
}

static void StapplerFilesystemDocumentsPath(wasm_exec_env_t exec_env, char *ptr, uint32_t size, bool rel, bool readOnly, ListOutput *target) {
	auto env = ExecEnv::get(exec_env);

	String result;
	if (readOnly) {
		result = filesystem::documentsPathReadOnly<Interface>(StringView(ptr, size), rel);
	} else {
		result = filesystem::documentsPath<Interface>(StringView(ptr, size), rel);
	}

	char *outStringBuffer = nullptr;
	auto outOffset = env->allocate(uint32_t(result.size() * sizeof(char)), &outStringBuffer);

	memcpy(outStringBuffer, result.data(), result.size() * sizeof(char));

	target->ptr = outOffset;
	target->len = uint32_t(result.size());
}

static void StapplerFilesystemCachesPath(wasm_exec_env_t exec_env, char *ptr, uint32_t size, bool rel, bool readOnly, ListOutput *target) {
	auto env = ExecEnv::get(exec_env);

	String result;
	if (readOnly) {
		result = filesystem::cachesPathReadOnly<Interface>(StringView(ptr, size), rel);
	} else {
		result = filesystem::cachesPath<Interface>(StringView(ptr, size), rel);
	}

	char *outStringBuffer = nullptr;
	auto outOffset = env->allocate(uint32_t(result.size() * sizeof(char)), &outStringBuffer);

	memcpy(outStringBuffer, result.data(), result.size() * sizeof(char));

	target->ptr = outOffset;
	target->len = uint32_t(result.size());
}

static void StapplerFilesystemCurrentDir(wasm_exec_env_t exec_env, char *ptr, uint32_t size, bool rel, ListOutput *target) {
	auto env = ExecEnv::get(exec_env);

	String result = filesystem::currentDir<Interface>(StringView(ptr, size), rel);

	char *outStringBuffer = nullptr;
	auto outOffset = env->allocate(uint32_t(result.size() * sizeof(char)), &outStringBuffer);

	memcpy(outStringBuffer, result.data(), result.size() * sizeof(char));

	target->ptr = outOffset;
	target->len = uint32_t(result.size());
}

static void StapplerFilesystemFtw(wasm_exec_env_t exec_env, char *ptr, uint32_t size, uint32_t fn, uint32_t arg, int32_t depth, bool dirFirst) {
	auto env = ExecEnv::get(exec_env);
	char *buf = nullptr;

	auto bufOffset = env->allocate(PATH_MAX, &buf);

	uint32_t args[4];
	args[0] = arg;
	args[1] = bufOffset;

	filesystem::ftw(StringView(ptr, size), [&] (StringView path, bool isFile) {
		buf = env->appToNative<char>(bufOffset);
		memcpy(buf, path.data(), path.size());

		args[2] = uint32_t(path.size());
		args[3] = uint32_t(isFile);

		wasm_runtime_call_indirect(exec_env, fn, 4, args);
	}, depth, dirFirst);

	env->free(bufOffset);
}

static uint32_t StapplerFilesystemFtwB(wasm_exec_env_t exec_env, char *ptr, uint32_t size, uint32_t fn, uint32_t arg, int32_t depth, bool dirFirst) {
	auto env = ExecEnv::get(exec_env);
	char *buf = nullptr;

	auto bufOffset = env->allocate(PATH_MAX, &buf);

	uint32_t args[4];

	auto ret = filesystem::ftw_b(StringView(ptr, size), [&] (StringView path, bool isFile) {
		buf = env->appToNative<char>(bufOffset);
		memcpy(buf, path.data(), path.size());

		args[0] = arg;
		args[1] = bufOffset;
		args[2] = uint32_t(path.size());
		args[3] = uint32_t(isFile);

		if (wasm_runtime_call_indirect(exec_env, fn, 4, args)) {
			return bool(args[0]);
		}
		return false;
	}, depth, dirFirst);

	env->free(bufOffset);
	return ret;
}

static uint32_t StapplerFilesystemOpen(wasm_exec_env_t exec_env, char *ptr, uint32_t size) {
	auto f = filesystem::openForReading(StringView(ptr, size));
	if (f) {
		auto env = ExecEnv::get(exec_env);
		auto mod = env->getInstance();
		auto obj = new filesystem::File(move(f));

		return mod->addHandle(obj, [obj] {
			delete obj;
		});
	}
	return ModuleInstance::InvalidHandle;
}

static uint32_t StapplerFilesystemOpenTmp(wasm_exec_env_t exec_env, char *ptr, uint32_t size, bool delOnClose) {
	auto f = filesystem::File::open_tmp(StringView(ptr, size), delOnClose);
	if (f) {
		auto env = ExecEnv::get(exec_env);
		auto mod = env->getInstance();
		auto obj = new filesystem::File(move(f));

		return mod->addHandle(obj, [obj] {
			delete obj;
		});
	}
	return ModuleInstance::InvalidHandle;
}

static void StapplerFilesystemFileDrop(wasm_exec_env_t exec_env, uint32_t handle) {
	auto mod = ExecEnv::get(exec_env)->getInstance();
	auto file = mod->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[resource-drop]file: invalid handle");
		return;
	}

	mod->removeHandle(handle);
}

static uint32_t StapplerFilesystemFileRead(wasm_exec_env_t exec_env, uint32_t handle, uint8_t *buf, uint32_t bufSize) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.read: invalid handle");
		return 0;
	}

	return uint32_t(file->read(buf, bufSize));
}

static uint64_t StapplerFilesystemFileSeek(wasm_exec_env_t exec_env, uint32_t handle, int64_t offset, io::Seek pos) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.seek: invalid handle");
		return 0;
	}

	return file->seek(offset, pos);
}

static uint64_t StapplerFilesystemFileTell(wasm_exec_env_t exec_env, uint32_t handle) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.tell: invalid handle");
		return 0;
	}

	return file->tell();
}

static uint64_t StapplerFilesystemFileSize(wasm_exec_env_t exec_env, uint32_t handle) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.size: invalid handle");
		return 0;
	}

	return file->size();
}

static int32_t StapplerFilesystemFileXsgetc(wasm_exec_env_t exec_env, uint32_t handle) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.xsgetc: invalid handle");
		return 0;
	}

	return file->xsgetc();
}

static int32_t StapplerFilesystemFileXsputc(wasm_exec_env_t exec_env, uint32_t handle, int32_t c) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.xsputc: invalid handle");
		return 0;
	}

	return file->xsputc(c);
}

static uint32_t StapplerFilesystemFileXsputn(wasm_exec_env_t exec_env, uint32_t handle, uint8_t *buf, uint32_t bufSize) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.xsputn: invalid handle");
		return 0;
	}

	return uint32_t(file->xsputn((const char *)buf, bufSize));
}

static uint32_t StapplerFilesystemFileXsgetn(wasm_exec_env_t exec_env, uint32_t handle, uint8_t *buf, uint32_t bufSize) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.xsgetn: invalid handle");
		return 0;
	}

	return uint32_t(file->xsgetn((char *)buf, bufSize));
}

static uint32_t StapplerFilesystemFileIsOpen(wasm_exec_env_t exec_env, uint32_t handle) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.is-open: invalid handle");
		return 0;
	}

	return file->is_open();
}

static uint32_t StapplerFilesystemFileEof(wasm_exec_env_t exec_env, uint32_t handle) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.eof: invalid handle");
		return 0;
	}

	return file->eof();
}

static void StapplerFilesystemFileClose(wasm_exec_env_t exec_env, uint32_t handle) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.close: invalid handle");
		return;
	}

	file->close();
}

static void StapplerFilesystemFileCloseRemove(wasm_exec_env_t exec_env, uint32_t handle) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.close-remove: invalid handle");
		return;
	}

	file->close_remove();
}

static void StapplerFilesystemFileCloseRename(wasm_exec_env_t exec_env, uint32_t handle, char *name, uint32_t len) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.close-rename: invalid handle");
		return;
	}

	file->close_rename(StringView(name, len));
}

static uint32_t StapplerFilesystemFileGetTmpPath(wasm_exec_env_t exec_env, uint32_t handle, char *buf, uint32_t len) {
	auto file = ExecEnv::get(exec_env)->getInstance()->getObject<filesystem::File>(handle);
	if (!file) {
		log::source().error("wasm::Runtime", "[method]file.get-tmp-path: invalid handle");
		return 0;
	}

	auto path = file->path();
	len = max(len, uint32_t(strlen(path)));
	memcpy(buf, path, len);
	return len;
}

static NativeSymbol stapper_filesystem_symbols[] = {
	NativeSymbol{"exists", (void *)&StapplerFilesystemExists, "(*~)i", NULL},
	NativeSymbol{"stat", (void *)&StapplerFilesystemStat, "(*~*)", NULL},
	NativeSymbol{"get-writable-path", (void *)&StapplerFilesystemWritablePath, "(*~ii*)", NULL},
	NativeSymbol{"get-documents-path", (void *)&StapplerFilesystemDocumentsPath, "(*~ii*)", NULL},
	NativeSymbol{"get-caches-path", (void *)&StapplerFilesystemCachesPath, "(*~ii*)", NULL},
	NativeSymbol{"get-current-work-dir", (void *)&StapplerFilesystemCurrentDir, "(*~i*)", NULL},
	NativeSymbol{"ftw", (void *)&StapplerFilesystemFtw, "(*~iiii)", NULL},
	NativeSymbol{"ftw-b", (void *)&StapplerFilesystemFtwB, "(*~iiii)i", NULL},
	NativeSymbol{"open", (void *)&StapplerFilesystemOpen, "(*~)i", NULL},
	NativeSymbol{"open-tmp", (void *)&StapplerFilesystemOpenTmp, "(*~i)i", NULL},

	NativeSymbol{"[method]file.read", (void *)&StapplerFilesystemFileRead, "(i*~)i", NULL},
	NativeSymbol{"[method]file.seek", (void *)&StapplerFilesystemFileSeek, "(iIi)I", NULL},
	NativeSymbol{"[method]file.tell", (void *)&StapplerFilesystemFileTell, "(i)I", NULL},
	NativeSymbol{"[method]file.size", (void *)&StapplerFilesystemFileSize, "(i)I", NULL},
	NativeSymbol{"[method]file.xsgetc", (void *)&StapplerFilesystemFileXsgetc, "(i)i", NULL},
	NativeSymbol{"[method]file.xsputc", (void *)&StapplerFilesystemFileXsputc, "(ii)i", NULL},
	NativeSymbol{"[method]file.xsputn", (void *)&StapplerFilesystemFileXsputn, "(i*~)i", NULL},
	NativeSymbol{"[method]file.xsgetn", (void *)&StapplerFilesystemFileXsgetn, "(i*~)i", NULL},
	NativeSymbol{"[method]file.is-open", (void *)&StapplerFilesystemFileIsOpen, "(i)i", NULL},
	NativeSymbol{"[method]file.eof", (void *)&StapplerFilesystemFileEof, "(i)i", NULL},
	NativeSymbol{"[method]file.close", (void *)&StapplerFilesystemFileClose, "(i)", NULL},
	NativeSymbol{"[method]file.close-remove", (void *)&StapplerFilesystemFileCloseRemove, "(i)", NULL},
	NativeSymbol{"[method]file.close-rename", (void *)&StapplerFilesystemFileCloseRename, "(i*~)", NULL},
	NativeSymbol{"[method]file.get-tmp-path", (void *)&StapplerFilesystemFileGetTmpPath, "(i*~)i", NULL},

	NativeSymbol{"[resource-drop]file", (void *)&StapplerFilesystemFileDrop, "(i)", NULL},

};

static NativeModule s_filesystemModule("stappler:wasm/filesystem", stapper_filesystem_symbols, sizeof(stapper_filesystem_symbols) / sizeof(NativeSymbol));

#endif

} // namespace stappler::wasm
