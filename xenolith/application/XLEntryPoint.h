/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_XLENTRYPOINT_H_
#define XENOLITH_APPLICATION_XLENTRYPOINT_H_

#include "XLContext.h" // IWYU pragma: keep
#include "XLDirector.h" // IWYU pragma: keep
#include "XLScene.h" // IWYU pragma: keep
#include "SPEventTimerHandle.h" // IWYU pragma: keep

// Syntactic sugar for SharedModule definition

// Usage: DEFINE_CONFIG_FUNCTION((ContextConfig &config){ })

#define DEFINE_CONFIG_FUNCTION(Function)\
SP_USED static STAPPLER_VERSIONIZED_NAMESPACE::SharedExtension __macro_appMakeConfigSymbol(\
		STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_APPCOMMON_NAME,\
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeConfigName,\
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeConfigSignature(\
				[]Function));

#define DEFINE_SCENE_FACTORY(SceneFactoryFunction) \
static_assert(::std::is_same_v<decltype(&SceneFactoryFunction),\
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeSceneSignature>, \
	"Scene factory function should match :Context::SymbolMakeSceneSignature"); \
SP_USED static STAPPLER_VERSIONIZED_NAMESPACE::SharedExtension __macro_appCommonSceneFactorySymbol(\
		STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_APPCOMMON_NAME, \
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeSceneName, \
		&SceneFactoryFunction);

#define DEFINE_PRIMARY_SCENE_CLASS(SceneClass) \
	static STAPPLER_VERSIONIZED_NAMESPACE::Rc<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Scene> __macro_makeScene(\
		STAPPLER_VERSIONIZED_NAMESPACE::NotNull<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::AppThread> app, \
		STAPPLER_VERSIONIZED_NAMESPACE::NotNull<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::AppWindow> window, \
		const STAPPLER_VERSIONIZED_NAMESPACE::xenolith::core::FrameConstraints &constraints) { \
	return STAPPLER_VERSIONIZED_NAMESPACE::Rc<SceneClass>::create(app, window, constraints); \
} \
DEFINE_SCENE_FACTORY(__macro_makeScene)


#define DEFINE_APP_THREAD_CONSTRUCTOR(AppThreadConstructorFunction) \
static_assert(::std::is_same_v<decltype(&AppThreadConstructorFunction),\
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeAppThreadSignature>, \
	"AppThread constructor function should match Context::SymbolMakeAppThreadSignature"); \
SP_USED static STAPPLER_VERSIONIZED_NAMESPACE::SharedExtension __macro_appCommonAppThreadConstructorSymbol(\
		STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_APPCOMMON_NAME, \
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeAppThreadName, \
		&AppThreadConstructorFunction);

#define DEFINE_APP_THREAD_CLASS(AppThreadClass) \
	static STAPPLER_VERSIONIZED_NAMESPACE::Rc<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::AppThread> __macro_makeAppThread(\
		STAPPLER_VERSIONIZED_NAMESPACE::NotNull<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context> ctx) { \
	return STAPPLER_VERSIONIZED_NAMESPACE::Rc<AppThreadClass>::create(ctx); \
} \
DEFINE_APP_THREAD_CONSTRUCTOR(__macro_makeAppThread)


#define DEFINE_CONTEXT_CONSTRUCTOR(ContextConstructorFunction) \
static_assert(::std::is_same_v<decltype(&ContextConstructorFunction),\
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeContextSignature>, \
	"Context constructor function should match :Context::SymbolMakeContextSignature"); \
SP_USED static STAPPLER_VERSIONIZED_NAMESPACE::SharedExtension __macro_appCommonContextConstructorSymbol(\
		STAPPLER_VERSIONIZED_NAMESPACE::buildconfig::MODULE_APPCOMMON_NAME, \
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context::SymbolMakeContextName, \
		&ContextConstructorFunction);

#define DEFINE_CONTEXT_CLASS(ContextClass) \
	static STAPPLER_VERSIONIZED_NAMESPACE::Rc<STAPPLER_VERSIONIZED_NAMESPACE::xenolith::Context> __macro_makeContext(\
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::ContextConfig &&config, \
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::ContentInitializer &&ctxInit) { \
	return STAPPLER_VERSIONIZED_NAMESPACE::Rc<ContextClass>::create(\
		STAPPLER_VERSIONIZED_NAMESPACE::move(config),\
		STAPPLER_VERSIONIZED_NAMESPACE::move(ctxInit)\
	); \
} \
DEFINE_CONTEXT_CONSTRUCTOR(__macro_makeContext)


#endif // XENOLITH_APPLICATION_XLENTRYPOINT_H_
