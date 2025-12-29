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

#ifndef CORE_ABI_LINUX_SPABILINUXELF_H_
#define CORE_ABI_LINUX_SPABILINUXELF_H_

#include "SPStringView.h" // IWYU pragma: keep
#include "SPFilesystem.h"
#include "SPDso.h"

// ELF file format implementation
// (https://refspecs.linuxbase.org/elf/elf.pdf)
//
// Neded for Dso implementation on Linux for statically-linked apps
//
namespace STAPPLER_VERSIONIZED stappler::abi {

using Elf32_Addr = uint32_t;
using Elf32_Half = uint16_t;
using Elf32_Off = uint32_t;
using Elf32_Sword = int32_t;
using Elf32_Word = uint32_t;

using Elf64_Addr = uint64_t;
using Elf64_Half = uint16_t;
using Elf64_Off = uint64_t;
using Elf64_Sword = int32_t;
using Elf64_Word = uint32_t;
using Elf64_Xword = uint64_t;
using Elf64_Sxword = int64_t;

static constexpr int EI_MAG0 = 0;
static constexpr int EI_MAG1 = 1;
static constexpr int EI_MAG2 = 2;
static constexpr int EI_MAG3 = 3;
static constexpr int EI_CLASS = 4;
static constexpr int EI_DATA = 5;
static constexpr int EI_VERSION = 6;
static constexpr int EI_OSABI = 7;
static constexpr int EI_ABIVERSION = 8;
static constexpr int EI_PAD = 9;
static constexpr int EI_NIDENT = 16;

enum ElfClass : uint8_t {
	ELFCLASS32 = 1,
	ELFCLASS64 = 2
};

enum ElfVersion : uint8_t {
	EV_NONE = 0,
	EV_CURRENT = 1,
};

enum ElfDataFormat : uint8_t {
	ELFDATA2LSB = 1,
	ELFDATA2MSB = 2
};

enum class ElfOsAbi : uint8_t {
	SystemV = 0x00,
	HP_UX = 0x01,
	NetBSD = 0x02,
	Linux = 0x03,
	GNU_Hurd = 0x04,
	Solaris = 0x06,
	AIX = 0x07,
	IRIX = 0x08,
	FreeBSD = 0x09,
	Tru64 = 0x0A,
	NovellModesto = 0x0B,
	OpenBSD = 0x0C,
	OpenVMS = 0x0D,
	NonStopKernel = 0x0E,
	AROS = 0x0F,
	FenixOS = 0x10,
	NuxiCloudABI = 0x11,
	OpenVOS = 0x12
};

enum ElfType : uint16_t {
	ET_NONE = 0,
	ET_REL = 1,
	ET_EXEC = 2,
	ET_DYN = 3,
	ET_CORE = 4,
	ET_LOOS = 0xFE00,
	ET_HIOS = 0xFEFF,
	ET_LOPROC = 0xff00,
	ET_HIPROC = 0xffff
};

enum class ElfMachine {
	None = 0,
	WE_32100 = 0x01,
	SPARC = 0x02,
	x86 = 0x03,
	m68000 = 0x04,
	m88000 = 0x05,
	MCU = 0x06,
	i80860 = 0x07,
	MIPS = 0x08,
	System_370 = 0x09,
	MIPS_RS3000_LE = 0x0A,
	PA_RISC = 0x0F,
	i80960 = 0x13,
	PowerPC = 0x14,
	PowerPC_64bit = 0x15,
	S390 = 0x16,
	SPU_SPC = 0x17,
	V800 = 0x24,
	FR20 = 0x25,
	TRW_RH_32 = 0x26,
	RCE = 0x27,
	Arm = 0x28,
	DigitalAlpha = 0x29,
	SuperH = 0x2A,
	SPARC_V9 = 0x2B,
	TriCore = 0x2C,
	ArgonautRISC = 0x2D,
	H8_300 = 0x2E,
	H8_300H = 0x2F,
	H8S = 0x30,
	H8_500 = 0x31,
	IA_64 = 0x32,
	MIPS_X = 0x33,
	ColdFire = 0x34,
	M68HC12 = 0x35,
	MMA = 0x36,
	PCP = 0x37,
	nCPU_RISC = 0x38,
	NDR1 = 0x39,
	StarCore = 0x3A,
	ME16 = 0x3B,
	ST100 = 0x3C,
	TinyJ = 0x3D,
	x86_64 = 0x3E,
	SonyDSP = 0x3F,
	PDP_10 = 0x40,
	PDP_11 = 0x41,
	FX66 = 0x42,
	ST9_8_16bit = 0x43,
	ST7_8bit = 0x44,
	MC68HC16 = 0x45,
	MC68HC11 = 0x46,
	MC68HC08 = 0x47,
	MC68HC05 = 0x48,
	SVx = 0x49,
	ST19_8bit = 0x4A,
	DigitalVAX = 0x4B,
	Axis_32bit = 0x4C,
	Infineon_32bit = 0x4D,
	Element14_64bit = 0x4E,
	LSI_16bit = 0x4F,
	TMS320C6000 = 0x8C,
	e2k = 0xAF,
	AArch64 = 0xB7,
	ZilogZ80 = 0xDC,
	RISCV = 0xF3,
	BerkeleyPacketFilter = 0xF7,
	WDC65C816 = 0x101,
	LoongArch = 0x102,
};

enum ElfSectionType : uint32_t {
	SHT_NULL = 0,
	SHT_PROGBITS = 1,
	SHT_SYMTAB = 2,
	SHT_STRTAB = 3,
	SHT_RELA = 4,
	SHT_HASH = 5,
	SHT_DYNAMIC = 6,
	SHT_NOTE = 7,
	SHT_NOBITS = 8,
	SHT_REL = 9,
	SHT_SHLIB = 10,
	SHT_DYNSYM = 11
};

enum ElfProgramEntryType {
	PT_NULL = 0,
	PT_LOAD = 1,
	PT_DYNAMIC = 2,
	PT_INTERP = 3,
	PT_NOTE = 4,
	PT_SHLIB = 5,
	PT_PHDR = 6
};

static constexpr uint32_t PF_X = 0x1;
static constexpr uint32_t PF_W = 0x2;
static constexpr uint32_t PF_R = 0x4;

static constexpr uint32_t AT_NULL = 0; /* End of vector */
static constexpr uint32_t AT_IGNORE = 1; /* Entry should be ignored */
static constexpr uint32_t AT_EXECFD = 2; /* File descriptor of program */
static constexpr uint32_t AT_PHDR = 3; /* Program headers for program */
static constexpr uint32_t AT_PHENT = 4; /* Size of program header entry */
static constexpr uint32_t AT_PHNUM = 5; /* Number of program headers */
static constexpr uint32_t AT_PAGESZ = 6; /* System page size */
static constexpr uint32_t AT_BASE = 7; /* Base address of interpreter */
static constexpr uint32_t AT_FLAGS = 8; /* Flags */
static constexpr uint32_t AT_ENTRY = 9; /* Entry point of program */
static constexpr uint32_t AT_NOTELF = 10; /* Program is not ELF */
static constexpr uint32_t AT_UID = 11; /* Real uid */
static constexpr uint32_t AT_EUID = 12; /* Effective uid */
static constexpr uint32_t AT_GID = 13; /* Real gid */
static constexpr uint32_t AT_EGID = 14; /* Effective gid */
static constexpr uint32_t AT_HWCAP = 16;
static constexpr uint32_t AT_CLKTCK = 17; /* Frequency of times() */
static constexpr uint32_t AT_HWCAP2 = 26;
static constexpr uint32_t AT_EXECFN = 31;

struct Elf32_Ehdr {
	uint8_t e_ident[EI_NIDENT];
	Elf32_Half e_type;
	Elf32_Half e_machine;
	Elf32_Word e_version;
	Elf32_Addr e_entry;
	Elf32_Off e_phoff;
	Elf32_Off e_shoff;
	Elf32_Word e_flags;
	Elf32_Half e_ehsize;
	Elf32_Half e_phentsize;
	Elf32_Half e_phnum;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum;
	Elf32_Half e_shstrndx;
};

struct Elf64_Ehdr {
	uint8_t e_ident[EI_NIDENT];
	Elf64_Half e_type;
	Elf64_Half e_machine;
	Elf64_Word e_version;
	Elf64_Addr e_entry;
	Elf64_Off e_phoff;
	Elf64_Off e_shoff;
	Elf64_Word e_flags;
	Elf64_Half e_ehsize;
	Elf64_Half e_phentsize;
	Elf64_Half e_phnum;
	Elf64_Half e_shentsize;
	Elf64_Half e_shnum;
	Elf64_Half e_shstrndx;
};

struct Elf32_Shdr {
	Elf32_Word sh_name;
	Elf32_Word sh_type;
	Elf32_Word sh_flags;
	Elf32_Addr sh_addr;
	Elf32_Off sh_offset;
	Elf32_Word sh_size;
	Elf32_Word sh_link;
	Elf32_Word sh_info;
	Elf32_Word sh_addralign;
	Elf32_Word sh_entsize;
};

struct Elf64_Shdr {
	Elf64_Word sh_name;
	Elf64_Word sh_type;
	Elf64_Xword sh_flags;
	Elf64_Addr sh_addr;
	Elf64_Off sh_offset;
	Elf64_Xword sh_size;
	Elf64_Word sh_link;
	Elf64_Word sh_info;
	Elf64_Xword sh_addralign;
	Elf64_Xword sh_entsize;
};

struct Elf32_Phdr {
	Elf32_Word p_type;
	Elf32_Off p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
};

struct Elf64_Phdr {
	Elf64_Word p_type;
	Elf64_Word p_flags;
	Elf64_Off p_offset;
	Elf64_Addr p_vaddr;
	Elf64_Addr p_paddr;
	Elf64_Xword p_filesz;
	Elf64_Xword p_memsz;
	Elf64_Xword p_align;
};

struct Elf32_auxv_t {
	uint32_t a_type;
	union {
		uint32_t a_val;
	} a_un;
};

struct Elf64_auxv_t {
	uint64_t a_type;
	union {
		uint64_t a_val;
	} a_un;
};

class SP_PUBLIC ElfFile : public memory::PoolObject {
public:
	struct Section {
		StringView name;
		BytesView header;
		BytesView data;

		ElfSectionType type = SHT_NULL;
		uint64_t fileOffset = 0;
		uint64_t addralign = 0;
		uint64_t entsize = 0;
	};

	struct ProgramEntity {
		BytesView header;
		BytesView data;

		ElfProgramEntryType type = PT_NULL;
		uint64_t offset = 0;
		uint64_t filesize = 0;
		uint64_t vaddr = 0;
		uint64_t memsize = 0;
		uint64_t align = 0;
		uint64_t flags = 0;
	};

	struct MappedSegment {
		uint8_t *ptr = nullptr;
		size_t size = 0;
		int flags = 0;

		operator bool() const { return ptr != nullptr; }
	};

	virtual ~ElfFile();

	static SharedRc<ElfFile> create(const FileInfo &);
	static SharedRc<ElfFile> create(BytesView, bool dup);

	bool init(const FileInfo &);
	bool init(BytesView, bool dup);

	using PoolObject::PoolObject;

	ElfClass getClass() const;
	ElfDataFormat getDataFormat() const;
	ElfOsAbi getOsAbi() const;
	uint8_t getAbiVersion() const;
	ElfType getType() const;
	ElfMachine getMachine() const;

	uint16_t getValue(uint16_t) const;
	uint32_t getValue(uint32_t) const;
	uint64_t getValue(uint64_t) const;

	StringView getInterp() const;

	SpanView<Section> getSections() const;

	// try to run object with it's interpreter
	bool load(unsigned long *originalStackPointer);

	uint64_t map();
	void unmap();

	uint64_t getProgramHeaderOffset() const;
	uint16_t getProgramHeaderEntrySize() const;
	uint16_t getProgramHeaderEntryCount() const;

	uint64_t getSectionHeaderOffset() const;
	uint16_t getSectionHeaderEntrySize() const;
	uint16_t getSectionHeaderEntryCount() const;

	uint64_t getEntryPoint() const;
	uintptr_t getBaseAddress() const;

	// check if specific pointer within one of segments, and returns this segment
	MappedSegment getMapping(uintptr_t) const;

protected:
	uint16_t getSectionNameStringTableIndex() const;

	SpanView<Section> extractSectionHeaders(BytesView file, bool dup) const;
	SpanView<ProgramEntity> extractProgramHeaders(BytesView file, bool dup) const;

	void runelf(unsigned long *sp, int argc, const char *argv[]);

	BytesView _header;

	SpanView<Section> _sectionHeaders;
	SpanView<ProgramEntity> _programHeaders;
	Map<StringView, const Section *> _sections;

	uint32_t _pageSize = 0;
	unsigned char *_mappingBaseAddr = nullptr;
	SpanView<MappedSegment> _mappedSegments;
};

} // namespace stappler::abi

#endif // CORE_ABI_LINUX_SPABILINUXELF_H_
