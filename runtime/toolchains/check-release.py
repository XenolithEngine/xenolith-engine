#!/usr/bin/env python3
# Copyright (c) 2026 Stappler Team <admin@stappler.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Release pre-flight for the SDK packages in hosts/ and targets/.

`make release-check` only asks whether every package has a `release` stamp with
the current tag. This goes through what the packages hold, without building
anything:

  * the tag in every `release` stamp;
  * the files every host and every target family must carry (compiler, linkers,
    debugger, shader tools, host.mk / target.mk, licenses, the bundled
    third-party libraries);
  * that every binary is built for the package's architecture - executables and
    shared libraries by their header, static archives by their first object;
  * what host binaries load at run time: anything that is neither inside the
    package nor part of the OS means the host is not self-contained;
  * that sibling targets of one family (the four linux-gnu ones, the two
    windows ones, ...) carry the same set of libraries;
  * absolute or dangling symlinks and stray directories, all of which would
    ship in the tarball as they are;
  * with --archives, the staged release/ archives: present, not older than the
    package, and with a good signature.

The package lists are read from the Makefile (RELEASE_HOSTS / RELEASE_TARGETS),
so this script and `make release-export` cannot disagree about what a release is.

Exit status is 0 when nothing is wrong, 1 otherwise. Warnings do not fail the
run unless --strict is given.
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------------------
# Reporting

class Report:
	def __init__(self, verbose):
		self.verbose = verbose
		self.errors = 0
		self.warnings = 0
		self.current = None
		self.printed_header = False

	def begin(self, name):
		self.current = name
		self.printed_header = False

	def _header(self):
		if not self.printed_header:
			print(f'== {self.current}')
			self.printed_header = True

	def error(self, msg):
		self.errors += 1
		self._header()
		print(f'  ERROR  {msg}')

	def warn(self, msg):
		self.warnings += 1
		self._header()
		print(f'  warn   {msg}')

	def info(self, msg):
		if self.verbose:
			self._header()
			print(f'  info   {msg}')

	def end(self):
		if self.verbose and not self.printed_header:
			print(f'== {self.current}: ok')

# ---------------------------------------------------------------------------
# Makefile introspection

def read_make_list(name):
	"""Value of a plain `NAME := a \\ b \\ c` list in the top-level Makefile."""
	with open(os.path.join(ROOT, 'Makefile')) as f:
		text = f.read().replace('\\\n', ' ')
	m = re.search(r'^' + re.escape(name) + r'\s*:?=(.*)$', text, re.M)
	if not m:
		sys.exit(f'check-release: {name} not found in Makefile')
	return m.group(1).split()

def git_tag():
	try:
		return subprocess.check_output(['git', 'describe', '--tags', '--abbrev=0'],
			cwd=ROOT, stderr=subprocess.DEVNULL, text=True).strip()
	except (OSError, subprocess.CalledProcessError):
		return None

def llvm_version():
	"""SP_LLVM_VER (major) from common/utils/llvm-version.mk."""
	path = os.path.join(ROOT, 'common', 'utils', 'llvm-version.mk')
	try:
		with open(path) as f:
			for line in f:
				m = re.match(r'\s*SP_LLVM_VER\s*:?=\s*(\S+)', line)
				if m:
					return m.group(1)
	except OSError:
		pass
	return None

# ---------------------------------------------------------------------------
# Triples

# Architecture of a triple, as one of the names the binary readers below return.
def triple_arch(triple):
	arch = triple.split('-')[0]
	return {
		'x86_64': 'x86_64', 'aarch64': 'aarch64', 'riscv64': 'riscv64',
		'loongarch64': 'loongarch64', 'i686': 'i686', 'armv7a': 'arm',
		'wasm32': 'wasm', 'wasm64': 'wasm',
	}.get(arch)

def triple_os(triple):
	t = triple.split('+')[0]
	if '-windows-' in t: return 'windows'
	if '-apple-' in t: return 'apple'
	if t.startswith('wasm'): return 'wasm'
	if 'android' in t: return 'android'
	if '-xenolithos-' in t: return 'xenolithos'
	if t.endswith('-musl'): return 'linux-musl'
	if t.endswith('-gnu'): return 'linux-gnu'
	return 'unknown'

# Targets of one family share a build template, so they should carry the same
# libraries. The family key is the triple with the architecture taken out.
def target_family(triple):
	if triple == 'unknown-ndk-linux-android':
		return None
	base, _, flavour = triple.partition('+')
	parts = base.split('-')
	if parts[0] == 'armv7a':
		parts = parts[1:]
		parts[-1] = 'android'   # androideabi -> android
	else:
		parts = parts[1:]
	key = '-'.join(parts)
	return key + ('+' + flavour if flavour else '')

# ---------------------------------------------------------------------------
# Binary readers. Each returns (format, arch) or None for a file that is not a
# binary we know; arch is None when the format carries no usable one.

ELF_MACHINES = {3: 'i686', 40: 'arm', 62: 'x86_64', 183: 'aarch64', 243: 'riscv64', 258: 'loongarch64'}
COFF_MACHINES = {0x14c: 'i686', 0x8664: 'x86_64', 0xaa64: 'aarch64', 0xa641: 'aarch64', 0x1c4: 'arm'}
MACHO_CPUS = {7: 'i686', 0x01000007: 'x86_64', 12: 'arm', 0x0100000c: 'aarch64', 0x0200000c: 'aarch64'}

def ident_bytes(data):
	"""Format/arch of an object or image from its first bytes (at least 64)."""
	if data[:4] == b'\x7fELF':
		endian = '<' if data[5] == 1 else '>'
		machine = struct.unpack(endian + 'H', data[18:20])[0]
		return ('elf', ELF_MACHINES.get(machine, f'elf-machine-{machine}'))
	if data[:4] in (b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe'):
		cpu = struct.unpack('<I', data[4:8])[0]
		return ('macho', MACHO_CPUS.get(cpu, f'macho-cpu-{cpu:#x}'))
	if data[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
		# Fat binary - but 0xcafebabe is also a Java class; fat headers have a
		# small slice count.
		n = struct.unpack('>I', data[4:8])[0]
		if 0 < n < 16:
			arches = []
			step = 32 if data[3] == 0xbf else 20
			for i in range(n):
				off = 8 + i * step
				if off + 4 > len(data):
					break
				cpu = struct.unpack('>I', data[off:off + 4])[0]
				arches.append(MACHO_CPUS.get(cpu, f'macho-cpu-{cpu:#x}'))
			return ('macho-fat', tuple(arches))
		return None
	if data[:4] == b'\0asm':
		return ('wasm', 'wasm')
	if data[:2] == b'MZ':
		pe = struct.unpack('<I', data[0x3c:0x40])[0] if len(data) >= 0x40 else 0
		return ('pe', pe)   # resolved by the caller, the header is further in
	if data[:4] in (b'BC\xc0\xde', b'\xde\xc0\x17\x0b'):
		return ('bitcode', None)
	if data[:4] == b'\0\0\xff\xff' and len(data) >= 8:
		# COFF short import member (import libraries)
		machine = struct.unpack('<H', data[6:8])[0]
		return ('coff-import', COFF_MACHINES.get(machine, f'coff-{machine:#x}'))
	if len(data) >= 20:
		machine = struct.unpack('<H', data[0:2])[0]
		if machine in COFF_MACHINES:
			return ('coff', COFF_MACHINES[machine])
		if data[:4] == b'\0\0\xff\xff':
			return None
	return None

def ident_file(path):
	try:
		with open(path, 'rb') as f:
			head = f.read(4096)
			if head[:8] == b'!<arch>\n':
				return ident_archive(f)
			r = ident_bytes(head)
			if r and r[0] == 'pe':
				off = r[1]
				f.seek(off)
				hdr = f.read(6)
				if hdr[:4] != b'PE\0\0':
					return None
				machine = struct.unpack('<H', hdr[4:6])[0]
				return ('pe', COFF_MACHINES.get(machine, f'coff-{machine:#x}'))
			return r
	except OSError:
		return None

AR_SPECIAL = {b'/', b'//', b'/SYM64/', b'__.SYMDEF', b'__.SYMDEF SORTED', b'/<ECSYMBOLS>/'}

def ident_archive(f):
	"""Arch of a static/import library by its first recognisable member."""
	pos = 8
	f.seek(0, os.SEEK_END)
	end = f.tell()
	seen_bitcode = False
	for _ in range(64):
		if pos + 60 > end:
			break
		f.seek(pos)
		hdr = f.read(60)
		name = hdr[:16].rstrip(b' ')
		try:
			size = int(hdr[48:58].strip() or b'0')
		except ValueError:
			return None
		data_pos = pos + 60
		# BSD long names: "#1/<len>", the name precedes the data
		skip = 0
		if name.startswith(b'#1/'):
			skip = int(name[3:])
			f.seek(data_pos)
			name = f.read(skip).rstrip(b'\0')
		if name not in AR_SPECIAL:
			f.seek(data_pos + skip)
			r = ident_bytes(f.read(64))
			if r and r[0] == 'bitcode':
				seen_bitcode = True
			elif r:
				return r
		pos = data_pos + size + (size & 1)
	return ('bitcode', None) if seen_bitcode else ('archive', None)

def arch_matches(found, expected):
	if expected is None or found is None:
		return True
	if isinstance(found, tuple):    # fat Mach-O: the expected slice must be in it
		return expected in found
	return found == expected

# ---------------------------------------------------------------------------
# Run-time dependencies of host binaries

ELF_SYSTEM_LIBS = re.compile(r'^(libc|libm|libdl|libpthread|librt|libutil|libresolv|libanl|'
	r'ld-linux[-\w.]*|ld-musl-\w+|libc\.musl-\w+|ld64)\.so(\.\d+)*$|^ld-linux[-\w.]*\.so(\.\d+)*$')

def elf_needed(path):
	"""DT_NEEDED entries of an ELF image (64- and 32-bit, little-endian)."""
	with open(path, 'rb') as f:
		data = f.read()
	if data[:4] != b'\x7fELF' or data[5] != 1:
		return []
	is64 = data[4] == 2
	if is64:
		phoff, = struct.unpack_from('<Q', data, 0x20)
		phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
	else:
		phoff, = struct.unpack_from('<I', data, 0x1c)
		phentsize, phnum = struct.unpack_from('<HH', data, 0x2a)
	loads = []
	dyn = None
	for i in range(phnum):
		off = phoff + i * phentsize
		if is64:
			p_type, _, p_offset, p_vaddr, _, p_filesz = struct.unpack_from('<IIQQQQ', data, off)
		else:
			p_type, p_offset, p_vaddr, _, p_filesz = struct.unpack_from('<IIIII', data, off)
		if p_type == 1:
			loads.append((p_vaddr, p_offset, p_filesz))
		elif p_type == 2:
			dyn = (p_offset, p_filesz)
	if not dyn:
		return []
	def v2o(addr):
		for vaddr, offset, size in loads:
			if vaddr <= addr < vaddr + size:
				return addr - vaddr + offset
		return None
	entries = []
	strtab = None
	ent = 16 if is64 else 8
	fmt = '<qQ' if is64 else '<iI'
	for i in range(dyn[1] // ent):
		tag, val = struct.unpack_from(fmt, data, dyn[0] + i * ent)
		if tag == 0:
			break
		if tag == 1:
			entries.append(val)
		elif tag == 5:
			strtab = v2o(val)
	if strtab is None:
		return []
	out = []
	for e in entries:
		s = strtab + e
		out.append(data[s:data.index(b'\0', s)].decode())
	return out

def macho_dylibs(path):
	"""LC_LOAD_DYLIB / LC_LOAD_WEAK_DYLIB / LC_REEXPORT_DYLIB of a thin 64-bit Mach-O."""
	with open(path, 'rb') as f:
		data = f.read()
	if data[:4] != b'\xcf\xfa\xed\xfe':
		return []
	ncmds, = struct.unpack_from('<I', data, 16)
	off = 32
	out = []
	for _ in range(ncmds):
		cmd, size = struct.unpack_from('<II', data, off)
		if cmd in (0xc, 0x80000018, 0x8000001f):
			name_off, = struct.unpack_from('<I', data, off + 8)
			s = off + name_off
			out.append(data[s:data.index(b'\0', s)].decode())
		off += size
	return out

def pe_imports(path):
	"""Imported DLL names of a PE image."""
	with open(path, 'rb') as f:
		data = f.read()
	pe, = struct.unpack_from('<I', data, 0x3c)
	nsec, = struct.unpack_from('<H', data, pe + 6)
	optsize, = struct.unpack_from('<H', data, pe + 20)
	opt = pe + 24
	magic, = struct.unpack_from('<H', data, opt)
	ddir = opt + (112 if magic == 0x20b else 96)
	imp_rva, _ = struct.unpack_from('<II', data, ddir + 8)
	secs = []
	for i in range(nsec):
		s = opt + optsize + i * 40
		vsize, vaddr, rsize, raddr = struct.unpack_from('<IIII', data, s + 8)
		secs.append((vaddr, max(vsize, rsize), raddr))
	def r2o(rva):
		for vaddr, size, raddr in secs:
			if vaddr <= rva < vaddr + size:
				return rva - vaddr + raddr
		return None
	out = []
	if not imp_rva:
		return out
	off = r2o(imp_rva)
	while off is not None and off + 20 <= len(data):
		name_rva = struct.unpack_from('<I', data, off + 12)[0]
		if name_rva == 0:
			break
		n = r2o(name_rva)
		out.append(data[n:data.index(b'\0', n)].decode())
		off += 20
	return out

# A host must run on a clean machine of its OS. These are what that machine has.
PE_SYSTEM_DLLS = {
	'kernel32.dll', 'ntdll.dll', 'user32.dll', 'gdi32.dll', 'advapi32.dll', 'shell32.dll',
	'ole32.dll', 'oleaut32.dll', 'ws2_32.dll', 'bcrypt.dll', 'crypt32.dll', 'version.dll',
	'shlwapi.dll', 'psapi.dll', 'dbghelp.dll', 'rpcrt4.dll', 'secur32.dll', 'iphlpapi.dll',
	'userenv.dll', 'winmm.dll', 'imm32.dll', 'comdlg32.dll', 'comctl32.dll', 'mswsock.dll',
	'ncrypt.dll', 'setupapi.dll', 'cfgmgr32.dll', 'powrprof.dll', 'dnsapi.dll', 'netapi32.dll',
	'wldap32.dll', 'normaliz.dll', 'bcryptprimitives.dll', 'diasymreader.dll', 'synchronization.dll',
	'kernelbase.dll', 'mpr.dll', 'winhttp.dll', 'wininet.dll', 'ntdsapi.dll', 'dwmapi.dll',
	'combase.dll', 'shcore.dll', 'uxtheme.dll', 'dxva2.dll',
}

def pe_is_system(name):
	n = name.lower()
	return n in PE_SYSTEM_DLLS or n.startswith('api-ms-win-core-') or n.startswith('ext-ms-')

def macho_is_system(name):
	return name.startswith('/usr/lib/') or name.startswith('/System/Library/')

# ---------------------------------------------------------------------------
# Package walking

LIB_RE = re.compile(r'\.(a|lib|so|dll|dylib|tbd)$|\.so\.\d+(\.\d+)*$')
BIN_EXT_SKIP = re.compile(r'\.(h|hpp|hh|inc|def|txt|md|mk|cmake|ini|py|pc|la|json|modulemap|'
	r'map|ld|lds|spec|cfg|s|S|c|cc|cpp|in|sh|tcl|pl|xml|html|css|js|mjs|syms|ver|tbd|order|'
	r'pem|der|gz|xz|bz2|zip|png|svg)$', re.I)

def walk(root):
	"""(relpath, abspath, is_link) of every entry under root, not following links."""
	for dirpath, dirnames, filenames in os.walk(root):
		for name in dirnames + filenames:
			p = os.path.join(dirpath, name)
			yield os.path.relpath(p, root), p, os.path.islink(p)

def check_links(rep, pkg_dir, allow_host_link):
	"""Symlinks must be relative and resolve inside the package. Targets point
	lib/clang/include into `host/`, which the installer creates next to them.
	runtime/rootfs is a device filesystem image: absolute links there are meant to
	resolve on the device."""
	for rel, p, is_link in walk(pkg_dir):
		if not is_link:
			continue
		dest = os.readlink(p)
		if rel.startswith(os.path.join('runtime', 'rootfs') + os.sep):
			continue
		if os.path.isabs(dest):
			rep.error(f'absolute symlink {rel} -> {dest}')
			continue
		resolved = os.path.normpath(os.path.join(os.path.dirname(p), dest))
		inside = os.path.relpath(resolved, pkg_dir)
		if inside.startswith('..'):
			rep.error(f'symlink leaves the package: {rel} -> {dest}')
			continue
		if not os.path.exists(p):
			if allow_host_link and (inside == 'host' or inside.startswith('host' + os.sep)):
				continue
			rep.error(f'dangling symlink {rel} -> {dest}')

def check_nonempty(rep, pkg_dir, rel, what='file'):
	p = os.path.join(pkg_dir, rel)
	if not os.path.lexists(p):
		rep.error(f'missing {what} {rel}')
		return False
	if os.path.isfile(p) and os.path.getsize(p) == 0 and what == 'file':
		rep.error(f'empty {rel}')
		return False
	return True

def check_arches(rep, pkg_dir, expected, subdirs, skip=lambda rel: False, accept=None):
	"""Every recognisable binary under subdirs must be for `expected` (or one of
	`accept`). Returns the number checked."""
	checked = 0
	wrong = {}
	for sub in subdirs:
		base = os.path.join(pkg_dir, sub)
		if not os.path.isdir(base):
			continue
		for rel, p, is_link in walk(base):
			rel = os.path.join(sub, rel)
			if is_link or not os.path.isfile(p) or BIN_EXT_SKIP.search(p) or skip(rel):
				continue
			r = ident_file(p)
			if not r or r[1] is None:
				continue
			checked += 1
			arch = r[1]
			ok = arch_matches(arch, expected) if accept is None else \
				any(arch_matches(arch, a) for a in accept)
			if not ok:
				wrong.setdefault(str(arch), []).append(rel)
	for arch, files in sorted(wrong.items()):
		sample = ', '.join(sorted(files)[:4]) + (f' (+{len(files) - 4} more)' if len(files) > 4 else '')
		rep.error(f'{len(files)} binaries built for {arch}, expected {expected or accept}: {sample}')
	return checked

# ---------------------------------------------------------------------------
# Hosts

HOST_TOOLS_POSIX = [
	'clang-{V}', 'clang', 'clang++', 'cc', 'c++', 'ar', 'clang-cl', 'clang-cpp',
	'lld', 'ld.lld', 'ld64.lld', 'lld-link', 'wasm-ld',
	'lldb', 'lldb-server', 'lldb-argdumper',
	'llvm-ar', 'llvm-ranlib', 'llvm-lib', 'llvm-dlltool', 'llvm-nm', 'llvm-objcopy', 'llvm-strip',
	'llvm-objdump', 'llvm-readobj', 'llvm-readelf', 'llvm-readtapi', 'llvm-rc', 'llvm-windres',
	'llvm-ml', 'llvm-symbolizer', 'llvm-cov', 'llvm-cxxfilt', 'llvm-dwp', 'llvm-size', 'llvm-strings',
	'llvm-install-name-tool', 'llvm-bitcode-strip', 'llvm-otool', 'llvm-addr2line',
	'glslang', 'glslangValidator',
	'spirv-as', 'spirv-cfg', 'spirv-diff', 'spirv-dis', 'spirv-link', 'spirv-lint',
	'spirv-objdump', 'spirv-opt', 'spirv-reduce', 'spirv-val',
	'make', 'xlmake',
]

# Natively built hosts (x86_64 linux) and the macOS ones carry the full tool set.
HOST_TOOLS_FULL = HOST_TOOLS_POSIX + [
	'clang-format', 'clang-installapi', 'diagtool', 'lldb-dap', 'lldb-instr',
	'llvm-ifs', 'llvm-libtool-darwin', 'llvm-link', 'llvm-lipo', 'llvm-mt', 'llvm-pdbutil',
]

HOST_TOOLS_WINDOWS = [
	'clang.exe', 'clang++.exe', 'clang-cl.exe', 'clang-cpp.exe',
	'lld.exe', 'lld-link.exe', 'ld.lld.exe', 'ld64.lld.exe', 'wasm-ld.exe',
	'llvm-ar.exe', 'llvm-lib.exe', 'llvm-ranlib.exe', 'llvm-dlltool.exe',
	'llvm-objcopy.exe', 'llvm-strip.exe', 'llvm-rc.exe',
	'lldb.exe', 'lldb-server.exe', 'lldb-dap.exe', 'liblldb.dll', 'LTO.dll',
	'glslang.exe', 'spirv-link.exe', 'sprt.dll', 'xlmake.exe',
]

HOST_LIBS_LINUX = ['libc++.so.1', 'libc++abi.so.1', 'libunwind.so.1', 'libLLVM.so', 'libLTO.so',
	'libRemarks.so', 'liblldb.so']
HOST_LIBS_MACOS = ['libLLVM.dylib', 'libLTO.dylib', 'libRemarks.dylib', 'libclang.dylib',
	'libclang-cpp.dylib', 'libSPIRV-Tools.dylib', 'libSPIRV-Tools-opt.dylib']

HOST_TOP_ALLOWED = {'bin', 'lib', 'include', 'share', 'host.mk', 'release'}

def check_host(rep, host, tag, llvm_ver, native_host):
	d = os.path.join(ROOT, 'hosts', host)
	osname = triple_os(host)
	arch = triple_arch(host)

	if not os.path.isdir(d):
		rep.error('not built')
		return
	check_release_stamp(rep, d, tag)
	check_nonempty(rep, d, 'host.mk')

	for e in sorted(os.listdir(d)):
		if e not in HOST_TOP_ALLOWED:
			rep.error(f'stray entry at top level: {e}' +
				(' (looks like a target sysroot)' if os.path.exists(os.path.join(d, e, 'target.mk')) else ''))

	if osname == 'windows':
		tools = HOST_TOOLS_WINDOWS
		libs = []
	else:
		full = osname == 'apple' or host == native_host
		tools = [t.replace('{V}', llvm_ver or '') for t in (HOST_TOOLS_FULL if full else HOST_TOOLS_POSIX)]
		libs = HOST_LIBS_MACOS if osname == 'apple' else HOST_LIBS_LINUX
	check_nonempty(rep, d, 'share/licenses', 'directory')
	for t in tools:
		p = os.path.join(d, 'bin', t)
		if not os.path.exists(p):
			rep.error(f'missing tool bin/{t}' + (' (dangling link)' if os.path.lexists(p) else ''))
		elif os.path.getsize(p) == 0:
			rep.error(f'empty tool bin/{t}')
	for l in libs:
		if not os.path.exists(os.path.join(d, 'lib', l)):
			rep.error(f'missing library lib/{l}')

	# clang resource headers: targets link lib/clang/include into the host's copy
	if llvm_ver:
		inc = os.path.join('lib', 'clang', llvm_ver, 'include')
		if not os.path.isfile(os.path.join(d, inc, 'stddef.h')):
			rep.error(f'missing clang resource headers {inc}')

	n = check_arches(rep, d, arch, ['bin', 'lib'], skip=lambda rel: rel.startswith('lib/clang/'))
	rep.info(f'{n} binaries checked for {arch}')
	check_links(rep, d, allow_host_link=False)
	check_host_deps(rep, d, osname)

def check_host_deps(rep, d, osname):
	"""Every library a host binary loads must be in the package or in the OS."""
	bindir = os.path.join(d, 'bin')
	libdir = os.path.join(d, 'lib')
	shipped = set()
	for sub in (bindir, libdir):
		if os.path.isdir(sub):
			shipped.update(e.lower() if osname == 'windows' else e for e in os.listdir(sub))
	bad = {}
	for sub in (bindir, libdir):
		if not os.path.isdir(sub):
			continue
		for e in os.listdir(sub):
			p = os.path.join(sub, e)
			if os.path.islink(p) or not os.path.isfile(p):
				continue
			r = ident_file(p)
			if not r:
				continue
			try:
				if r[0] == 'elf':
					for n in elf_needed(p):
						if n not in shipped and not ELF_SYSTEM_LIBS.match(n):
							bad.setdefault(n, []).append(e)
				elif r[0] == 'macho':
					for n in macho_dylibs(p):
						if macho_is_system(n):
							continue
						base = os.path.basename(n)
						if not (n.startswith('@') and base in shipped):
							bad.setdefault(n, []).append(e)
				elif r[0] == 'pe':
					for n in pe_imports(p):
						if n.lower() not in shipped and not pe_is_system(n):
							bad.setdefault(n, []).append(e)
			except (struct.error, ValueError, IndexError) as ex:
				rep.warn(f'cannot read dependencies of {os.path.relpath(p, d)}: {ex}')
	for lib, users in sorted(bad.items()):
		rep.error(f'needs {lib}, which is neither shipped nor part of the OS (used by {", ".join(sorted(users)[:4])})')

# ---------------------------------------------------------------------------
# Targets

# Third-party libraries every published target has to carry, by base name.
TARGET_CORE_LIBS = [
	'brotlicommon', 'brotlidec', 'brotlienc', 'bz2', 'crypto', 'ssl', 'curl-openssl',
	'freetype', 'gif', 'gost_engine', 'harfbuzz', 'harfbuzz-gpu', 'harfbuzz-raster',
	'harfbuzz-subset', 'harfbuzz-vector', 'jpeg', 'lzma', 'nghttp3', 'ngtcp2',
	'ngtcp2_crypto_ossl', 'png16', 'sharpyuv', 'SheenBidi', 'sqlite3', 'tiff',
	'webp', 'webpdecoder', 'webpdemux', 'webpmux', 'z', 'zstd',
]

# ...and on top of that, per OS
TARGET_OS_LIBS = {
	'linux-gnu': ['backtrace', 'c++abi', 'unwind', 'xml2', 'expat', 'ffi', 'drm',
		'wayland-client', 'iwasm-release', 'c', 'm'],
	'linux-musl': ['backtrace', 'c++abi', 'unwind', 'xml2', 'expat', 'ffi', 'drm',
		'wayland-client', 'iwasm-release', 'c', 'm'],
	'xenolithos': ['backtrace', 'c++abi', 'unwind', 'xml2', 'expat', 'ffi', 'drm',
		'wayland-client', 'iwasm-release', 'c', 'm', 'vulkan'],
	'android': ['backtrace', 'c++abi', 'unwind', 'iwasm-release', 'cpufeatures-webp',
		'c', 'm', 'log', 'android'],
	'apple': ['backtrace', 'xml2', 'iwasm-release', 'VulkanLayerSettings'],
	'windows': [],
	'wasm': ['c++abi', 'unwind', 'm', 'sprt'],
}

# Libraries whose presence legitimately differs between sibling architectures:
# compiler-rt builds each sanitizer, the ORC runtime and the DataFlow runtimes only
# where they are supported, glibc ships libmvec only for x86_64/aarch64, and the
# dynamic loader has a different name on every architecture.
ARCH_OPTIONAL = re.compile(r'(^|/)ld-linux[-\w.@]*\.so|(^|/)libmvec\.so|orc_rt|'
	r'libclang_rt\.(dd|dyndd|cc_kext\w*)[-.]|darwin/libclang_rt\.ios\.a$|'
	r'libclang_rt\.(asan|hwasan|lsan|msan|tsan|ubsan|dfsan|cfi|gwp_asan|'
	r'memprof|nsan|rtsan|tysan|safestack|scudo|xray|ctx_profile|fuzzer|orc|stats|radsan|'
	r'profile|builtins_ctx|asan_static|asan_cxx|asan-preinit|hwasan_aliases|ubsan_minimal|'
	r'ubsan_standalone|ubsan_standalone_cxx|hwasan_cxx|hwasan-preinit|cfi_diag|'
	r'msan_cxx|tsan_cxx|xray-[\w-]+|fuzzer_\w+|nsan|ubsan_minimal_\w*)')

ARCH_TOKENS = ['loongarch64', 'riscv64', 'aarch64', 'x86_64', 'armv7a', 'armv7', 'armhf',
	'arm64', 'i686', 'i386', 'wasm32', 'wasm64', 'arm', 'x64', 'x86']

# Libraries a target is known not to have, with the reason. Applies both to the
# required list and to the comparison with sibling targets.
EXPECTED_GAPS = [
	# target-linux/Makefile: WAMR has no LoongArch backend (no WAMR_BUILD_TARGET,
	# no 64-bit invokeNative), so iwasm is not built there.
	(r'^loongarch64-', r'libiwasm-(release|debug)\.a$', 'WAMR does not support LoongArch'),
	# LoongArch entered glibc after 2.34 merged libanl/libutil into libc, so the
	# port has no compatibility stubs for them.
	(r'^loongarch64-.*-gnu$', r'(^|/)lib(anl|util)\.so', 'glibc has no libanl/libutil stubs on LoongArch'),
]

def expected_gap(target, rel):
	for tre, pre, _ in EXPECTED_GAPS:
		if re.search(tre, target) and re.search(pre, rel):
			return True
	return False

def normalise(rel, triple):
	rel = rel.replace(triple.split('+')[0], '@TRIPLE@')
	for tok in ARCH_TOKENS:
		rel = re.sub(r'(?<![A-Za-z0-9])' + tok + r'(?![A-Za-z0-9])', '@ARCH@', rel)
	return rel

def lib_basename(fname):
	"""libfoo.a / foo.lib / libfoo.so.1 / libfoo.tbd -> foo"""
	n = re.sub(r'\.so(\.\d+)*$', '', fname)
	n = re.sub(r'\.(a|lib|tbd|dylib|dll)$', '', n)
	if n.startswith('lib') and not fname.endswith('.lib'):
		n = n[3:]
	return n

def target_libs(d):
	out = {}
	for rel, p, is_link in walk(d):
		if LIB_RE.search(rel) and (os.path.isfile(p) or is_link):
			out[rel] = p
	return out

TARGET_TOP_ALLOWED = {'include_libc', 'lib', 'usr', 'share', 'System', 'runtime',
	'target.mk', 'target.ini', 'toolchain.cmake', 'release'}

def check_target(rep, target, tag, families):
	d = os.path.join(ROOT, 'targets', target)
	osname = triple_os(target)
	if not os.path.isdir(d):
		rep.error('not built')
		return
	check_release_stamp(rep, d, tag)
	check_nonempty(rep, d, 'target.mk')
	check_nonempty(rep, d, 'share/licenses', 'directory')
	for e in sorted(os.listdir(d)):
		if e not in TARGET_TOP_ALLOWED:
			rep.error(f'stray entry at top level: {e}')

	libs = target_libs(d)
	names = {}
	for rel in libs:
		names.setdefault(lib_basename(os.path.basename(rel)), []).append(rel)

	required = TARGET_CORE_LIBS + TARGET_OS_LIBS.get(osname, [])
	if target == 'unknown-ndk-linux-android':
		required = TARGET_CORE_LIBS + ['backtrace', 'cpufeatures-webp', 'iwasm-release']
		# one copy per ABI
		for n in required:
			got = len(names.get(n, []))
			if got == 0:
				rep.error(f'missing library {n}')
			elif got < 4:
				rep.error(f'library {n} present for {got} of 4 ABIs: {", ".join(sorted(names[n]))}')
	else:
		for n in required:
			if n not in names and not expected_gap(target, f'lib{n}.a'):
				rep.error(f'missing library {n}')

	# The compiler-rt builtins are what every link needs.
	if osname not in ('android',) and target != 'unknown-ndk-linux-android':
		if not any('clang_rt.builtins' in r or 'clang_rt.osx' in r or 'clang_rt.ios' in r
				for r in libs):
			rep.error('missing compiler-rt builtins (lib/clang/.../libclang_rt.builtins*)')

	if target == 'unknown-ndk-linux-android':
		accept = ['aarch64', 'arm', 'i686', 'x86_64']
		n = check_arches(rep, d, None, ['usr', 'lib'], accept=accept)
	else:
		arch = triple_arch(target)
		n = check_arches(rep, d, arch, ['lib', 'usr', 'runtime', 'System'],
			skip=lambda rel: rel.startswith('usr/share/'))
	rep.info(f'{n} binaries checked')
	check_links(rep, d, allow_host_link=True)

	fam = target_family(target)
	if fam:
		families.setdefault(fam, {})[target] = {normalise(r, target) for r in libs}

def check_families(rep, families):
	for fam, members in sorted(families.items()):
		if len(members) < 2:
			continue
		rep.begin(f'family *-{fam} ({", ".join(sorted(members))})')
		union = set().union(*members.values())
		for t, s in sorted(members.items()):
			missing = sorted(m for m in union - s if not expected_gap(t, m))
			hard = [m for m in missing if not ARCH_OPTIONAL.search(m)]
			soft = len(missing) - len(hard)
			if hard:
				sample = ', '.join(hard[:12]) + (f' (+{len(hard) - 12} more)' if len(hard) > 12 else '')
				rep.error(f'{t} lacks {len(hard)} libraries its siblings have: {sample}')
			if soft:
				rep.info(f'{t} lacks {soft} sanitizer/profiling runtimes (arch-dependent)')
		rep.end()

# ---------------------------------------------------------------------------
# Common

def check_release_stamp(rep, d, tag):
	p = os.path.join(d, 'release')
	if not os.path.isfile(p):
		rep.error('no release stamp')
		return
	with open(p, encoding='utf-8-sig') as f:
		got = f.read().strip()
	if tag and got != tag:
		rep.error(f'release stamp says {got!r}, expected {tag!r}')

def check_archives(rep, kind, names, tag, verify_sig):
	gpg = shutil.which('gpg')
	for n in names:
		rep.begin(f'release/{kind}_{n}.tar.xz')
		pkg = os.path.join(ROOT, kind + 's', n)
		arc = os.path.join(ROOT, 'release', f'{kind}_{n}.tar.xz')
		sig = arc + '.sig'
		if not os.path.isfile(arc):
			rep.error('archive not staged (make release-export)')
		else:
			stamp = os.path.join(pkg, 'release')
			if os.path.exists(stamp) and os.path.getmtime(arc) < os.path.getmtime(stamp):
				rep.error('archive is older than the package release stamp')
			if not os.path.isfile(sig):
				rep.error('signature missing')
			elif os.path.getmtime(sig) < os.path.getmtime(arc):
				rep.error('signature is older than the archive')
			elif verify_sig and gpg:
				r = subprocess.run([gpg, '--verify', sig, arc], capture_output=True)
				if r.returncode != 0:
					rep.error('bad signature: ' + r.stderr.decode(errors='replace').strip().splitlines()[-1])
		rep.end()

def main():
	ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
	ap.add_argument('--tag', help='expected release tag (default: git describe --tags --abbrev=0)')
	ap.add_argument('--archives', action='store_true', help='also check the staged release/ archives')
	ap.add_argument('--no-verify-sig', action='store_true', help='with --archives, do not run gpg --verify')
	ap.add_argument('--strict', action='store_true', help='warnings fail the run too')
	ap.add_argument('-v', '--verbose', action='store_true', help='also print what passed')
	ap.add_argument('packages', nargs='*', help='limit to these hosts/<id> or targets/<id>')
	args = ap.parse_args()

	tag = args.tag or git_tag()
	llvm_ver = llvm_version()
	hosts = read_make_list('RELEASE_HOSTS')
	targets = read_make_list('RELEASE_TARGETS')
	if args.packages:
		want = {p.rstrip('/') for p in args.packages}
		hosts = [h for h in hosts if f'hosts/{h}' in want]
		targets = [t for t in targets if f'targets/{t}' in want]
		unknown = want - {f'hosts/{h}' for h in hosts} - {f'targets/{t}' for t in targets}
		if unknown:
			sys.exit('not in the release set: ' + ', '.join(sorted(unknown)))

	# The natively built host carries a few tools the cross-built ones do not.
	native_host = 'x86_64-unknown-linux-gnu'

	rep = Report(args.verbose)
	print(f'release tag: {tag or "(none)"}; LLVM {llvm_ver or "?"}; '
		f'{len(hosts)} hosts, {len(targets)} targets')
	if not tag:
		rep.begin('release tag')
		rep.error('cannot determine the release tag, pass --tag')

	for h in hosts:
		rep.begin(f'hosts/{h}')
		check_host(rep, h, tag, llvm_ver, native_host)
		rep.end()

	families = {}
	for t in targets:
		rep.begin(f'targets/{t}')
		check_target(rep, t, tag, families)
		rep.end()
	check_families(rep, families)

	if args.archives:
		check_archives(rep, 'host', hosts, tag, not args.no_verify_sig)
		check_archives(rep, 'target', targets, tag, not args.no_verify_sig)

	print(f'\n{rep.errors} error(s), {rep.warnings} warning(s)')
	if rep.errors or (args.strict and rep.warnings):
		return 1
	return 0

if __name__ == '__main__':
	sys.exit(main())
