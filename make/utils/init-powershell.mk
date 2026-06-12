# Copyright (c) 2026 Xenloith Team <admin@xenolith.studio>
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

$(call print_verbose,(init-powershell.mk) Init with powershell)

" := ", [char]0x22, "

UNAME := Windows
SHELL = powershell.exe

POWERSHELL := 1

GLOBAL_SHELL := powershell

GLOBAL_MAKE ?= $(MAKE)
GLOBAL_MKDIR ?= powershell New-Item -ItemType Directory -Force -Path
GLOBAL_AR ?= ar rcs
GLOBAL_ECHO ?= Write-Host

shell_mkdir = $(call print_verbose,Powershell (mkdir): $(shell New-Item -Path "$(1)" -ItemType Directory -Force | Out-Null))

rule_rm = powershell 'if (Test-Path "$(1)") { Remove-Item -Recurse -Force -ErrorAction Ignore -Path "$(1)" }'
rule_cp = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force
rule_mkdir = powershell New-Item -ItemType Directory -Force -Path $(1) | Out-Null

shell_override_file = \
	$(call print_verbose,Powershell (override): $(shell Set-Content "$(strip $(1))" '$(strip $(2))') )

shell_append_file = \
	$(call print_verbose,Powershell (append): $(shell Add-Content "$(1)" '$(2)') )

shell_cat = \
	$(shell if (Test-Path -Path "$(1)" -PathType Leaf) { Get-Content "$(1)" })

shell_arith = $(shell powershell $(1))

STAPPLER_HOST_ARCH ?= $(shell [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture)

ifeq ($(STAPPLER_HOST_ARCH),X64)
STAPPLER_HOST_ARCH := x86_64
else ifeq ($(STAPPLER_HOST_ARCH),X86)
STAPPLER_HOST_ARCH := x86
else ifeq ($(STAPPLER_HOST_ARCH),Arm64)
STAPPLER_HOST_ARCH := aarch64
else ifeq ($(STAPPLER_HOST_ARCH),Arm)
STAPPLER_HOST_ARCH := arm
endif

ANDROID_HOST := windows-$(STAPPLER_HOST_ARCH)

STAPPLER_HOST := $(STAPPLER_HOST_ARCH)-pc-windows-msvc

$(call print_verbose,(init-powershell.mk) UNAME: $(UNAME))
$(call print_verbose,(init-powershell.mk) STAPPLER_HOST_ARCH: $(STAPPLER_HOST_ARCH))
$(call print_verbose,(init-powershell.mk) STAPPLER_HOST: $(STAPPLER_HOST))
$(call print_verbose,(init-powershell.mk) ANDROID_HOST: $(ANDROID_HOST))

