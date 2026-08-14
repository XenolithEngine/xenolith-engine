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

#ifndef UTILS_INSTALLER_CORE_SRC_INSTALLERCORE_SCU_CPP_
#define UTILS_INSTALLER_CORE_SRC_INSTALLERCORE_SCU_CPP_

#include "SPICommon.h"

#include "SPIDirs.cc"
#include "SPITriple.cc"
#include "SPITransport.cc"
#include "SPICatalogue.cc"
#include "SPISettings.cc" // after Catalogue/EngineSource headers: resolves against their defaults
#include "SPIState.cc"
#include "SPIEngineSource.cc"
#include "SPIJob.cc"
#include "SPIProcess.cc"
#include "SPIInstall.cc"
#include "SPIScaffold.cc"
#include "SPIBuild.cc"
#include "SPIProjects.cc"

#endif // UTILS_INSTALLER_CORE_SRC_INSTALLERCORE_SCU_CPP_
