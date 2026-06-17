# unit.pro — юнит-тесты доменного ядра (assert-based). Qt не нужен.
# Появляется как отдельная run-цель "test_unit" в селекторе Qt Creator.
TEMPLATE = app
TARGET   = test_unit
CONFIG  += console
CONFIG  -= app_bundle
QT      -= core gui

include(../../common.pri)
SOURCES += test_core.cpp
