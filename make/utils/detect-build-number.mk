# Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

sp_detect_git = $(shell git -C $(1) rev-parse --git-dir 2> /dev/null)
sp_detect_build_write_rev = $(firstword $(2) $(call shell_override_file, $(1)/.build_number,$(2)))

sp_detect_build_number_git = $(shell git -C $(1) rev-list --count HEAD)
sp_detect_build_number_file = $(call shell_cat,$(1)/.build_number)

sp_detect_build_number = $(if $(call sp_detect_git,$(1)),\
	$(call sp_detect_build_write_rev,$(1),$(call sp_detect_build_number_git,$(1))),\
	$(call sp_detect_build_number_file,$(1)))
