# C++ Anonymizer — QMake project (Qt5 / C++17)

Статический анонимизатор кода C++: обратимо заменяет имена, строки, комментарии,
user-инклуды и (опц.) чувствительные подстроки на плейсхолдеры. Движок —
tree-sitter-cpp (завендорен, компилятор не нужен). Инварианты: точный round-trip и
один символ → один ID на весь проект.

Построен по **Clean Architecture** с явными слоями и инверсией зависимостей.

## Сборка

```sh
qmake
make            # nmake / jom на Windows
make check      # собрать и прогнать тесты
```
Собирается GUI-бинарь **anonctl** (Qt5) и два тестовых бинаря (test_unit, test_e2e).

## Архитектура

Зависимости направлены внутрь: `presentation → application → domain ← infrastructure`.
Доменный слой не зависит ни от Qt, ни от tree-sitter, ни от nlohmann/json.

```
src/
  domain/             — сущности и порты (без внешних зависимостей)
    Types.h             Category, Hit, Leak (value-объекты)
    Placeholder.h       PlaceholderFactory — генерация/распознавание "ID_0001"
    Dictionary.h        агрегат словаря + порт IDictionaryStore
    IParser.h           порт парсера
    EditApplier.h       применение замен к тексту (right-to-left)
    Policies.h          PreserveList, ScrubPatterns, LeakPatterns
  application/        — use-cases (оркестрация поверх портов)
    Anonymizer.{h,cpp}      прямая анонимизация (forward)
    Deanonymizer.{h,cpp}    восстановление: restoreTokens / restoreText
    LeakAuditor.{h,cpp}     аудит утечек
    AnonymizerService.h     фасад + composition root (ручной DI)
  infrastructure/     — реализации портов
    TreeSitterParser.{h,cpp}        IParser на tree-sitter-cpp (RAII внутри)
    JsonFileDictionaryStore.{h,cpp} IDictionaryStore на JSON-файле
gui/                  — presentation (Qt5): mainwindow.{h,cpp}, main_gui.cpp
tests/  unit/ e2e/    — юнит- и end-to-end тесты
third_party/          — tree-sitter, tree-sitter-cpp, nlohmann/json (vendored)
```

### Порты и адаптеры

Интерфейсы введены только там, где есть реальная альтернатива реализации:
- **IParser** — сейчас `TreeSitterParser`; альтернативы: libclang (точнее различает
  члены библиотечных типов) или лёгкий regex-парсер.
- **IDictionaryStore** — сейчас `JsonFileDictionaryStore`; альтернативы: in-memory
  для тестов, SQLite для больших словарей.

Use-cases и value-объекты — конкретные классы (спекулятивная абстракция не вводилась).

## GUI

**anonctl** — полноценное Qt5 окно (presentation-слой над `AnonymizerService`):
- Две панели: исходный код ↔ анонимизированный результат
- Тулбар: **Forward**, **Reverse**, **Reverse Text**, **Leak Scan**, выбор режима (opaque/format)
- **Меню Dictionary**: создать / загрузить / сохранить словарь (JSON через `IDictionaryStore`)
- **Меню Clipboard**: анонимизировать / деанонимизировать буфер (Ctrl+Shift+A / D)
- **Leak Report**: док-панель снизу с таблицей найденных утечек
- Статусбар: статистика словаря в реальном времени
- Batch: File → Batch process (выбор входной и выходной директорий)

## Тесты

Запуск из Qt Creator: `test_unit` и `test_e2e` — отдельные run-цели в селекторе.
Из консоли: `make check` собирает и гоняет оба набора.

- **test_unit** (29 проверок) — use-cases через фасад: round-trip, идемпотентность,
  preserve-list, extern-трекинг, leak-детекция, format-режим, JSON round-trip словаря.
- **test_e2e** (71 проверка) — реалистичный SCADA-исходник ~335 строк через полный
  конвейер: анонимизация, восстановление, аудит утечек, межфайловая консистентность.

## Использование (программно)

```cpp
#include "application/AnonymizerService.h"

application::AnonymizerService svc;   // composition root: парсер + use-cases
domain::Dictionary dict;

// Анонимизация (opaque или format)
std::string anon = svc.anonymize(source, dict, application::StringMode::Opaque);

// Восстановление кода (точный round-trip) или произвольного текста
std::string code = svc.restoreCode(anon, dict);
std::string text = svc.restoreText(llmAnswer, dict);

// Аудит утечек
std::vector<domain::Leak> leaks = svc.audit(source);

// Сохранение/загрузка словаря
infrastructure::JsonFileDictionaryStore store;
store.save("dict.json", dict);
domain::Dictionary loaded = store.load("dict.json");
```

## Копипаст-воркфлоу

Через GUI (горячие клавиши внутри anonctl):
- **Ctrl+Shift+A** — анонимизировать буфер обмена
- **Ctrl+Shift+D** — деанонимизировать буфер обмена

## Плейсхолдеры

`ID_*` имена · `STR_*` строки · `CMT_*` комментарии · `HDR_*` заголовки ·
скраб: `IP_* PATH_* HOST_* MAC_* EMAIL_* URL_*`.

## Безопасность

Словарь содержит оригиналы — это секрет: код отдаём наружу, словарь и leak-report
держим локально. Перед отправкой прогоняем Leak Scan.

## Ограничения

- Компилируемость вывода не гарантируется (требование снято осознанно).
- «Твой символ vs библиотечный» — по curated preserve-list (`domain/Policies.h`), не
  семантически; точное различение членов библиотечных типов дал бы libclang (порт
  `IParser` для этого и введён).
- Скраб-регэкспы эвристичны (особенно `HOST`) — дотачиваются под конкретные данные.
- Объём — только код C++ (форматы данных вне текущего scope).
- `restoreText` имеет сложность O(N×M) на проход × размер словаря; при больших
  словарях стоит перейти на Aho-Corasick (отмечено как TODO).


---

## Changelog

### v3 — Clean Architecture, удаление CLI

Полный рефакторинг монолитного ядра (`anon_core.{h,cpp}`, набор свободных функций
в `namespace anon`) в слоистую архитектуру с инверсией зависимостей.

- **CLI удалён полностью** (директория `cli/`, `main.cpp`). Точка входа — только GUI.
- **Доменный слой** (`src/domain/`): чистые value-объекты (`Category`, `Hit`, `Leak`),
  агрегат `Dictionary` (инкапсулирует разделы, прячет JSON), `PlaceholderFactory`
  (генерация/распознавание плейсхолдеров со счётчиками), `EditApplier`, доменные
  политики (`PreserveList`, `ScrubPatterns`, `LeakPatterns`). Без зависимостей от
  Qt / tree-sitter / nlohmann.
- **Порты** (`IParser`, `IDictionaryStore`) — введены только там, где есть реальная
  альтернатива реализации (по запросу: «интерфейсы где есть реальная альтернатива»).
- **Use-cases** (`src/application/`): `Anonymizer`, `Deanonymizer`, `LeakAuditor` —
  оркестрация поверх портов, зависят только от абстракций. Фасад `AnonymizerService`
  как composition root с ручным DI.
- **Инфраструктура** (`src/infrastructure/`): `TreeSitterParser` (весь C-API
  tree-sitter и RAII инкапсулированы в .cpp), `JsonFileDictionaryStore` (маппинг
  домена ↔ JSON-схема, обратно совместим со старым форматом словаря).
- **GUI** переписан под `AnonymizerService` + `domain::Dictionary` +
  `JsonFileDictionaryStore` вместо прямых вызовов ядра и работы с `nlohmann::json`.
- **Тесты** переписаны под новый API через фасад: unit 29/29, e2e 71/71 (qmake-сборка).

### v2 — Code review, bug fixes, GUI

#### Критические исправления

1. **RAII для tree-sitter ресурсов.** `TSParser` и `TSTree` — C-объекты без деструкторов.
   Раньше при исключении (из `nlohmann::json`, `std::regex` и т.д.) между `ts_parser_new()`
   и `ts_parser_delete()` ресурсы утекали. Добавлены обёртки `TSParserGuard`, `TSTreeGuard`
   и `ParseResult`, которые гарантируют освобождение при любом исходе.

2. **Проверка argc для каждого CLI-режима.** При недостаточном числе аргументов для
   `forward`/`reverse`/`reverse-text` (нужно 5) или `batch` (нужно 5) происходил доступ
   за границы `argv` → UB. Добавлена per-mode валидация с выводом usage.

3. **Проверка nullptr от `ts_parser_parse_string()`.** Функция может вернуть `nullptr`
   (allocation failure). `ParseResult` проверяет оба этапа (создание парсера и парсинг)
   и бросает `std::runtime_error` при неудаче.

#### Высокий приоритет

4. **Итеративный обход AST.** `collect()` и `walkExtern()` переписаны с рекурсии на
   итеративный DFS с явным `std::vector`-стеком. Устраняет риск переполнения стека
   на глубоко вложенном коде (автогенерация, шаблоны, макро-таблицы в SCADA-проектах).

5. **Кэширование regex.** `scrubbers()` и `leakPats()` теперь возвращают `static const&`
   вместо копии вектора. Regex компилируются один раз за весь процесс. Ранее в `batch`
   режиме на каждый файл и каждый строковый литерал пересоздавались все regex — на порядки
   медленнее, чем нужно.

6. **Другие regex вынесены в static.** `isPlaceholder`, `maxIdx`, `reverseText` —
   все regex-объекты, которые использовались в циклах, стали `static const`.

#### Исправление ошибки сборки (MinGW 7.3 / GCC 7.3)

7. **`std::filesystem` → совместимость с GCC < 8.** На MinGW 7.3 `std::filesystem`
   не существует — только `std::experimental::filesystem`. Добавлен `__has_include`
   shim с автоматическим выбором нужного варианта. Порядок инклудов изменён:
   `<filesystem>` до `"anon_core.h"` (до `nlohmann/json`), что также устраняет
   ADL-конфликт `begin()`/`end()`.

8. **ADL-конфликт nlohmann/json + range-for.** Макрос `NLOHMANN_CAN_CALL_STD_FUNC_IMPL`
   инжектирует свободные `begin()`/`end()` через ADL, которые конфликтовали с range-for
   по `std::vector<fs::path>` на GCC 7.3. Все проблемные range-for заменены на
   index-based / explicit iterator циклы.

9. **`%zu` → `%u` + cast.** MinGW 7.3 MSVCRT не поддерживает `%zu` в `fprintf`.
   Заменено на `%u` с `(unsigned)` кастом.

10. **`-lstdc++fs` для линковки.** GCC < 9 требует явного `-lstdc++fs` для
    `std::filesystem` / `std::experimental::filesystem`. Добавлен в `cli.pro`.

#### Средний приоритет

11. **`pad()` переписан на `std::to_string`.** Убран `snprintf` с фиксированным буфером
    `char[48]` — теперь нет ограничения длины префикса.

12. **Magic number `10` → `kDeclaratorLen`.** Константа длины строки `"declarator"`
    для `ts_node_child_by_field_name()` вынесена в именованную переменную.

13. **Подавление `-Wunused-variable`.** Structured bindings `auto& [k, v]` в `maxIdx()`
    и `forward()` — добавлен `(void)k` где `k` не используется.

#### Стиль и читаемость

14. **Форматирование.** Все плотные однострочные функции раскрыты: `forward()` (основной
    цикл по хитам), `reverseTokens()` (обратные маппинги переименованы в `invNames`,
    `invStrings`, `invComments`, `invIncludes`), `reverseText()`, `scrubString()`,
    `isPlaceholder()`, `replaceAll()`, `slice()`, `applyEdits()`, `maxIdx()`,
    `ensureDict()`, `declName()`.

15. **Комментарии.** Добавлены поясняющие комментарии: `extern "C"` для tree-sitter API,
    TODO для экстернализации `PRESERVE` в конфиг-файл, TODO для Aho-Corasick в
    `reverseText`, TODO для коллизии include-ключей в batch.

#### Юнит-тесты

16. **17 тестов** в `tests/test_core.cpp` (assert-based, без фреймворка):
    - Round-trip: `reverse(forward(x)) == x` для простого кода, строк, инклудов,
      многострочных комментариев
    - Консистентность: один символ → один ID между вызовами
    - Preserve-list: `std`, `vector`, `main`, `push_back` не анонимизируются
    - Forward производит плейсхолдеры, оригинал исчезает
    - Пустой вход
    - `reverseText` восстанавливает плейсхолдеры в прозе
    - Format mode: IP-адреса в строках заменяются на `IP_*`
    - Leak scan: детектирует IP в строковых литералах
    - Leak scan: не фолзит на плейсхолдерах (`IP_0001`)
    - Идемпотентность: `forward(forward(x))` не меняет результат
    - `ensureDict` создаёт все 5 секций
    - `extern` декларации трекаются с флагом `"extern": true`

    Сборка: `cd tests && qmake && make && ./test_anon`

#### GUI (Qt5)

17. **Полноценный Qt5 GUI** (`gui/mainwindow.{h,cpp}`, `gui/main_gui.cpp`):
    - Двухпанельный редактор с моноширинным шрифтом и QSplitter
    - Тулбар: Forward, Reverse, Reverse Text, Leak Scan, комбобокс режима
    - Меню File: Open source, Save result, Batch process, Quit
    - Меню Dictionary: New / Load / Save / Save As (с защитой несохранённых изменений)
    - Меню Clipboard: Anonymize (Ctrl+Shift+A) / Deanonymize (Ctrl+Shift+D) —
      полная замена старого `anonctl`
    - Dock-панель Leak Report: QTableWidget (Line / Pattern / Value),
      скрыта по умолчанию, появляется после Leak Scan
    - Статусбар: live-статистика словаря (names / strings / comments / includes / scrub)
    - Титл окна: имя файла словаря + маркер `*` при несохранённых изменениях
    - `gui.pro` теперь использует `QT = core gui widgets`
