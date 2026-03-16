# Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
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

ifeq ($(findstring Windows,$(OS)),Windows)

SHELL = powershell.exe
MKDIR = powershell New-Item -ItemType Directory -Force -Path
RM := powershell Remove-Item -Recurse -Force -ErrorAction Ignore -Path 

rule_mkdir = powershell New-Item -ItemType Directory -Force -Path "$(1)" | Out-Null
rule_rm = powershell 'if (Test-Path "$(1)") { Remove-Item -Recurse -Force -ErrorAction Ignore -Path "$(1)" }'
rule_cp = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force
rule_cpr = powershell Copy-Item -Path "$(1)" -Destination "$(2)" -Force -Recurse
rule_mv = powershell Move-Item -Path "$(1)" -Destination "$(2)" -Force

mklibname = $(addprefix lib,$(addsuffix .lib, $(1)))

else

MKDIR = mkdir -p
RM := rm -rf

rule_mkdir = mkdir -p $(1)
rule_rm = rm -rf $(1)
rule_cp = cp -f $(1) $(2)
rule_cpr = cp -rf $(1) $(2)
rule_mv = mv -f $(1) $(2)

mklibname = $(addprefix lib,$(addsuffix .a, $(1)))

endif
