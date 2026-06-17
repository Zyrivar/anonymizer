# anonymizer.pro — верхний уровень. Сборка: `qmake && make`. Тесты: `make check`.
#   anonctl   : Qt5 GUI (presentation-слой над core)            [Qt5 widgets]
#   test_unit : юнит-тесты доменного ядра    (run-цель в Qt Creator)
#   test_e2e  : end-to-end тест              (run-цель в Qt Creator)
#
# Архитектура (Clean Architecture, см. src/):
#   domain/         — сущности, value-объекты, порты (без внешних зависимостей)
#   application/    — use-cases (Anonymizer, Deanonymizer, LeakAuditor) + фасад
#   infrastructure/ — реализации портов (TreeSitterParser, JsonFileDictionaryStore)
#   presentation    — gui/ (Qt5)
TEMPLATE = subdirs

SUBDIRS = gui unit e2e

# Подпроекты тестов лежат в tests/, но объявлены здесь напрямую,
# чтобы Qt Creator показал test_unit и test_e2e как отдельные run-цели
# в селекторе запуска (слева внизу), а не прятал их во вложенном subdirs.
unit.subdir = tests/unit
e2e.subdir  = tests/e2e

# Консольный запуск всех тестов одной командой: `make check`
check.CONFIG  = recursive
check.commands = \
    cd $$shell_path($$OUT_PWD/tests/unit) && $(MAKE) && ./test_unit && \
    cd $$shell_path($$OUT_PWD/tests/e2e)  && $(MAKE) && ./test_e2e
QMAKE_EXTRA_TARGETS += check
