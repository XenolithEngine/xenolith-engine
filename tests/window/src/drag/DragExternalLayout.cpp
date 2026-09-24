/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCommon.h"

#include "drag/DragExternalLayout.h"
#include "XLDropTarget.h"
#include "XLUiStyleResolver.h"
#include "XL2dLayer.h"
#include "XLAction.h"
#include "XLDirector.h"
#include "XLServerAppThread.h"
#include "XLContext.h"
#include "XLCoreRenderSession.h"
#include "SPUrl.h"

#if SPRT_WINDOWS
#include <sprt/runtime/utils/dso.h>
#include <sprt/wrappers/windows/ole2.h>
#include <sprt/wrappers/windows/user_api.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/comdef.h>
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

#if SPRT_WINDOWS

namespace {

// One CF_HDROP, as the shell hands a file list over
class DragExternalDataObject : public IDataObject {
public:
	DragExternalDataObject(sprt::window::WideString &&files) : _files(sp::move(files)) { }
	virtual ~DragExternalDataObject() = default;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
		if (__sprt_memcmp(&riid, &__uuidof(IUnknown), sizeof(IID)) == 0
				|| __sprt_memcmp(&riid, &__uuidof(IDataObject), sizeof(IID)) == 0) {
			*ppv = static_cast<IDataObject *>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refs; }
	virtual ULONG STDMETHODCALLTYPE Release() override {
		auto count = --_refs;
		if (count == 0) {
			sprt::__delete(this);
		}
		return count;
	}

	virtual HRESULT STDMETHODCALLTYPE GetData(FORMATETC *fmt, STGMEDIUM *medium) override {
		if (QueryGetData(fmt) != S_OK) {
			return HRESULT(0x8004'0064L); // DV_E_FORMATETC
		}

		// DROPFILES, then the wide names, each NUL-terminated, and one more NUL
		auto size = sizeof(DROPFILES) + _files.size() * sizeof(char16_t) + sizeof(char16_t);
		auto handle = GlobalAlloc(GMEM_MOVEABLE, size);
		auto ptr = reinterpret_cast<uint8_t *>(GlobalLock(handle));
		__sprt_memset(ptr, 0, size);
		auto header = reinterpret_cast<DROPFILES *>(ptr);
		header->pFiles = sizeof(DROPFILES);
		header->fWide = TRUE;
		__sprt_memcpy(ptr + sizeof(DROPFILES), _files.data(), _files.size() * sizeof(char16_t));
		GlobalUnlock(handle);

		medium->tymed = TYMED_HGLOBAL;
		medium->hGlobal = handle;
		medium->pUnkForRelease = nullptr;
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *, STGMEDIUM *) override {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *fmt) override {
		return (fmt && fmt->cfFormat == CF_HDROP && (fmt->tymed & TYMED_HGLOBAL))
				? S_OK
				: HRESULT(0x8004'0064L);
	}
	virtual HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *, FORMATETC *) override {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE SetData(FORMATETC *, STGMEDIUM *, BOOL) override {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC **) override {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *, DWORD, IAdviseSink *, DWORD *) override {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **) override { return E_NOTIMPL; }

protected:
	sprt::atomic<ULONG> _refs = 1;
	sprt::window::WideString _files;
};

// The window of this process that registered a drop target; OLE keeps the interface in a window
// property under this name
struct DragExternalFind {
	HWND window = nullptr;
	IDropTarget *target = nullptr;
};

static bool DragExternal_findTarget(DragExternalFind &out) {
	using EnumProc = BOOL(WINAPI *)(HWND, LPARAM);
	using EnumWindowsFn = BOOL(WINAPI *)(EnumProc, LPARAM);
	using GetPidFn = DWORD(WINAPI *)(HWND, DWORD *);
	using GetPropFn = HANDLE(WINAPI *)(HWND, LPCWSTR);
	using CurrentPidFn = DWORD(WINAPI *)(void);

	static sprt::Dso s_user32(StringView("user32.dll"));
	static sprt::Dso s_kernel32(StringView("kernel32.dll"));
	if (!s_user32 || !s_kernel32) {
		return false;
	}

	static auto s_enum = s_user32.sym<EnumWindowsFn>("EnumWindows");
	static auto s_getPid = s_user32.sym<GetPidFn>("GetWindowThreadProcessId");
	static auto s_getProp = s_user32.sym<GetPropFn>("GetPropW");
	static auto s_currentPid = s_kernel32.sym<CurrentPidFn>("GetCurrentProcessId");
	if (!s_enum || !s_getPid || !s_getProp || !s_currentPid) {
		return false;
	}

	struct Ctx {
		DragExternalFind *out;
		DWORD pid;
	} ctx{&out, s_currentPid()};

	s_enum([](HWND hwnd, LPARAM param) -> BOOL {
		auto ctx = reinterpret_cast<Ctx *>(param);
		DWORD pid = 0;
		s_getPid(hwnd, &pid);
		if (pid != ctx->pid) {
			return TRUE;
		}
		auto prop = s_getProp(hwnd, reinterpret_cast<LPCWSTR>(u"OleDropTargetInterface"));
		if (!prop) {
			return TRUE;
		}
		ctx->out->window = hwnd;
		ctx->out->target = reinterpret_cast<IDropTarget *>(prop);
		return FALSE;
	}, reinterpret_cast<LPARAM>(&ctx));

	return out.target != nullptr;
}

// Two names with their terminators; the data object adds the list's final NUL
static sprt::window::WideString DragExternal_files() {
	static constexpr char16_t Files[] = u"C:\\tmp\\a b.txt\0C:\\tmp\\привет.png\0";
	return sprt::window::WideString(Files, sizeof(Files) / sizeof(char16_t) - 1);
}

static POINTL DragExternal_screenPoint(HWND hwnd, Vec2 world) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	POINT pt{LONG(world.x), LONG(rc.bottom - LONG(world.y) - 1)};
	ClientToScreen(hwnd, &pt);
	return POINTL{pt.x, pt.y};
}

} // namespace

#endif

namespace {

static constexpr auto s_css = StringView(R"css(
text-input { display: flex; width: 240px; height: 32px; background-color: #202020; }
text-input > label { color: #e0e0e0; font-size: 14px; }
)css");

static constexpr auto UriList = StringView("text/uri-list");
static constexpr auto TextPlain = StringView("text/plain");

} // namespace

bool DragExternalLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_css);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_files = addChild(Rc<basic2d::Layer>::create(Color::Teal_700), ZOrder(1));
	_files->setName("files");
	_files->setAnchorPoint(Anchor::BottomLeft);
	_files->setPosition(Vec2(60.0f, 300.0f));
	_files->setContentSize(Size2(240.0f, 120.0f));

	// Takes a file list and nothing else, as a Copy; the drop reads the list asynchronously
	setDropTarget(_files,
			DropTargetSlots{
				.accept = [this](const DragEvent &event) -> DragResponse {
		++_counters.accepts;
		if (!event.data || !event.data->hasType(UriList)) {
			return DragResponse();
		}
		return DragResponse{event.allowed & DragActions::Copy};
	},
				.enter = [this](const DragEvent &) { ++_counters.enters; },
				.over = [this](const DragEvent &) { ++_counters.overs; },
				.leave = [this](const DragEvent &) { ++_counters.leaves; },
				.drop = [this](const DragEvent &event, DragActions action) -> bool {
		++_counters.drops;
		++_drops;
		return event.data->read(UriList, [this, action](Status st, BytesView bytes) {
			_readText = StringView(reinterpret_cast<const char *>(bytes.data()), bytes.size())
								.str<Interface>();
			_readBeforeFinish =
					sprt::status::isSuccessful(st) && _offerA && !_offerA->hasFinished();

			// What a hand-made drop from a file manager shows, once the automatic run is over
			UrlView::readUriList(_readText, [&](StringView uri) {
				auto path = UrlView::readFilePath<Interface>(uri);
				log::source().info("DragExternalTest", "dropped (", toInt(action), "): ", uri,
						" -> ", path);
				_dropped.emplace_back(sp::move(path));
			});
		}, this);
	},
			});

	_field = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_field->setName("field");
	_field->setAnchorPoint(Anchor::BottomLeft);
	_field->setPosition(Vec2(60.0f, 200.0f));
	_field->setContentSize(Size2(240.0f, 32.0f));
	_field->setText(StringView("ab"));

	_readOnly = addChild(Rc<ui::TextInput>::create(), ZOrder(1));
	_readOnly->setName("read-only");
	_readOnly->setAnchorPoint(Anchor::BottomLeft);
	_readOnly->setPosition(Vec2(60.0f, 120.0f));
	_readOnly->setContentSize(Size2(240.0f, 32.0f));
	_readOnly->setText(StringView("locked"));
	_readOnly->setReadOnly(true);

	StringView paths[] = {StringView("/tmp/a b.txt"), StringView("/tmp/привет.png")};
	_uriList = StringView(sprt::window::makeFileUriList(makeSpanView(paths, 2))).str<Interface>();

	runAction(Rc<Sequence>::create(Rc<DelayTime>::create(0.6f), [this] { runPhase1(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase2(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase3(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase4(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase5(); }, Rc<DelayTime>::create(0.3f),
			[this] { runPhase6(); }, Rc<DelayTime>::create(0.3f), [this] { runPhase7(); },
			Rc<DelayTime>::create(0.3f), [this] { runPhase8(); }, Rc<DelayTime>::create(0.3f),
			[this] { runNativePhase1(); }, Rc<DelayTime>::create(0.3f), [this] {
		runNativePhase2();
	}, Rc<DelayTime>::create(0.3f), [this] { runNativePhase3(); }));
	return true;
}

void DragExternalLayout::handleEnter(Scene *scene) {
	TestLayout::handleEnter(scene);
	_drag = DragSystem::acquireForNode(this);

	auto director = getDirector();
	auto app = director ? dynamic_cast<ServerAppThread *>(director->getApplication()) : nullptr;
	if (app && app->getContext()) {
		_looper = app->getContext()->getLooper();
	}
}

void DragExternalLayout::handleExit() {
	if (_drag && _drag->isDragging()) {
		_drag->cancelDrag();
	}
	_drag = nullptr;

	TestLayout::handleExit();
}

void DragExternalLayout::expect(bool cond, StringView phase, StringView what) {
	++_checks;
	if (!cond) {
		++_failures;
		log::source().error("DragExternalTest", phase, ": ", what);
	}
}

Rc<core::MemoryDropOffer> DragExternalLayout::makeOffer(StringView type, StringView data,
		DragActions allowed) {
	if (!_looper) {
		return nullptr;
	}

	sprt::window::Vector<core::MemoryDropOffer::Representation> reps;
	auto &rep = reps.emplace_back();
	rep.type = type.str<sprt::window::String>();
	rep.data = BytesView(data).bytes<sprt::window::Bytes>();
	return Rc<core::MemoryDropOffer>::create(_looper, sp::move(reps), allowed);
}

void DragExternalLayout::send(core::DropPhase phase, core::MemoryDropOffer *offer, Vec2 world,
		DragActions preferred) {
	auto director = getDirector();
	auto server = director ? director->getRenderServer() : nullptr;
	if (!server || !offer) {
		return;
	}

	core::DropEvent ev;
	ev.phase = phase;
	ev.offer = offer;
	ev.location = world;
	ev.preferred = preferred;
	server->handleNativeDropEvent(sp::move(ev));
}

Vec2 DragExternalLayout::centre(Node *node) const {
	const auto size = node->getContentSize();
	return node->convertToWorldSpace(Vec2(size.width / 2.0f, size.height / 2.0f));
}

void DragExternalLayout::runPhase1() {
	expect(_drag != nullptr, "phase1", "no drag system was acquired");
	expect(_looper != nullptr, "phase1", "no context looper to stand in for the OS");
	if (!_drag || !_looper) {
		return;
	}

	expect(_drag->getTargetCount() >= 3, "phase1", "the targets are not registered");

	_offerA = makeOffer(UriList, _uriList, DragActions::Copy | DragActions::Move);
	send(core::DropPhase::Enter, _offerA, centre(_files));
	send(core::DropPhase::Motion, _offerA, centre(_files));
}

void DragExternalLayout::runPhase2() {
	if (!_offerA) {
		return;
	}

	auto session = _drag->getSession();
	expect(session != nullptr, "phase2", "an external enter did not start a session");
	if (session) {
		expect(session->isExternal(), "phase2", "the session does not say it is external");
		expect(session->getExternalOffer() == _offerA.get(), "phase2",
				"the session holds another offer");
		expect(session->getData() && session->getData()->isExternal(), "phase2",
				"the data does not say it is external");
		expect(session->getData() && session->getData()->getLocal() == nullptr, "phase2",
				"external data has a local object");
		expect(session->getData() && session->getData()->encode(UriList).empty(), "phase2",
				"external data encoded synchronously");
		expect(session->getTarget() == _files, "phase2", "the file target is not current");
	}

	expect(_counters.enters == 1, "phase2", "the target did not get exactly one enter");
	expect(_counters.overs >= 1, "phase2", "the target got no over");
	expect(_offerA->getLastStatus() == DragActions::Copy, "phase2",
			"the OS was not told the drop would copy");

	// The OS owns the cursor during its drag
	auto cursor = _drag->getCursorListener();
	expect(!cursor || !cursor->isEnabled(), "phase2", "an external drag set the cursor");

	// Out to where nothing accepts
	send(core::DropPhase::Motion, _offerA, convertToWorldSpace(Vec2(900.0f, 600.0f)));
}

void DragExternalLayout::runPhase3() {
	if (!_offerA) {
		return;
	}

	expect(_counters.leaves == 1, "phase3", "leaving the target did not fire leave");
	expect(_offerA->getLastStatus() == DragActions::None, "phase3",
			"the OS was not told there is nowhere to drop");
	expect(_drag->getSession() != nullptr, "phase3",
			"the session ended while still over the window");

	send(core::DropPhase::Motion, _offerA, centre(_files));
	send(core::DropPhase::Drop, _offerA, centre(_files));
}

void DragExternalLayout::runPhase4() {
	if (!_offerA) {
		return;
	}

	expect(_counters.drops == 1, "phase4", "the drop slot did not run once");
	expect(_counters.enters == _counters.leaves, "phase4", "enter and leave are not bracketed");
	expect(_drag->getSession() == nullptr, "phase4", "the session outlived the drop");
	expect(_offerA->getReadCount() == 1, "phase4", "the drop did not read the offer exactly once");
	expect(StringView(_readText) == StringView(_uriList), "phase4", "the read bytes differ");
	expect(_readBeforeFinish, "phase4", "the OS offer was finished before the read answered");
	expect(_offerA->hasFinished(), "phase4", "the OS offer was not finished");
	expect(_offerA->getPerformed() == DragActions::Copy, "phase4",
			"the OS was not told the drop copied");

	Vector<String> paths;
	UrlView::readUriList(_readText,
			[&](StringView uri) { paths.emplace_back(UrlView::readFilePath<Interface>(uri)); });
	expect(paths.size() == 2, "phase4", "the list does not hold two files");
	if (paths.size() == 2) {
		expect(paths[0] == "/tmp/a b.txt", "phase4", "the first path did not survive the list");
		expect(paths[1] == "/tmp/привет.png", "phase4", "the second path did not survive the list");
	}

	// A drag that leaves is never finished: only a dropped offer is
	_baseline = _counters;
	_offerB = makeOffer(UriList, _uriList, DragActions::Copy);
	send(core::DropPhase::Enter, _offerB, centre(_files));
	send(core::DropPhase::Leave, _offerB, centre(_files));
}

void DragExternalLayout::runPhase5() {
	if (!_offerB) {
		return;
	}

	expect(_counters.enters == _baseline.enters + 1, "phase5", "the second drag did not enter");
	expect(_counters.leaves == _baseline.leaves + 1, "phase5", "the second drag did not leave");
	expect(_counters.drops == _baseline.drops, "phase5", "a drag that left was dropped");
	expect(_drag->getSession() == nullptr, "phase5", "a drag that left kept its session");
	expect(!_offerB->hasFinished(), "phase5", "a drag that left was finished");

	// One drag at a time: an in-process drag in flight refuses the external one
	DragOffer local;
	local.local = Rc<Ref>(this);
	local.localType = String("test/local");
	local.allowedActions = DragActions::Move;
	local.defaultAction = DragActions::Move;
	auto session = _drag->beginDrag(sp::move(local), Rc<Ref>(this));
	expect(session != nullptr, "phase5", "the in-process drag did not start");

	_baseline = _counters;
	_offerC = makeOffer(UriList, _uriList, DragActions::Copy);
	send(core::DropPhase::Enter, _offerC, centre(_files));
}

void DragExternalLayout::runPhase6() {
	if (!_offerC) {
		return;
	}

	auto session = _drag->getSession();
	expect(session && !session->isExternal(), "phase6",
			"the external drag displaced the local one");
	expect(_counters.enters == _baseline.enters, "phase6", "a refused drag entered a target");

	_drag->cancelDrag();

	// Its drop is refused too, and the OS hears that nothing was done
	send(core::DropPhase::Drop, _offerC, centre(_files));

	// A drop over nothing
	_offerD = makeOffer(UriList, _uriList, DragActions::Copy);
	send(core::DropPhase::Enter, _offerD, convertToWorldSpace(Vec2(900.0f, 600.0f)));
	send(core::DropPhase::Drop, _offerD, convertToWorldSpace(Vec2(900.0f, 600.0f)));
}

void DragExternalLayout::runPhase7() {
	if (!_offerD) {
		return;
	}

	expect(_offerC->hasFinished(), "phase7", "a refused drop was not finished");
	expect(_offerC->getPerformed() == DragActions::None, "phase7",
			"a refused drop reported an action");
	expect(_offerD->hasFinished(), "phase7", "a drop over nothing was not finished");
	expect(_offerD->getPerformed() == DragActions::None, "phase7",
			"a drop over nothing reported an action");
	expect(_counters.drops == _baseline.drops, "phase7", "a refused drop reached a target");

	// Text from another application goes into a field as a paste does
	_offerE = makeOffer(TextPlain, StringView("ext"), DragActions::Copy | DragActions::Move);
	send(core::DropPhase::Enter, _offerE, centre(_field));
	send(core::DropPhase::Drop, _offerE, centre(_field));

	_offerF = makeOffer(TextPlain, StringView("x"), DragActions::Copy);
	send(core::DropPhase::Enter, _offerF, centre(_readOnly));
	send(core::DropPhase::Drop, _offerF, centre(_readOnly));
}

void DragExternalLayout::runPhase8() {
	if (!_offerE) {
		return;
	}

	expect(_field->getText() == "abext", "phase8", "the field did not take the external text");
	expect(_offerE->hasFinished() && _offerE->getPerformed() == DragActions::Copy, "phase8",
			"the text drop did not finish as a Copy");
	expect(_readOnly->getText() == "locked", "phase8", "a read-only field took external text");
	expect(_offerF->hasFinished() && _offerF->getPerformed() == DragActions::None, "phase8",
			"the read-only field's refusal did not reach the OS");
}

void DragExternalLayout::runNativePhase1() {
#if SPRT_WINDOWS
	DragExternalFind found;
	expect(DragExternal_findTarget(found), "native1",
			"no IDropTarget is registered for the window");
	if (!found.target) {
		return;
	}

	_nativeWindow = found.window;
	_nativeTarget = found.target;
	_nativeDropsBefore = _drops;

	auto app = dynamic_cast<ServerAppThread *>(getDirector()->getApplication());
	auto point = DragExternal_screenPoint(found.window, centre(_files));

	// OLE calls a target on the thread that registered it
	app->getContext()->performOnThread([this, point] {
		auto target = reinterpret_cast<IDropTarget *>(_nativeTarget);
		auto data = new (sprt::nothrow) DragExternalDataObject(DragExternal_files());

		DWORD effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
		target->DragEnter(data, 0, point, &effect);
		_nativeEffects[0] = effect;

		effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
		target->DragOver(0, point, &effect);
		_nativeEffects[1] = effect;

		data->Release();
	}, this);
#endif
}

void DragExternalLayout::runNativePhase2() {
#if SPRT_WINDOWS
	if (!_nativeTarget) {
		return;
	}

	auto app = dynamic_cast<ServerAppThread *>(getDirector()->getApplication());
	auto point = DragExternal_screenPoint(reinterpret_cast<HWND>(_nativeWindow), centre(_files));

	// By now the application has answered the enter; the drop takes that answer
	app->getContext()->performOnThread([this, point] {
		auto target = reinterpret_cast<IDropTarget *>(_nativeTarget);
		auto data = new (sprt::nothrow) DragExternalDataObject(DragExternal_files());

		DWORD effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
		target->DragOver(0, point, &effect);
		_nativeEffects[2] = effect;

		effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
		target->Drop(data, 0, point, &effect);
		_nativeEffects[3] = effect;

		data->Release();
	}, this);
#endif
}

void DragExternalLayout::runNativePhase3() {
#if SPRT_WINDOWS
	if (_nativeTarget) {
		expect(_nativeEffects[0] == DROPEFFECT_NONE, "native3",
				"DragEnter answered before the application did");
		expect(_nativeEffects[2] == DROPEFFECT_COPY, "native3",
				"DragOver did not carry the application's copy");
		expect(_nativeEffects[3] == DROPEFFECT_COPY, "native3", "Drop did not answer a copy");
		expect(_drops == _nativeDropsBefore + 1, "native3",
				"the OLE drop did not reach the target");
		expect(_dropped.size() >= 2 && _dropped[_dropped.size() - 2] == "/c/tmp/a b.txt"
						&& _dropped.back() == "/c/tmp/привет.png",
				"native3", "the CF_HDROP paths did not arrive as the runtime's paths");
	}
#endif

	log::source().warn("DragExternalTest", "SUMMARY: ", _checks, " checks, ", _failures,
			" failures");
	_summary = true;
	_dropped.clear();
}

void DragExternalLayout::registerCommands() {
	TestLayout::registerCommands();

	// World-space centres, in pixels from the bottom-left of the window, for a driver that plays
	// the OS side from outside the process
	addCommand("points", "World-space centres of the targets", [this](Value &&) {
		Value result;
		auto put = [&](StringView name, Node *node) {
			auto point = centre(node);
			Value value;
			value.setDouble(point.x, "x");
			value.setDouble(point.y, "y");
			result.setValue(sp::move(value), name);
		};
		put("files", _files);
		put("field", _field);
		put("read-only", _readOnly);
		return result;
	});

	addCommand("state", "What real drops did once the automatic run was over", [this](Value &&) {
		Value result;
		result.setBool(_summary, "summary");
		result.setBool(_drag && _drag->isDragging(), "dragging");
		auto session = _drag ? _drag->getSession() : nullptr;
		result.setBool(session && session->isExternal(), "external");
		result.setInteger(int64_t(_drops), "drops");
		Value paths(Value::Type::ARRAY);
		for (auto &it : _dropped) { paths.addString(it); }
		result.setValue(sp::move(paths), "paths");
		result.setString(_field->getText(), "field");
		return result;
	});
}

} // namespace stappler::xenolith::app
