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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_WINAPIFAMILY_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_WINAPIFAMILY_H_


#define __SPRT_WINAPI_FAMILY_PC_APP               2
#define __SPRT_WINAPI_FAMILY_PHONE_APP            3
#define __SPRT_WINAPI_FAMILY_SYSTEM               4
#define __SPRT_WINAPI_FAMILY_SERVER               5
#define __SPRT_WINAPI_FAMILY_GAMES                6
#define __SPRT_WINAPI_FAMILY_DESKTOP_APP          100

#define __SPRT_WINAPI_FAMILY __SPRT_WINAPI_FAMILY_DESKTOP_APP

#define __SPRT_WINAPI_FAMILY_PARTITION(Partitions)     (Partitions)

#define __SPRT_WINAPI_PARTITION_DESKTOP (__SPRT_WINAPI_FAMILY == __SPRT_WINAPI_FAMILY_DESKTOP_APP)


#endif // SPRT_WRAPPERS_WINDOWS_ABI_WINAPIFAMILY_H_
