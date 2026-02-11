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

$(call print_verbose,(fn-powershell.mk) Using powershell)

POWERSHELL := 1

GLOBAL_SHELL := powershell

GLOBAL_RM ?= rm -f
GLOBAL_CP ?= cp
GLOBAL_MAKE ?= make
GLOBAL_MKDIR ?= powershell New-Item -ItemType Directory -Force -Path
GLOBAL_AR ?= ar rcs

shell_mkdir = $(call print_verbose,Powershell (mkdir): $(shell New-Item -Path "$(1)" -ItemType Directory -Force | Out-Null))

rule_mkdir = powershell New-Item -ItemType Directory -Force -Path $(1) | Out-Null

shell_override_file = \
	$(call print_verbose,Powershell (override): $(shell Set-Content "$(1)" '$(2)') )

shell_append_file = \
	$(call print_verbose,Powershell (append): $(shell Add-Content "$(1)" '$(2)') )

shell_cat = \
	$(shell if (Test-Path -Path "$(1)" -PathType Leaf) { Get-Content "$(1)" })

shell_arith = $(shell powershell $(1))
