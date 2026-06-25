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

#ifndef TESTS_WINDOW_CLIENT_SRC_CLIENTSCENE_H_
#define TESTS_WINDOW_CLIENT_SRC_CLIENTSCENE_H_

#include "XL2dScene.h"
#include "XL2dScrollView.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::client {

// Используем базовую 2D-сцену в качестве основы
class ClientScene : public basic2d::Scene2d {
public:
	virtual ~ClientScene() = default;

	// переопределяем создание сцены
	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &constraints) override;

	// переопределяем размещение объектов на сцене при изменении размера
	virtual void handleContentSizeDirty() override;

	virtual void handleEnter(Scene *) override;

protected:
	using Scene2d::init;

	// Простой закрашенный квадрат — минимальный тест клиентского рендеринга без шрифтов
	basic2d::Layer *_square = nullptr;

	// Запускаем бесконечную анимацию квадрата ровно один раз (проверка работы runAction в
	// клиентском контексте: пока действие активно, клиент шлёт серверу setReadyForNextFrame)
	bool _animStarted = false;
	uint64_t _animTick = 0;

	virtual StringView selectServerQueue(NotNull<AppThread> app,
			NotNull<core::RenderServerChannel> window) override;
};

} // namespace stappler::xenolith::client

#endif /* TESTS_WINDOW_CLIENT_SRC_CLIENTSCENE_H_ */
