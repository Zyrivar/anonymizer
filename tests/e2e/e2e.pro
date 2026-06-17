# e2e.pro — end-to-end тест (реалистичный исходник C++ ~335 строк). Qt не нужен.
# Появляется как отдельная run-цель "test_e2e" в селекторе Qt Creator.
TEMPLATE = app
TARGET   = test_e2e
CONFIG  += console
CONFIG  -= app_bundle
QT      -= core gui

# testcase: даёт цель `make check`, которая собирает и запускает бинарь
# (кроссплатформенно — сама учитывает подпапку release/ и расширение .exe).
CONFIG  += testcase
testcase.timeout = 120

include(../../common.pri)
SOURCES += test_e2e.cpp
