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

#include "InstallerStrings.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer::strings {

static Lang s_lang = Lang::En;

void setLang(Lang lang) { s_lang = lang; }
Lang getLang() { return s_lang; }

// Every string below is a literal with static storage, so returning a view of one is safe.
static StringView tr(StringView en, StringView ru, StringView zh) {
	switch (s_lang) {
	case Lang::Ru: return ru;
	case Lang::Zh: return zh;
	case Lang::En: break;
	}
	return en;
}

StringView appTitle() {
	return tr("Xenolith Installer", "Xenolith Installer", "Xenolith Installer");
}
StringView colName() { return tr("Name", "Имя", "名称"); }
StringView colSize() { return tr("Size", "Размер", "大小"); }
StringView colStatus() { return tr("Status", "Статус", "状态"); }
StringView groupEngine() { return tr("Engine", "Движок", "引擎"); }
StringView groupHosts() {
	return tr("Development Tools (hosts)", "Инструменты (hosts)", "开发工具 (hosts)");
}
StringView groupTargets() {
	return tr("Runtime Platforms (targets)", "Платформы (targets)", "运行平台 (targets)");
}
StringView engineNotReady() {
	return tr("Engine not prepared — tap + to clone", "Движок не готов — нажмите +", "引擎未准备");
}
StringView statusInstalled() { return tr("Installed", "Установлен", "已安装"); }
StringView statusNotInstalled() { return tr("Not Installed", "Не установлен", "未安装"); }
StringView statusUpdateAvailable() { return tr("Update available", "Есть обновление", "有更新"); }
StringView phaseDownloading() { return tr("Downloading", "Загрузка", "下载中"); }
StringView actionInstallEverything() {
	return tr("Install everything", "Установить всё", "全部安装");
}
String actionInstallSelected(size_t n) {
	return toString(tr("Install selected", "Установить выбранные", "安装所选"), " (", n, ")");
}
StringView actionRefreshAll() { return tr("Refresh all", "Обновить всё", "全部刷新"); }
StringView actionInstall() { return tr("Install", "Установить", "安装"); }
StringView actionDelete() { return tr("Delete", "Удалить", "删除"); }
StringView actionPrepare() { return tr("Prepare", "Подготовить", "准备"); }
StringView actionRefresh() { return tr("Refresh", "Обновить", "刷新"); }
StringView actionCancel() { return tr("Cancel", "Отмена", "取消"); }
StringView actionClose() { return tr("Close", "Закрыть", "关闭"); }
StringView confirmInstallTitle() { return tr("Install everything", "Установить всё", "全部安装"); }
StringView confirmInstallMessage() {
	return tr("Install the engine, your host toolchain and the native target?",
			"Установить движок, host и native target?", "安装引擎、主机工具链和本机目标？");
}
StringView confirmInstallSelectedTitle() {
	return tr("Install selected", "Установить выбранные", "安装所选");
}
String confirmInstallSelectedMessage(size_t n) {
	return toString(tr("Install ", "Установить ", "安装 "), n,
			tr(" component(s)?", " компонент(ов)?", " 个组件？"));
}
StringView confirmDeleteTitle() { return tr("Delete component", "Удалить компонент", "删除组件"); }
String confirmDeleteMessage(StringView label) {
	return toString(tr("Remove ", "Удалить ", "删除 "), label,
			tr(" and all its files?", " и все файлы?", " 及其所有文件？"));
}
StringView confirmEngineTitle() { return tr("Prepare engine", "Подготовить движок", "准备引擎"); }
StringView confirmEngineMessage() {
	return tr("Clone the default engine ref into the local data directory?",
			"Клонировать engine в локальный каталог?", "将默认引擎克隆到本地目录？");
}
StringView confirmRefreshTitle() { return tr("Refresh updates", "Обновить пакеты", "刷新更新"); }
StringView confirmRefreshMessage() {
	return tr("Reinstall components that have updates available?",
			"Переустановить компоненты с обновлениями?", "重新安装有更新的组件？");
}
StringView gearOpenDataDir() {
	return tr("Open data directory", "Открыть каталог данных", "打开数据目录");
}
StringView gearStorage() { return tr("Storage", "Хранилище", "存储"); }
StringView gearSettings() { return tr("Settings", "Настройки", "设置"); }
StringView gearDoctor() { return tr("Doctor", "Диагностика", "诊断"); }
StringView storageTitle() { return tr("Storage usage", "Использование диска", "存储占用"); }
StringView settingsTitle() { return tr("Settings", "Настройки", "设置"); }
StringView doctorTitle() { return tr("Doctor", "Диагностика", "诊断"); }
StringView projectsTitle() { return tr("Projects", "Проекты", "项目"); }
StringView onboardingTitle() {
	return tr("Welcome to Xenolith", "Добро пожаловать в Xenolith", "欢迎使用 Xenolith");
}
StringView onboardingBody() {
	return tr("Install the SDK for your machine in one click.",
			"Установите SDK для вашей машины в один клик.", "一键安装适用于本机的 SDK。");
}
StringView tabPackages() { return tr("Packages", "Пакеты", "软件包"); }
StringView tabProjects() { return tr("Projects", "Проекты", "项目"); }
StringView projectNew() { return tr("+ New Project", "+ Новый проект", "+ 新建项目"); }
StringView projectBack() { return tr("← Back", "← Назад", "← 返回"); }
StringView projectName() { return tr("Name", "Имя", "名称"); }
StringView projectLocation() { return tr("Location", "Папка", "位置"); }
StringView projectChoose() { return tr("Choose…", "Выбрать…", "选择…"); }
StringView projectCreate() { return tr("Create", "Создать", "创建"); }
StringView projectBuild() { return tr("Build", "Собрать", "构建"); }
StringView projectRun() { return tr("Run", "Запуск", "运行"); }
StringView projectRemove() { return tr("Remove", "Убрать", "移除"); }
String projectRemoveMessage(StringView name) {
	return toString(tr("Remove ", "Убрать ", "移除 "), name,
			tr(" from the registry? Files on disk stay.", " из списка? Файлы на диске останутся.",
					" ？磁盘上的文件保留。"));
}
StringView projectOpenFolder() { return tr("Open folder", "Открыть папку", "打开文件夹"); }
StringView projectsEmpty() {
	return tr("No projects yet. Create one to scaffold a Xenolith app.",
			"Пока нет проектов. Создайте новый, чтобы сгенерировать приложение.",
			"还没有项目。创建一个以生成 Xenolith 应用。");
}
StringView projectNameRule() {
	return tr("Use letters, digits, '-' or '_' only.", "Только буквы, цифры, '-' или '_'.",
			"仅限字母、数字、'-' 或 '_'。");
}
StringView projectPathNoSpace() {
	return tr("Location path must not contain spaces.", "Путь не должен содержать пробелов.",
			"路径不能包含空格。");
}
StringView projectNeedSdk() {
	return tr("Install the engine and a target toolchain on the Packages tab first.",
			"Сначала установите engine и target на вкладке Packages.",
			"请先在 Packages 页安装引擎和目标工具链。");
}
StringView projectCreating() { return tr("Creating…", "Создание…", "创建中…"); }

} // namespace stappler::xenolith::installer::strings
