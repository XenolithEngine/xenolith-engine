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

// Прикладной предкомпилированный заголовок (LOCAL_PRECOMPILED_HEADERS).
// При отладке через live reload заголовки движка считаются стабильными и
// предкомпилируются на уровне приложения, чтобы не парсить их в каждом TU.
//
// ВАЖНО: сюда включаются ТОЛЬКО стабильные заголовки движка/фреймворка.
// Локальные заголовки приложения (ClientScene.h и т.п.) меняются между
// итерациями и в PCH попадать НЕ должны — иначе каждая правка инвалидирует PCH.
//
// Порядок включения повторяет проверенный порядок из TU приложения:
// заголовки 2d (тянут XL2d.h) идут ДО remote/context-заголовков, т.к. XL2d.h
// содержит порядок-зависимый `using font::Autofit;` (см. XL2d.h:60).

#ifndef TESTS_WINDOW_CLIENT_SRC_CLIENTPCH_H_
#define TESTS_WINDOW_CLIENT_SRC_CLIENTPCH_H_

// База ядра движка (модульный PCH xenolith_core)
#include "XLCommon.h"

// xenolith application / context
#include "XLContext.h"

// xenolith 2d: сцена и виджеты (XL2dSceneContent.h тянет XL2d.h — включаем рано)
#include "XL2dSceneContent.h"
#include "XL2dScene.h"
#include "XL2dScrollView.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"
#include "XLUiButton.h"
#include "XLUiCloseGuardWidget.h"

// окружение приложения
#include "XLDirector.h"
#include "XLAppWindow.h"
#include "XLRemoteWindow.h"
#include "XLInputListener.h"
#include "XLEntryPoint.h"

// remote / точка входа клиента
#include "XLClientContext.h"
#include "XLRemoteProtocol.h"

// stappler core
#include "SPCoreCrypto.h"

#endif /* TESTS_WINDOW_CLIENT_SRC_CLIENTPCH_H_ */
