# unit.pro — юнит-тесты доменного ядра (assert-based). Qt не нужен.
# Появляется как отдельная run-цель "test_unit" в селекторе Qt Creator.
TEMPLATE = app
TARGET   = test_unit
CONFIG  += console
CONFIG  -= app_bundle
QT      -= core gui

# testcase: даёт цель `make check`, которая собирает и запускает бинарь
# (кроссплатформенно — сама учитывает подпапку release/ и расширение .exe).
CONFIG  += testcase
testcase.timeout = 120

include(../../common.pri)
SOURCES += test_core.cpp
