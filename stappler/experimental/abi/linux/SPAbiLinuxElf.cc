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
#include "SPPlatform.h"

#include <sys/mman.h>

namespace STAPPLER_VERSIONIZED stappler::abi {

SharedRc<ElfFile> ElfFile::create(const FileInfo &info) {
	return SharedRc<ElfFile>::create(SharedRefMode::Allocator, info);
}

SharedRc<ElfFile> ElfFile::create(BytesView data, bool dup) {
	return SharedRc<ElfFile>::create(SharedRefMode::Allocator, data, dup);
}

ElfFile::~ElfFile() { unmap(); }

bool ElfFile::init(const FileInfo &filePath) {
	auto absPath = filesystem::findPath<memory::PoolInterface>(filePath);

	if (!filesystem::exists(FileInfo{absPath})) {
		slog().error("ElfFile", "Fail to open file: ", absPath, ": not exists");
		return false;
	}

	auto map = filesystem::MemoryMappedRegion::mapFile(FileInfo{absPath},
			filesystem::MappingType::Private, filesystem::ProtFlags::MapRead);
	if (!map) {
		slog().error("ElfFile", "Fail to open file: ", absPath, ": fail to map file");
		return false;
	}

	return init(map.getView(), true);
}

bool ElfFile::init(BytesView data, bool dup) {
	static constexpr uint8_t s_elfSig[] = {0x7f, 'E', 'L', 'F'};

	if (data.size() < 64 || !data.starts_with<sizeof(s_elfSig)>(s_elfSig)) {
		slog().error("ElfFile", "Fail to opex file: invalid magic bytes");
		return false;
	}

	// read ident without moving pointer
	auto ident = BytesView(data).readBytes(EI_NIDENT);

	if (ident[EI_VERSION] != EV_CURRENT) {
		slog().error("ElfFile",
				"Fail to opex file: invalid ELF header version: ", int(ident[EI_VERSION]));
		return false;
	}

	switch (ident[EI_DATA]) {
	case ELFDATA2LSB:
	case ELFDATA2MSB: break;
	default:
		slog().error("ElfFile",
				"Fail to opex file: invalid ELF data format: ", int(ident[EI_DATA]));
		return false;
	}

	switch (ident[EI_CLASS]) {
	case ELFCLASS32:
		_header = BytesView(data).readBytes(sizeof(Elf32_Ehdr));
		if (_header.size() != sizeof(Elf32_Ehdr)) {
			slog().error("ElfFile", "Fail to opex file: invalid ELF header size");
			return false;
		}
		break;
	case ELFCLASS64:
		_header = BytesView(data).readBytes(sizeof(Elf64_Ehdr));
		if (_header.size() != sizeof(Elf64_Ehdr)) {
			slog().error("ElfFile", "Fail to opex file: invalid ELF header size");
			return false;
		}
		break;
	}

	if (_header.empty()) {
		slog().error("ElfFile", "Fail to opex file: fail to load elf header");
		return false;
	}

	if (dup) {
		_header = _header.pdup(getPool());
	}

	_sectionHeaders = extractSectionHeaders(data, dup);
	_programHeaders = extractProgramHeaders(data, dup);

	for (auto &it : _sectionHeaders) { _sections.emplace(it.name, &it); }

	_pageSize = platform::getMemoryPageSize();

	return true;
}

ElfClass ElfFile::getClass() const { return ElfClass(_header[EI_CLASS]); }

ElfDataFormat ElfFile::getDataFormat() const { return ElfDataFormat(_header[EI_DATA]); }

ElfOsAbi ElfFile::getOsAbi() const { return ElfOsAbi(_header[EI_OSABI]); }

uint8_t ElfFile::getAbiVersion() const { return _header[EI_ABIVERSION]; }

ElfType ElfFile::getType() const {
	switch (getClass()) {
	case ELFCLASS32:
		return ElfType(getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_type));
		break;
	case ELFCLASS64:
		return ElfType(getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_type));
		break;
	}
	return ET_NONE;
}

ElfMachine ElfFile::getMachine() const {
	switch (getClass()) {
	case ELFCLASS32:
		return ElfMachine(
				getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_machine));
		break;
	case ELFCLASS64:
		return ElfMachine(
				getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_machine));
		break;
	}
	return ElfMachine::None;
}

uint16_t ElfFile::getValue(uint16_t value) const {
	switch (getDataFormat()) {
	case ELFDATA2LSB: return byteorder::LittleToHost(value); break;
	case ELFDATA2MSB: return byteorder::BigToHost(value); break;
	}
	return 0;
}

uint32_t ElfFile::getValue(uint32_t value) const {
	switch (getDataFormat()) {
	case ELFDATA2LSB: return byteorder::LittleToHost(value); break;
	case ELFDATA2MSB: return byteorder::BigToHost(value); break;
	}
	return 0;
}

uint64_t ElfFile::getValue(uint64_t value) const {
	switch (getDataFormat()) {
	case ELFDATA2LSB: return byteorder::LittleToHost(value); break;
	case ELFDATA2MSB: return byteorder::BigToHost(value); break;
	}
	return 0;
}


StringView ElfFile::getInterp() const {
	auto it = _sections.find(StringView(".interp"));
	if (it != _sections.end()) {
		return BytesView(it->second->data).readString();
	}
	return StringView();
}

SpanView<ElfFile::Section> ElfFile::getSections() const { return _sectionHeaders; }

bool ElfFile::load(unsigned long *originalStackPointer) {
	unsigned long *p = originalStackPointer;
	p++;

	const char *argv[1];
	argv[0] = *(char **)p;

	/*z_sprintn(addrbuf, (unsigned long)do_jump, 16);
	argv[1] = addrbuf;*/

	runelf(originalStackPointer, 1, argv);
	return false;
}

uint64_t ElfFile::getProgramHeaderOffset() const {
	switch (getClass()) {
	case ELFCLASS32:
		return getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_phoff);
		break;
	case ELFCLASS64:
		return getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_phoff);
		break;
	}
	return 0;
}
uint16_t ElfFile::getProgramHeaderEntrySize() const {
	switch (getClass()) {
	case ELFCLASS32:
		return getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_phentsize);
		break;
	case ELFCLASS64:
		return getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_phentsize);
		break;
	}
	return 0;
}
uint16_t ElfFile::getProgramHeaderEntryCount() const {
	switch (getClass()) {
	case ELFCLASS32:
		return getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_phnum);
		break;
	case ELFCLASS64:
		return getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_phnum);
		break;
	}
	return 0;
}

uint64_t ElfFile::getSectionHeaderOffset() const {
	switch (getClass()) {
	case ELFCLASS32:
		return getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_shoff);
		break;
	case ELFCLASS64:
		return getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_shoff);
		break;
	}
	return 0;
}
uint16_t ElfFile::getSectionHeaderEntrySize() const {
	switch (getClass()) {
	case ELFCLASS32:
		return getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_shentsize);
		break;
	case ELFCLASS64:
		return getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_shentsize);
		break;
	}
	return 0;
}
uint16_t ElfFile::getSectionHeaderEntryCount() const {
	switch (getClass()) {
	case ELFCLASS32:
		return getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_shnum);
		break;
	case ELFCLASS64:
		return getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_shnum);
		break;
	}
	return 0;
}

uint16_t ElfFile::getSectionNameStringTableIndex() const {
	switch (getClass()) {
	case ELFCLASS32:
		return getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_shstrndx);
		break;
	case ELFCLASS64:
		return getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_shstrndx);
		break;
	}
	return 0;
}

uint64_t ElfFile::getEntryPoint() const {
	switch (getClass()) {
	case ELFCLASS32:
		return getValue(reinterpret_cast<const Elf32_Ehdr *>(_header.data())->e_entry);
		break;
	case ELFCLASS64:
		return getValue(reinterpret_cast<const Elf64_Ehdr *>(_header.data())->e_entry);
		break;
	}
	return 0;
}

uintptr_t ElfFile::getBaseAddress() const { return reinterpret_cast<uint64_t>(_mappingBaseAddr); }

SpanView<ElfFile::Section> ElfFile::extractSectionHeaders(BytesView file, bool dup) const {
	auto stringTableIndex = getSectionNameStringTableIndex();

	uint64_t offet = getSectionHeaderOffset();
	auto count = getSectionHeaderEntryCount();
	auto eltSize = getSectionHeaderEntrySize();
	uint32_t size = count * eltSize;

	auto sectionHeaders = file.sub(offet, size);
	if (dup) {
		sectionHeaders = sectionHeaders.pdup(getPool());
	}
	if (sectionHeaders.size() != size) {
		slog().error("ElfFile", "Fail to load ELF section header: invalid size/offset");
		return SpanView<Section>();
	}

	auto sectArrData = (Section *)memory::pool::palloc(getPool(), count * sizeof(Section));
	for (uint64_t i = 0; i < count; ++i) {
		uint64_t addralign = 0;
		uint64_t entsize = 0;
		ElfSectionType type = SHT_NULL;
		uint64_t fileOffet = 0;
		auto sectHeader = sectionHeaders.readBytes(eltSize);
		BytesView sectData;
		switch (getClass()) {
		case ELFCLASS32:
			if (sectHeader.size() < sizeof(Elf32_Shdr)) {
				slog().error("ElfFile",
						"Fail to load ELF section header: invalid header entry size");
				return SpanView<Section>();
			} else {
				auto h = reinterpret_cast<const Elf32_Shdr *>(sectHeader.data());
				if (getValue(h->sh_type) != SHT_NOBITS) {
					addralign = getValue(h->sh_addralign);
					entsize = getValue(h->sh_entsize);
					type = ElfSectionType(getValue(h->sh_type));
					fileOffet = getValue(h->sh_offset);
					auto sectSize = getValue(h->sh_size);
					sectData = file.sub(fileOffet, sectSize);
					if (sectData.size() != sectSize) {
						slog().error("ElfFile",
								"Fail to load ELF section header: invalid section data size");
						return SpanView<Section>();
					}
				}
			}
			break;
		case ELFCLASS64:
			if (sectHeader.size() < sizeof(Elf64_Shdr)) {
				slog().error("ElfFile",
						"Fail to load ELF section header: invalid header entry size");
				return SpanView<Section>();
			} else {
				auto h = reinterpret_cast<const Elf64_Shdr *>(sectHeader.data());
				if (getValue(h->sh_type) != SHT_NOBITS) {
					addralign = getValue(h->sh_addralign);
					entsize = getValue(h->sh_entsize);
					type = ElfSectionType(getValue(h->sh_type));
					fileOffet = getValue(h->sh_offset);
					auto sectSize = getValue(h->sh_size);
					sectData = file.sub(fileOffet, sectSize);
					if (sectData.size() != sectSize) {
						slog().error("ElfFile",
								"Fail to load ELF section header: invalid section data size");
						return SpanView<Section>();
					}
				}
			}
			break;
		}

		if (dup) {
			sectData = sectData.pdup(getPool());
		}

		sectArrData[i] = Section{
			StringView(),
			sectHeader,
			sectData,
			type,
			fileOffet,
			addralign,
			entsize,
		};
	}

	auto ret = SpanView<Section>(sectArrData, count);

	if (stringTableIndex != 0 && stringTableIndex < ret.size()) {
		auto &stringTableSection = ret[stringTableIndex];

		for (auto &it : ret) {
			auto nameOff = getValue(BytesView(it.header).readUnsigned32());
			if (nameOff >= stringTableSection.data.size() - 1) {
				slog().error("ElfFile",
						"Fail to load ELF section header: invalid section name offset: ", nameOff);
				return SpanView<Section>();
			} else {
				const_cast<Section &>(it).name = stringTableSection.data.sub(nameOff).readString();
			}
		}
	}

	return ret;
}

SpanView<ElfFile::ProgramEntity> ElfFile::extractProgramHeaders(BytesView file, bool dup) const {
	uint64_t offet = getProgramHeaderOffset();
	auto count = getProgramHeaderEntryCount();
	auto eltSize = getProgramHeaderEntrySize();
	uint32_t size = count * eltSize;
	auto programHeaders = file.sub(offet, size);
	if (dup) {
		programHeaders = programHeaders.pdup(getPool());
	}
	if (programHeaders.size() != size) {
		slog().error("ElfFile", "Fail to load ELF program header: invalid size/offset");
		return SpanView<ProgramEntity>();
	}

	auto pData = (ProgramEntity *)memory::pool::palloc(getPool(), count * sizeof(ProgramEntity));
	for (uint64_t i = 0; i < count; ++i) {
		auto entHeader = programHeaders.readBytes(eltSize);

		BytesView fileData;
		ElfProgramEntryType type = PT_NULL;
		uint64_t offset = 0;
		uint64_t filesize = 0;
		uint64_t vaddr = 0;
		uint64_t memsize = 0;
		uint64_t align = 0;
		uint64_t flags = 0;

		auto readHeader = [&](auto h) {
			type = (ElfProgramEntryType)getValue(h->p_type);
			offset = getValue(h->p_offset);
			filesize = getValue(h->p_filesz);
			vaddr = getValue(h->p_vaddr);
			memsize = getValue(h->p_memsz);
			flags = getValue(h->p_flags);
			align = getValue(h->p_align);
			if (filesize > 0) {
				fileData = file.sub(offset, filesize);
				if (fileData.size() != filesize) {
					slog().error("ElfFile",
							"Fail to load ELF section header: invalid section data size");
					fileData = BytesView();
				}
			}
		};

		switch (getClass()) {
		case ELFCLASS32:
			if (entHeader.size() < sizeof(Elf32_Phdr)) {
				slog().error("ElfFile",
						"Fail to load ELF section header: invalid header entry size");
				return SpanView<ProgramEntity>();
			} else {
				readHeader(reinterpret_cast<const Elf32_Phdr *>(entHeader.data()));
			}
			break;
		case ELFCLASS64:
			if (entHeader.size() < sizeof(Elf64_Phdr)) {
				slog().error("ElfFile",
						"Fail to load ELF section header: invalid header entry size");
				return SpanView<ProgramEntity>();
			} else {
				readHeader(reinterpret_cast<const Elf64_Phdr *>(entHeader.data()));
			}
			break;
		}

		if (dup) {
			fileData = fileData.pdup(getPool());
		}

		pData[i] = ProgramEntity{
			entHeader,
			fileData,
			type,
			offset,
			filesize,
			vaddr,
			memsize,
			align,
			flags,
		};
	}

	return SpanView<ProgramEntity>(pData, count);
}

void ElfFile::runelf(unsigned long *origSp, int argc, const char *argv[]) {
	auto interpElf = ElfFile::create(FileInfo{getInterp()});
	if (!interpElf) {
		slog().error("ElfFile", "Fail to find interp: ", getInterp());
		return;
	}

	auto selfType = getType();
	if (selfType != ET_DYN && selfType != ET_EXEC) {
		return;
	}

	if (!map()) {
		return;
	}

	if (!interpElf->map()) {
		return;
	}
}

uint64_t ElfFile::map() {
	if (_mappingBaseAddr != nullptr || !_mappedSegments.empty()) {
		return 0;
	}

	uint64_t minva = -1, maxva = 0;

	auto ROUND_PG = [this](auto x) {
		auto align = _pageSize - 1;
		return ((x) + (align)) & ~(align);
	};

	auto TRUNC_PG = [this](auto x) {
		auto align = _pageSize - 1;
		return (x) & ~(align);
	};

	size_t segmentsCount = 0;

	// Find required mapping size
	for (auto &iter : _programHeaders) {
		if (iter.type != PT_LOAD) {
			continue;
		}

		++segmentsCount;

		if (iter.vaddr < minva) {
			minva = iter.vaddr;
		}
		if (iter.vaddr + iter.memsize > maxva) {
			maxva = iter.vaddr + iter.memsize;
		}
	}

	minva = TRUNC_PG(minva);
	maxva = ROUND_PG(maxva);

	bool dyn = getType() == ET_DYN;

	/* For dynamic ELF let the kernel chose the address. */
	unsigned char *hint = dyn ? NULL : (unsigned char *)minva;

	/* Check that we can hold the whole image. */
	auto base = (unsigned char *)::mmap((void *)hint, maxva - minva, PROT_NONE,
			(dyn ? 0 : MAP_FIXED) | (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
	if (base == MAP_FAILED) {
		return false;
	}

	::munmap(base, maxva - minva);

	bool failed = false;

	MappedSegment *segs =
			(MappedSegment *)memory::pool::calloc(getPool(), segmentsCount, sizeof(MappedSegment));

	uint32_t idx = 0;
	for (auto &iter : _programHeaders) {
		if (iter.type != PT_LOAD) {
			continue;
		}

		auto off = iter.vaddr & (_pageSize - 1);
		auto start = dyn ? (unsigned long)base : 0;
		start += TRUNC_PG(iter.vaddr);

		auto sz = ROUND_PG(iter.memsize + off);

		auto p = (unsigned char *)::mmap((void *)start, sz, PROT_WRITE,
				MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (p != MAP_FAILED) {
			auto flags = ((iter.flags & PF_R) ? PROT_READ : 0)
					| ((iter.flags & PF_W) ? PROT_WRITE : 0)
					| ((iter.flags & PF_X) ? PROT_EXEC : 0);
			if (!iter.data.empty()) {
				::memcpy(p + off, iter.data.data(), iter.data.size());
			} else {
				slog().warn("ElfFile", "Invalid file size for binding");
			}
			::mprotect(p, sz, flags);
			segs[idx++] = MappedSegment{p, sz, flags};
		} else {
			failed = false;
		}
	}

	if (!failed) {
		_mappingBaseAddr = base;
		_mappedSegments = makeSpanView(segs, segmentsCount);
		return reinterpret_cast<uint64_t>(_mappingBaseAddr);
	}

	for (idx = 0; idx < segmentsCount; ++idx) {
		if (segs[idx++].ptr) {
			::munmap(segs[idx].ptr, segs[idx].size);
		}
	}

	return 0;
}

void ElfFile::unmap() {
	for (auto &it : _mappedSegments) {
		if (it.ptr && it.size) {
			::munmap(it.ptr, it.size);
		}
	}
	_mappedSegments = SpanView<MappedSegment>();
	_mappingBaseAddr = nullptr;
}

ElfFile::MappedSegment ElfFile::getMapping(uintptr_t ptr) const {
	for (auto &it : _mappedSegments) {
		if (ptr >= uintptr_t(it.ptr) && ptr < uintptr_t(it.ptr) + it.size) {
			return it;
		}
	}
	return MappedSegment();
}

} // namespace stappler::abi
