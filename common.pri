# common.pri — общий конфиг: слои core (domain/application/infrastructure)
# + вендоренные исходники tree-sitter и пути включения.
CONFIG += c++17
CONFIG -= app_bundle

# Грамматики tree-sitter содержат одноимённые файлы (parser.c, scanner.c) в
# разных каталогах. Без этого их объектные файлы (parser.o/scanner.o) перетёрли
# бы друг друга и при линковке пропал бы, например, tree_sitter_cpp.
# object_parallel_to_source раскладывает .o по подкаталогам, повторяя структуру.
CONFIG += object_parallel_to_source

# Исходники в UTF-8 (кириллица в комментариях). MSVC иначе выдаёт warning C4819;
# MinGW/GCC и Clang используют UTF-8 по умолчанию, флаг для них безвреден.
msvc: QMAKE_CXXFLAGS += /utf-8

INCLUDEPATH += \
    $$PWD/src \
    $$PWD/third_party \
    $$PWD/third_party/tree-sitter/lib/include \
    $$PWD/third_party/tree-sitter/lib/src \
    $$PWD/third_party/tree-sitter-cpp/src \
    $$PWD/third_party/tree-sitter-python/src

# --- Слой core: domain (header-only) + application + infrastructure ---
HEADERS += \
    $$PWD/src/domain/Types.h \
    $$PWD/src/domain/Placeholder.h \
    $$PWD/src/domain/IParser.h \
    $$PWD/src/domain/Dictionary.h \
    $$PWD/src/domain/EditApplier.h \
    $$PWD/src/domain/Policies.h \
    $$PWD/src/application/Anonymizer.h \
    $$PWD/src/application/Deanonymizer.h \
    $$PWD/src/application/LeakAuditor.h \
    $$PWD/src/application/AnonymizerService.h \
    $$PWD/src/infrastructure/TreeSitterParser.h \
    $$PWD/src/infrastructure/LanguageProfile.h \
    $$PWD/src/infrastructure/CppLanguageProfile.h \
    $$PWD/src/infrastructure/PythonLanguageProfile.h \
    $$PWD/src/infrastructure/LanguageProfiles.h \
    $$PWD/src/infrastructure/JsonFileDictionaryStore.h

SOURCES += \
    $$PWD/src/application/Anonymizer.cpp \
    $$PWD/src/application/Deanonymizer.cpp \
    $$PWD/src/application/LeakAuditor.cpp \
    $$PWD/src/infrastructure/TreeSitterParser.cpp \
    $$PWD/src/infrastructure/CppLanguageProfile.cpp \
    $$PWD/src/infrastructure/PythonLanguageProfile.cpp \
    $$PWD/src/infrastructure/LanguageProfiles.cpp \
    $$PWD/src/infrastructure/JsonFileDictionaryStore.cpp

# --- Вендоренные C-исходники tree-sitter (qmake компилирует их как C) ---
SOURCES += \
    $$PWD/third_party/tree-sitter/lib/src/lib.c \
    $$PWD/third_party/tree-sitter-cpp/src/parser.c \
    $$PWD/third_party/tree-sitter-cpp/src/scanner.c \
    $$PWD/third_party/tree-sitter-python/src/parser.c \
    $$PWD/third_party/tree-sitter-python/src/scanner.c
