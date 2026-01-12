/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_PRESENTATION_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_PRESENTATION_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/surface_info.h>

namespace sprt::window {

// Опции работы движка презентации кадров
// Позволяют балансировать презентацию кадров между нагрузкой на GPU, низкой задержкой ввода
// и стабильным кадровым интервалом.
// Настройки по умолчанию выполнены сбалансированно в пользу снижения задержки ввода
//
// Ряд опций зависит от реализации оконного менеджера. Для работы движок презантации получает
// предпочитаемый набор опций от окна файловой системы.
//
// При работе нужно помнить, что Xenolith требует стабильной асинхронной работы.
// Использовать опции, которые могут заблокировать презентацию или получение кадрового буфера
// категорически не следует, это может нарушить нормальную работу движка.
// Например, способ презентации в системе в режиме earlyPresent может блокировать презентацию
// по вертикальной синхронизации (оснвоной поток будет заблокирован до сигнала). В таком случае,
// этот режим в системе нужно отключить. Функции получения кадра и презентации всегда должны
// завершаться без блокировки.
//
// Особой осторожности требуют старые или экспериментальные системы, где асинхронный режим
// работы графичского стека не реализован на уровне системы. Для таких систем необходимо
// использовать собственную реализацию таймеров (режим usePresentWindow) вместо ожидания
// синхронизации от самой системы
struct PresentationOptions {
	// Запускать следующий кадр только по запросу либо наличию действий в процессе.
	// В таком случае, PresentationInterval будет считаться на основе реальной презентации
	// Показательными для производительности остаются интервал подготовки кадра и
	// таймер использования GPU
	bool renderOnDemand = true;

	// Использовать внешний сигнал вертикальной синхронизации (система должна поддерживать)
	// В этом режиме готовые к презентации кадры ожидают сигнала, прежде, чем отправиться
	// Также, по сигналу система запрашиваает новый буфер для отрисовки следующего кадра
	// Если система не успела подготовить новый кадр - обновление пропускается
	bool followDisplayLink = false;

	// Начинать новый кадр только по внешнему сигналу вертикальной синхронизации
	// В этом режиме система нчинает новый кадр по сигналу, и отправляет его на презентацию
	// по готовности без ожидания. Если новый сигнал синхронизации поступил до презентации
	// предыдущего кадра - новый кадр будет начат сразу же
	// Обеспечивает более низкую задержку ввода, чем базовый followDisplayLink, но требует
	// большей поддержки от ОС и менеджера окон.
	// Стандартный режим для Linux+Wayland и Android+Choreographer
	// Несовместим с usePresentWindow
	//
	// P.S. Интерфейс к Wayland запрашивает новый сигнал синхронизации только при отправке кадра
	// на презентацию. То есть, если кадр не укладывается в релизное окно, без поддержки от композитора
	// можно ожидать жёсткой потери кадров. В таком случае рекомендуется работать в режиме usePresentWindow
	bool followDisplayLinkBarrier = false;

	// Использовать внеэкранный рендеринг для подготовки изображений. В этом режиме презентация нового изображения выполняется
	// строго синхронно (см. presentImmediate)
	// TODO -пока не реализовано
	bool renderImageOffscreen = false;

	// Начинать новый кадр как только предыдущий был отправлен на исполнение (то есть, до его завершения и презентации)
	// Позволяет иметь несколько кадров в одновременной обработке для большей загрузке графического процессора
	// Не стоит применять без выделенного или интегрированного GPU (то есть, для рендеринга на CPU)
	bool preStartFrame = true;

	// Использовать временное окно для презентации кадра
	// Вместо презентации по готовности, система будет стараться удерживать целевую частоту кадров за счёт откладывания
	// презентации до следующего окна времени
	// Не работает в режиме followDisplayLink и followDisplayLinkBarrier
	bool usePresentWindow = true;

	// Отправлять кадр на презентацию сразу после его отправки в обработку. Может снизить видимую задержку ввода.
	// Использовать с осторожностью в режиме FIFO с двойной буферизацией (предпочитать тройную)
	bool earlyPresent = true;

	// Сихронизировать обновление параметров и закрытие окна с оконным менеджером
	// Необходимо для немедленного повторного использования поверхности менеджером окон (Android)
	//
	// В этом режиме вызов AppWindow::close может создавать существенную задержку
	bool syncConstraintsUpdate = false;
};

enum class UpdateConstraintsFlags : uint32_t {
	None,
	DeprecateSwapchain = 1 << 0,
	SwitchToFastMode = 1 << 1,
	SwitchToNext = 1 << 2,
	EndOfLife = 1 << 3,
	Finalized = 1 << 4,
	EnableLiveResize = 1 << 5,
	DisableLiveResize = 1 << 6,
	SyncUpdate = 1 << 7,

	// to be more clear what DeprecateSwapchain means
	WindowResized = DeprecateSwapchain,
};

SPRT_DEFINE_ENUM_AS_MASK(UpdateConstraintsFlags)

enum class PresentationUpdateFlags : uint32_t {
	None,
	DisplayLink = 1 << 0,
	FlushPending = 1 << 1,
};

SPRT_DEFINE_ENUM_AS_MASK(PresentationUpdateFlags)

struct PresentationFrameInfo {
	uint64_t order;
	FrameConstraints constraints;
};

} // namespace sprt::window

#endif
