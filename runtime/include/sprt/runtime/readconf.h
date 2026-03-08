/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_READCONF_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_READCONF_H_

#include <sprt/runtime/callback.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/enum.h>

namespace sprt {

/* Reader for INI-style config files like:

[Section]
Key1=Value1
Key2 = Value2
  Key3 = Value3

KeyWithNoValue1 = 
KeyWithNoValue2

When value is read, cb will be called with (<section_name>,<value_name>,<value_content>);
Section name sent with square braces ("[Section]")
For key-value pair before any section <section_name> will be empty
Spaces are stripped from keys and values
User should return false to stop reading, and true to continue reading

data should contains full config file as UTF8 string

Returns:
	Status::Ok on syccess
	Status::Declined when user stops reading
	Status::ErrorInvalidArguemnt on syntax error
*/

enum class ConfFeatures : uint32_t {
	None,

	// Do not stop on recoverable errors
	ContinueOnError = 1 << 0,
};

SPRT_DEFINE_ENUM_AS_MASK(ConfFeatures);

Status readconf(const Callback<bool(StringView, StringView, StringView)> &cb, StringView data,
		ConfFeatures = ConfFeatures::ContinueOnError);

} // namespace sprt

#endif
