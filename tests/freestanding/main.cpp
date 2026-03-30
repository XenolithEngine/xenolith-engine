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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sprt/runtime/log.h>
#include <sprt/runtime/platform.h>

namespace sprt {

void performUnameTest();
void performUnistdTest();
void performDirTest();
void performLinkTest();
void performListTests();

void performPthreadTest();
void performPthreadCondTest();
void performPthreadRwlockTest();
void performPthreadBarrierTest();
void performPthreadSpinlockTest();

void performMallocStringTests();

void performMallocListTests();
void performMallocForwardListTests();

void performHashTests();

} // namespace sprt

int main(int argc, const char *argv[]) {
	auto str =
			"Проверяю работоспособность вывода в UTF-8, строка должна быть читаема из терминала\n";

	fwrite(str, strlen(str), 1, stdout);

	sprt::performMallocForwardListTests();
	sprt::performMallocListTests();
	/*sprt::performHashTests();
	sprt::performMallocStringTests();

	sprt::oslog::vpinfo(__SPRT_LOCATION, "main", "Exec path: ", sprt::platform::getExecPath());
	sprt::oslog::vpinfo(__SPRT_LOCATION, "main", "Home path: ", sprt::platform::getHomePath());
	sprt::oslog::vpinfo(__SPRT_LOCATION, "main",
			"Unique Device Id: ", sprt::platform::getUniqueDeviceId());

	sprt::performPthreadTest();
	sprt::performPthreadCondTest();
	sprt::performPthreadRwlockTest();
	sprt::performPthreadBarrierTest();
	sprt::performPthreadSpinlockTest();
	sprt::performUnameTest();
	sprt::performUnistdTest();
	sprt::performDirTest();
	sprt::performLinkTest();*/
	return 0;
}
