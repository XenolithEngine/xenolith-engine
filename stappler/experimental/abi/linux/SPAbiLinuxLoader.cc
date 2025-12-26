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

#include "SPAbiLinuxElf.h"
#include "SPAbiLinux.h"
#include "SPPlatform.h"
#include "SPAbiLinuxLoader.h"

#include <fcntl.h>
#include <sys/mman.h>

#include <setjmp.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <bits/fdhelper.h>

// Linux loader based on [this](https://github.com/pfalcon/foreign-dlopen) concept

#define AMD_REXB    0x41
#define AMD_REXW    0x48
#define AMD_MOV_IMM 0xb8

#define ARM_REG_OFF 0
#define ARM_IMM_OFF 5
#define ARM_IDX_OFF 21
#define ARM_MOV_NEX 0xf280'0000u

#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_NOLOAD 4
#define RTLD_GLOBAL 256
#define RTLD_LOCAL  0

namespace STAPPLER_VERSIONIZED stappler::abi {

static constexpr bool PRECOMPILE_DSO_FN = false;

static constexpr int ARCH_SET_GS = 0x1001;
static constexpr int ARCH_SET_FS = 0x1002;
static constexpr int ARCH_GET_FS = 0x1003;
static constexpr int ARCH_GET_GS = 0x1004;

static constexpr unsigned long HWCAP2_RING3MWAIT = 1 << 0;
static constexpr unsigned long HWCAP2_FSGSBASE = 1 << 1;

struct ElfTypeWrapper {
	using Elf_long = unsigned long;
#if __x86_64__
	using Elf_Ehdr = Elf64_Ehdr;
	using Elf_Phdr = Elf64_Phdr;
	using Elf_auxv_t = Elf64_auxv_t;

	static constexpr ElfClass LoaderClass = ELFCLASS64;
	static constexpr ElfMachine LoaderMachine = ElfMachine::x86_64;
#elif __aarch64__
	using Elf_Ehdr = Elf64_Ehdr;
	using Elf_Phdr = Elf64_Phdr;
	using Elf_auxv_t = Elf64_auxv_t;

	static constexpr ElfClass LoaderClass = ELFCLASS64;
	static constexpr ElfMachine LoaderMachine = ElfMachine::AArch64;
#elif __i386__
	using Elf_Ehdr = Elf32_Ehdr;
	using Elf_Phdr = Elf32_Phdr;
	using Elf_auxv_t = Elf32_auxv_t;

	static constexpr ElfClass LoaderClass = ELFCLASS32;
	static constexpr ElfMachine LoaderMachine = ElfMachine::x86;
#elif __arm__
	using Elf_Ehdr = Elf32_Ehdr;
	using Elf_Phdr = Elf32_Phdr;
	using Elf_auxv_t = Elf32_auxv_t;

	static constexpr ElfClass LoaderClass = ELFCLASS32;
	static constexpr ElfMachine LoaderMachine = ElfMachine::Arm;
#else
#error "Unsupported architecture for Linux Loader, recompile without DSO in static toolchain"
#endif
};

// Info about exec params, passed on stack
// see: https://articles.manugarg.com/aboutelfauxiliaryvectors
struct ElfStackInfo : ElfTypeWrapper {
	// read values from predefined stack pointer
	// Stack pointer can be acquired as argv - 1 from 'main'
	static ElfStackInfo getForSp(unsigned long *);

	unsigned long *start = nullptr;
	unsigned long *end = nullptr;
	unsigned long *envp = nullptr;
	Elf_auxv_t *auxp = nullptr;

	uint32_t argc = 0;
	uint32_t envc = 0;
	uint32_t auxc = 0;
	uint32_t total = 0;

	Elf_long getAuxValue(unsigned long) const;
};

struct RunParams {
	unsigned long *origSp = nullptr;
	int argc = 0;
	const char **argv = nullptr;
	uintptr_t progHeader = 0;
	uintptr_t progHeaderElts = 0;
	uintptr_t progHeaderEltSize = 0;
	uintptr_t progBase = 0;
	uintptr_t progEntry = 0;
	uintptr_t interpBase = 0;
	uintptr_t interpEntry = 0;
	const char *path = nullptr;
};

enum class LoaderFlags : uint32_t {
	None,
	HasGsFsCaps = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(LoaderFlags)

struct LinuxLoader;

// use most-safe alignment possible
struct alignas(32) LoaderHeader {
	unsigned long originalTcb = 0;
	unsigned long foreignTcb = 0;
	unsigned long returnValueCache = 0;
	LinuxLoader *root = nullptr;
};

struct LinuxLoader : ElfTypeWrapper {
	LoaderHeader _header;

	ElfStackInfo _initStack;
	LoaderFlags _flags = LoaderFlags::None;

	SharedRc<ElfFile> _helperBinary;
	SharedRc<ElfFile> _platformInterp;

	jmp_buf _jmpbuf;

	DsoLoader dso;

	void *z_libc = nullptr;

	int (*z_pthread_create)(pthread_t *, const pthread_attr_t *, typeof(void *(void *)) *,
			void *) = nullptr;
	int (*z_pthread_join)(pthread_t, void **) = nullptr;
	int (*z_pthread_detach)(pthread_t) = nullptr;

	JitCompiler jitCompiler;

	bool load(int originalArgc, const char *originalArgv[]);
	void exec(int argc, const char *argv[]);

	bool setup();
} s_loader;

enum class _ThreadFlags : uint32_t {
	None = 0,
	Joinable = 1 << 0,
};

SP_DEFINE_ENUM_AS_MASK(_ThreadFlags)

static void _getTcbAddr(unsigned long *target) {
#if __x86_64__
	::syscall(SYS_arch_prctl, ARCH_GET_FS, target);
#else
#error "Implement me!"
#endif
}

static void _setTcbAddr(unsigned long target) {
#if __x86_64__
	::syscall(SYS_arch_prctl, ARCH_SET_FS, target);
#else
#error "Implement me!"
#endif
}

static void _setLoaderAddr(unsigned long target) {
#if __x86_64__
	::syscall(SYS_arch_prctl, ARCH_SET_GS, target);
#else
#error "Implement me!"
#endif
}

static void do_jump(void **p) {
	// we now in foreign context

	s_loader.dso.z_dlopen = reinterpret_cast<decltype(s_loader.dso.z_dlopen)>(p[0]);
	s_loader.dso.z_dlsym = reinterpret_cast<decltype(s_loader.dso.z_dlsym)>(p[1]);
	s_loader.dso.z_dlclose = reinterpret_cast<decltype(s_loader.dso.z_dlclose)>(p[2]);
	s_loader.dso.z_dlerror = reinterpret_cast<decltype(s_loader.dso.z_dlerror)>(p[3]);

	unsigned long fs = 0;
	::syscall(SYS_arch_prctl, ARCH_GET_FS, &fs);

	s_loader.z_libc = s_loader.dso.z_dlopen("libc.so.6", RTLD_NOW);

	s_loader.z_pthread_create = reinterpret_cast<decltype(s_loader.z_pthread_create)>(
			s_loader.dso.z_dlsym(s_loader.z_libc, "pthread_create"));

	s_loader.z_pthread_join = reinterpret_cast<decltype(s_loader.z_pthread_join)>(
			s_loader.dso.z_dlsym(s_loader.z_libc, "pthread_join"));

	s_loader.z_pthread_detach = reinterpret_cast<decltype(s_loader.z_pthread_detach)>(
			s_loader.dso.z_dlsym(s_loader.z_libc, "pthread_detach"));

	auto h = s_loader.dso.z_dlopen("libxcb.so", RTLD_LAZY);

	// read TCB from current interp
	_getTcbAddr(&s_loader._header.foreignTcb);

	// switch to our original context before return
	_setTcbAddr(s_loader._header.originalTcb);

	longjmp(s_loader._jmpbuf, 1);
}

static void sprintn(char *buf, unsigned long ul, int base) {
	/* hold a long in base 8 */
	char *p;

	p = buf;
	do { *p++ = "0123456789abcdef"[ul % base]; } while (ul /= base);
	*p-- = 0;

	/* output is reversed, swap it now */
	while (p > buf) {
		char c = *p;
		*p = *buf;
		*buf = c;
		buf++;
		p--;
	}
}

static void _fini(void) { printf("Fini at work\n"); }

// clang-format off
__attribute__((naked, noreturn)) static void _execTrampo(void (*entry)(void), unsigned long *sp, void (*fini)(void)) {
    __asm__(
		"mov %rsi, %rsp\n"
		"jmp *%rdi\n"
		"hlt\n"
    );
}
// clang-format on

__attribute__((noreturn)) static void _run(ElfStackInfo info, const RunParams &params) {
	unsigned long *newSp = nullptr, *p = nullptr;
	Elf64_auxv_t *av = nullptr;

	// correct argc/argv counter
	info.total -= info.argc; // remove initial argv
	info.argc = params.argc;
	info.total += params.argc; // add new argc

	p = newSp = (unsigned long *)::alloca(sizeof(unsigned long) * info.total);

	// set up argc/argv
	*p++ = params.argc;
	::memcpy(p, params.argv, params.argc * sizeof(unsigned long));
	p += params.argc;
	*p++ = 0; // do not forget zero padding

	// copy old env and aux
	::memcpy(p, info.envp, sizeof(unsigned long) * (info.end - info.envp));

	// skip env block
	p += info.envc + 1;

	//auto nwords = 1 + params.argc + 1 + envc + 1 + auxc * 2 + 1 + 1;
	av = (Elf64_auxv_t *)(p);

	// Reassign some vectors that are important for
	// the dynamic linker and for lib C.
	while (av->a_type != AT_NULL) {
		switch (av->a_type) {
		case AT_PHDR: av->a_un.a_val = params.progHeader; break;
		case AT_PHNUM: av->a_un.a_val = params.progHeaderElts; break;
		case AT_PHENT: av->a_un.a_val = params.progHeaderEltSize; break;
		case AT_ENTRY: av->a_un.a_val = params.progEntry; break;
		case AT_EXECFN: av->a_un.a_val = uintptr_t(params.path); break;
		case AT_BASE: av->a_un.a_val = params.interpBase; break;
		default: break;
		}
		++av;
	}

	_execTrampo((void (*)(void))(params.interpEntry), newSp, _fini);
	::exit(0);
}

bool LinuxLoader::load(int originalArgc, const char *originalArgv[]) {
	// Get info about stack we running on
	s_loader._initStack = ElfStackInfo::getForSp((unsigned long *)originalArgv - 1);

	// Process original aux vectors for hardware capabilities
	auto auxIter = s_loader._initStack.auxp;
	while (auxIter->a_type != 0) {
		switch (auxIter->a_type) {
		case AT_HWCAP2:
			if (hasFlag(auxIter->a_un.a_val, HWCAP2_FSGSBASE)) {
				// we have  rdfsbase/rdgsbase/wrfsbase/wrgsbase asm ops without syscalls
				s_loader._flags |= LoaderFlags::HasGsFsCaps;
			}
			break;
		default: break;
		}
		++auxIter;
	}

	// TODO: for now, we have only precompiled binary for glibc-based systems
	// We need to detect which interp is default on system (to detect musl-based or some), then provide helper for it
	//
	// For extreme comapibility, we need to construct helper in place with JIT compilation and package it into ELF

	// load precompiled helper binary and check if we can run it
	auto helperFile = ElfFile::create(BytesView(__fdhelper_data, sizeof(__fdhelper_data)), false);
	if (helperFile->getClass() != LinuxLoader::LoaderClass
			|| helperFile->getMachine() != LinuxLoader::LoaderMachine) {
		slog().error("abi::LinuxLoader",
				"Invalid machine or class in Elf package: unable to load it");
		return false;
	}

	auto selfType = helperFile->getType();
	if (selfType != ET_DYN && selfType != ET_EXEC) {
		slog().error("abi::LinuxLoader", "Elf package with helper is not runnable");
		return false;
	}

	// Load file interpreter and check if we can run it
	auto interpElf = ElfFile::create(FileInfo{helperFile->getInterp()});
	if (!interpElf) {
		slog().error("abi::LinuxLoader", "Fail to find interp: ", helperFile->getInterp());
		return false;
	}

	if (interpElf->getClass() != LoaderClass || interpElf->getMachine() != LoaderMachine) {
		slog().error("abi::LinuxLoader",
				"Invalid machine or class in Elf interpreter: unable to load it");
		return false;
	}

	auto interpType = interpElf->getType();
	if (interpType != ET_DYN && interpType != ET_EXEC) {
		slog().error("abi::LinuxLoader", "Elf interpreter is not runnable");
		return false;
	}

	// Map helper and loader into virtual memory
	// Helper and loader should be PIE
	auto fileMapping = helperFile->map();
	if (!fileMapping) {
		slog().error("abi::LinuxLoader", "Fail to map helper binary");
		return false;
	}

	auto interpMapping = interpElf->map();
	if (!interpMapping) {
		slog().error("abi::LinuxLoader", "Fail to map elf interpreter");
		return false;
	}

	// now we ready to start, save opened files with loader
	_helperBinary = sp::move(helperFile);
	_platformInterp = sp::move(interpElf);

	// acquire default TCB from our current libc
	_getTcbAddr(&s_loader._header.originalTcb);

	_header.root = this;

	return true;
}

void LinuxLoader::exec(int argc, const char *argv[]) {
	auto helperBase = _helperBinary->getBaseAddress();
	auto interpBase = _platformInterp->getBaseAddress();
	auto interpEntry = _platformInterp->getEntryPoint();

	if (auto m = _platformInterp->getMapping(interpBase + interpEntry)) {
		if (hasFlag(m.flags, PROT_EXEC)) {
			RunParams params{
				_initStack.start,
				argc,
				argv,
				uintptr_t(helperBase) + _helperBinary->getProgramHeaderOffset(),
				_helperBinary->getProgramHeaderEntryCount(),
				_helperBinary->getProgramHeaderEntrySize(),
				uintptr_t(helperBase),
				uintptr_t(helperBase) + _helperBinary->getEntryPoint(),
				uintptr_t(interpBase),
				uintptr_t(interpBase + interpEntry),
				argv[0],
			};

			_run(_initStack, params);
		}
	}
}

bool LinuxLoader::setup() {
	// set GS to loader header
	_setLoaderAddr(reinterpret_cast<uintptr_t>(&s_loader._header));

	if constexpr (PRECOMPILE_DSO_FN) {
		dso.z_dlopen = jitCompiler.compileForeignCall(dso.z_dlopen);
		dso.z_dlsym = jitCompiler.compileForeignCall(dso.z_dlsym);
		dso.z_dlclose = jitCompiler.compileForeignCall(dso.z_dlclose);
		dso.z_dlerror = jitCompiler.compileForeignCall(dso.z_dlerror);
	}

	z_pthread_create = jitCompiler.compileForeignCall(z_pthread_create);
	z_pthread_join = jitCompiler.compileForeignCall(z_pthread_join);
	z_pthread_detach = jitCompiler.compileForeignCall(z_pthread_detach);

	/*startForeignThread([](void *) -> void * {
		printf("Other thread %ld\n", pthread_self());
		return nullptr;
	}, nullptr, 0);*/

	return true;
}

// clang-format off
// mimic function call ABI as if we call original function
// set RAX to target function address before call
__attribute__((naked)) void _foreignTrampo() {
	asm volatile (
		"popq %%gs:16\n" // preserve return address
		"movq %%gs:8, %%r11\n" // read foreign TCB
		"wrfsbaseq %%r11\n" // set current TCB to foreign
		"call *%%rax\n" // call our function
		"movq %%gs:0, %%r11\n" // read original TCB
		"wrfsbaseq %%r11\n" // set original TCB
		"pushq %%gs:16\n" // restore return address
		"ret\n" // return to saved return address
		:
		:
		// inform compiler about all possible changes with clobber list
		// see https://stackoverflow.com/questions/37502841/calling-printf-in-extended-inline-asm/37503773#37503773
		: "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11",
          "xmm0","xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8","xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
          "mm0","mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm6",
          "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)", "memory"
    );
}
// clang-format on


BytesView JitCompiler::MemNode::allocate(size_t size) {
	size = math::align(size, size_t(32));
	if (remains < size) {
		return BytesView();
	}

	auto ptr = current;
	current += size;
	remains -= size;

	memset(ptr, 0, size);

	return BytesView(ptr, size);
}

JitCompiler::MemNode *JitCompiler::allocateMemNode(MemNode *original) {
	auto size = original ? original->size : platform::getMemoryPageSize();
	auto p =
			::mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		return nullptr;
	}

	auto offset = math::align(sizeof(MemNode), size_t(32));

	auto node = new (p) MemNode;
	node->self = p;
	node->size = size;
	node->next = original;
	node->current = reinterpret_cast<uint8_t *>(p) + offset;
	node->remains = size - offset;

	return node;
}

JitCompiler::~JitCompiler() {
	while (memory) {
		auto mem = memory->self;
		auto size = memory->size;

		memory = memory->next;

		::munmap(mem, size);
	}
}

JitCompiler::JitCompiler() { }

BytesView JitCompiler::allocate(size_t size) {
	if (!memory) {
		memory = allocateMemNode(nullptr);
	}

	size = math::align(size, size_t(32));

	if (memory->remains < size) {
		memory = allocateMemNode(memory);
	}

	return memory->allocate(size);
}

auto JitCompiler::compileForeignCall(FunctionPtr ptr) -> FunctionPtr {
	std::unique_lock lock(mutex);

	auto segment = allocate(24);
	auto p = const_cast<uint8_t *>(segment.data());

	// make movabs instruction
	auto movimm = [](uint8_t *p, int reg, uint64_t val) {
		int rex;
		rex = AMD_REXW;
		if (reg & 8) {
			rex |= AMD_REXB;
		}
		*p++ = rex;
		*p++ = AMD_MOV_IMM | (reg & 7);
		memcpy(p, &val, sizeof(val));
		p += sizeof(val);
		return p;
	};

	p = movimm(p, 0 /* rax */, reinterpret_cast<uintptr_t>(ptr));
	p = movimm(p, 11 /* r11 */, reinterpret_cast<uintptr_t>(_foreignTrampo));

	// make jmp
	*p++ = 0x41;
	*p++ = 0xff;
	*p++ = 0xe3; // r11

	return reinterpret_cast<FunctionPtr>(const_cast<uint8_t *>(segment.data()));
}

ForeignDso *ForeignDso::open(void *ptr) {
	auto node = JitCompiler::allocateMemNode(nullptr);
	if (!node) {
		return nullptr;
	}

	auto d = node->allocate(sizeof(ForeignDso));
	auto out = const_cast<uint8_t *>(d.data());

	auto dso = new (out) ForeignDso;
	dso->_typeId = ForeignDso::TypeId;
	dso->compiler.memory = node;
	dso->handle = ptr;
	return dso;
}

void ForeignDso::close(ForeignDso *dso) {
	// extract memory from JitCompiler to safely call destructor
	auto memory = dso->compiler.memory;
	dso->compiler.memory = nullptr;

	dso->~ForeignDso();

	// unmap all memory
	while (memory) {
		auto mem = memory->self;
		auto size = memory->size;

		memory = memory->next;

		::munmap(mem, size);
	}
}

bool startLinuxLoader(int argc, const char *procArgv[]) {
	if (!s_loader.load(argc, procArgv)) {
		slog().error("abi::LinuxLoader",
				"Fail to load LinuxLoader: external DSO and FFI will not be available");
		return false;
	}

	// use our filename as a name for out helper binary
	// ??? should we use AT_EXECFN instead?
	auto file = procArgv[0];

	// pack return function address into argv for helper process
	char addrbuf[17];
	const char *argv[2];
	sprintn(addrbuf, (unsigned long)do_jump, 16);
	argv[0] = file;
	argv[1] = addrbuf;

	if (!setjmp(s_loader._jmpbuf)) {
		// run helper process
		s_loader.exec(2, argv);

		// it this function returns - we failed to start Loader
		slog().error("abi::LinuxLoader",
				"Fail to start LinuxLoader: external DSO and FFI will not be available");
		return false;
	}

	// Loader should be ready, check if it has all needed functions
	if (s_loader.dso.z_dlopen && s_loader.dso.z_dlclose && s_loader.dso.z_dlsym) {
		if (s_loader.setup()) {
			return true;
		}
	}
	return false;
}

ElfStackInfo ElfStackInfo::getForSp(unsigned long *sp) {
	ElfStackInfo info;
	info.start = sp;

	// read original runstruct (argv, env, aux)

	unsigned long *p = sp;
	info.argc = *(p++); // argc
	while (*p != 0) { ++p; } // argv
	++p; // = NULL

	// env
	info.envp = p;
	while (*p != 0) {
		++info.envc;
		++p;
	}
	++p; // = NULL

	// aux vector
	info.auxp = (Elf_auxv_t *)p;
	while (*p != 0) {
		++info.auxc;
		++p;
		++p;
	}
	p += 2; // = (0, 0)

	info.end = p;
	info.total = (p - sp);
	return info;
}

ElfStackInfo::Elf_long ElfStackInfo::getAuxValue(unsigned long type) const {
	auto aux = auxp;
	while (aux->a_type != 0) {
		if (aux->a_type == type) {
			return aux->a_un.a_val;
		}
		++aux;
	}
	return 0;
}

static auto ERROR_LOADER_INVALID = "stappler-abi: LinuxLoader: no foreign dlopen defined";
static auto ERROR_NO_SELF = "stappler-abi: LinuxLoader can not open application itself";
static auto ERROR_NO_FOREIGN_MEM =
		"stappler-abi: LinuxLoader fail to allocate memory for ForeignDso";

ForeignDso *openForeign(StringView name, DsoFlags flags, const char **err) {
	if (!s_loader.dso.z_dlopen) {
		*err = ERROR_LOADER_INVALID;
	}

	if (hasFlag(flags, DsoFlags::Self) || name.empty()) {
		*err = ERROR_NO_SELF;
		return nullptr;
	}

	int f = 0;
	if (hasFlag(flags, DsoFlags::Lazy)) {
		f |= RTLD_LAZY;
	}
	if (hasFlag(flags, DsoFlags::Global)) {
		f |= RTLD_GLOBAL;
	}

	auto path = name.str<memory::StandartInterface>();

	_setTcbAddr(s_loader._header.foreignTcb);

	auto handle = s_loader.dso.z_dlopen(path.data(), RTLD_NOW);

	_setTcbAddr(s_loader._header.originalTcb);

	if (!handle) {
		*err = s_loader.dso.z_dlerror();
		return nullptr;
	}

	auto dso = ForeignDso::open(handle);
	if (dso) {
		return dso;
	}

	*err = ERROR_NO_FOREIGN_MEM;
	return nullptr;
}

void closeForeign(DsoFlags flags, ForeignDso *handle) { ForeignDso::close(handle); }

void *symForeign(ForeignDso *h, StringView name, DsoSymFlags flags, const char **err) {
	auto sym = s_loader.dso.z_dlsym(h->handle,
			name.terminated() ? name.data() : name.str<memory::StandartInterface>().data());
	if (sym) {
		if (hasFlag(flags, DsoSymFlags::Executable)) {
			sym = h->compiler.compileForeignCall(sym);
		}
		return sym;
	}

	*err = s_loader.dso.z_dlerror();
	return nullptr;
}

struct ThreadHeader {
	LoaderHeader header;
	pthread_t preparedThread;
	pthread_t foreignThread;

	void *(*cb)(void *) = nullptr;
	void *arg = nullptr;
};

static void *_start_thread(void *arg) {
	auto header = reinterpret_cast<ThreadHeader *>(arg);

	_setLoaderAddr(reinterpret_cast<uintptr_t>(&header->header));

	_getTcbAddr(&header->header.foreignTcb);
	_setTcbAddr(header->header.originalTcb);

	__sp_pthread_initialize(header->preparedThread, gettid());

	auto result = header->cb(header->arg);

	// header can be unmapped by finalizer, preserve foreign TCB
	auto foreignTcb = header->header.foreignTcb;

	__sp_pthread_finalize(header->preparedThread, (void *)SP_MAGIC);

	// before exit, set TCB to foreign to successfully exit
	// after return, we will be in foreign libc finalization code
	_setTcbAddr(foreignTcb);

	// disable loader for this thread
	_setLoaderAddr(0);
	return result;
}

pthread_t startForeignThread(void *(*cb)(void *), void *arg, uint32_t f) {
	unsigned long tcb = 0;
	sigset_t sig;
	pthread_t preparedThread;
	_ThreadFlags flags = _ThreadFlags(f);

	auto dState = hasFlag(flags, _ThreadFlags::Joinable) ? PTHREAD_CREATE_JOINABLE
														 : PTHREAD_CREATE_DETACHED;

	if (__sp_pthread_prepare(&preparedThread, &sig, &tcb, dState) != 0) {
		return pthread_t(0);
	}

	void *headerLocation = nullptr;
	if (__sp_pthread_get_header(preparedThread, &headerLocation) != 0) {
		__sp_pthread_cancel(preparedThread, &sig);
		return pthread_t(0);
	}

	auto header = new (headerLocation) ThreadHeader;
	header->header.originalTcb = tcb;
	header->header.root = &s_loader;
	header->preparedThread = preparedThread;
	header->cb = cb;
	header->arg = arg;

	if (s_loader.z_pthread_create(&header->foreignThread, nullptr, _start_thread, header) != 0) {
		__sp_pthread_cancel(preparedThread, &sig);
	}

	__sp_pthread_attach(preparedThread, &sig, 0);

	return preparedThread;
}

void *joinForeignThread(pthread_t preparedThread) {
	void *result = nullptr;
	void *headerLocation = nullptr;
	if (__sp_pthread_is_attached(preparedThread)
			|| __sp_pthread_get_header(preparedThread, &headerLocation) != 0) {
		perror("Only attached foreign threads can be joined with joinForeignThread");
		abort();
		return nullptr;
	}

	auto header = reinterpret_cast<ThreadHeader *>(headerLocation);

	// first - join foreign thread, from which we receive result
	header->header.root->z_pthread_join(header->foreignThread, &result);

	// then - join our thread to correctly destroy it's TCB and pseudostack
	pthread_join(header->preparedThread, nullptr);

	return result;
}

void detachForeignThread(pthread_t preparedThread) {
	void *headerLocation = nullptr;
	if (__sp_pthread_is_attached(preparedThread)
			|| __sp_pthread_get_header(preparedThread, &headerLocation) != 0) {
		perror("Only attached foreign threads can be joined with detachForeignThread");
		abort();
		return;
	}

	auto header = reinterpret_cast<ThreadHeader *>(headerLocation);

	// preserve foreign thread address, becouse detach can destroy header
	auto foreignThread = header->foreignThread;

	// by first - detach out thread, as it's virtually noop (but do not use header after this)
	pthread_detach(header->preparedThread);

	// then detach actual foreign thread
	s_loader.z_pthread_detach(foreignThread);
}

} // namespace stappler::abi
