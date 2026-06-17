# gui.pro — Qt5 GUI анонимизатор (цель: anonctl).
# Полноценный GUI с двухпанельным редактором, управлением словарём, отчётом об утечках
# и встроенной анонимизацией буфера обмена (замена старой минимальной обёртки).
TEMPLATE = app
TARGET   = anonctl
QT       = core gui widgets
include(../common.pri)

HEADERS += mainwindow.h
SOURCES += main_gui.cpp mainwindow.cpp

# --- Авто-деплой Qt DLL в Windows после сборки (windeployqt) ---
# Без этого anonctl.exe не запустится вне Qt Creator: Windows не находит
# Qt5Core/Gui/Widgets.dll, runtime-DLL MinGW или platforms/qwindows.dll.
# windeployqt копирует их все рядом с exe.
win32 {
    # $(DESTDIR_TARGET) — makefile-переменная qmake с реальным путём к exe
    # (учитывает подпапку release/ или debug/ при debug_and_release-сборке).
    windeployqt = $$shell_path($$dirname(QMAKE_QMAKE)/windeployqt.exe)
    QMAKE_POST_LINK += $$shell_quote($$windeployqt) $(DESTDIR_TARGET) $$escape_expand(\\n\\t)
}
