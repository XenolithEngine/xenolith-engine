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

#ifndef SPRT_WRAPPERS_WINDOWS_SHELLAPI_H_
#define SPRT_WRAPPERS_WINDOWS_SHELLAPI_H_

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/abi/shellapi.h>

// FILEOP_FLAGS (SHFILEOPSTRUCT / IFileOperation::SetOperationFlags). Only the bits
// llvm's Path.inc combines into a no-UI recycle delete are defined; FOF_NO_UI is the
// SDK's convenience aggregate, FOFX_NOCOPYHOOKS is a shobjidl extended flag.
typedef unsigned int FILEOP_FLAGS;

#define FOF_SILENT __SPRT_FOF_SILENT
#define FOF_NOCONFIRMATION __SPRT_FOF_NOCONFIRMATION
#define FOF_NOCONFIRMMKDIR __SPRT_FOF_NOCONFIRMMKDIR
#define FOF_NOERRORUI __SPRT_FOF_NOERRORUI
#define FOF_NO_UI __SPRT_FOF_NO_UI

#define FOFX_NOCOPYHOOKS __SPRT_FOFX_NOCOPYHOOKS

#endif // SPRT_WRAPPERS_WINDOWS_SHELLAPI_H_
