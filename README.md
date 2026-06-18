# C++ Anonymizer

[![CI](https://github.com/Zyrivar/anonymizer/actions/workflows/ci.yml/badge.svg)](https://github.com/Zyrivar/anonymizer/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Обратимый статический анонимизатор исходного кода — **C++** и **Python**.
Заменяет имена, строковые литералы, комментарии, user-инклуды и (опционально)
чувствительные подстроки на плейсхолдеры — чтобы безопасно отдавать
проприетарный код внешним помощникам (LLM, консультантам), а затем восстановить
как сам код, так и ответ помощника, написанный в терминах плейсхолдеров.

Движок парсинга — **tree-sitter** (грамматики C++ и Python завендорены в
`third_party/`, отдельный компилятор грамматик не нужен). Языки подключаются
через **профили** — добавить новый можно без правки ядра (см.
[«Поддержка языков»](#поддержка-языков)). Стек: **Qt5, C++17, qmake**.

### Ключевые инварианты

- **Точный round-trip:** `restoreCode(anonymize(x)) == x`.
- **Консистентность:** один символ → один ID на весь проект (между файлами).
- **Идемпотентность:** плейсхолдеры не анонимизируются повторно.

---

## Зачем это

Вы хотите спросить у LLM про кусок проприетарного кода, но не можете отдать
наружу реальные имена, IP-адреса, пути и хосты. Воркфлоу:

1. **Anonymize** — код превращается в безопасную версию с плейсхолдерами
   (`ID_0001`, `STR_0007`, `IP_0002`…). Соответствия пишутся в локальный словарь.
2. Отдаёте обезличенный код помощнику, получаете ответ в тех же плейсхолдерах.
3. **Reverse Text** — восстанавливаете ответ обратно в ваши реальные имена.
4. **Leak Scan** — перед отправкой проверяете, что наружу не утекло ничего лишнего.

Словарь и leak-отчёт **никогда не покидают вашу машину** — это секрет.

---

## Поддержка языков

Поддерживаются **C / C++** и **Python**. Язык исходника выбирается в GUI
(комбобокс «Язык исходника» на тулбаре) или программно — передачей профиля
в `AnonymizerService`.

Вся специфика языка вынесена в `infrastructure::LanguageProfile`: грамматика
tree-sitter, имена типов узлов (что считать именем / строкой / комментарием /
инклудом), сбор `extern`-связывания и preserve-список (builtins/stdlib, не
подлежащие анонимизации). Парсер `TreeSitterParser` — языко-независимый драйвер
поверх профиля; доменный слой и use-case'ы про конкретный язык не знают.

| Язык   | Профиль                 | Особенности                                  |
|--------|-------------------------|----------------------------------------------|
| C/C++  | `CppLanguageProfile`    | `#include`, `extern "C"`, `// ` и `/* */`    |
| Python | `PythonLanguageProfile` | комментарии `#`, f-string с интерполяцией, без extern, полный preserve builtins/stdlib |

**Добавить язык** — три шага, без изменений в ядре:

1. завендорить грамматику в `third_party/` и подключить её `parser.c`/`scanner.c`
   в `common.pri`;
2. написать профиль (наследник `LanguageProfile`) с типами узлов и preserve-листом;
3. зарегистрировать его в `infrastructure/LanguageProfiles.cpp`.

> Round-trip-инвариант и формат плейсхолдеров — общие для всех языков; стиль
> комментария (`#` / `//` / `/* */`) подбирается автоматически по исходнику.

---

## Сборка

Проверено на **Qt 5.15.2 / MinGW 8.1.0 64-bit** (Windows).

```sh
qmake
mingw32-make            # или make / nmake / jom
mingw32-make check      # собрать и прогнать оба набора тестов
```

Собираются три цели:

| Цель        | Что это                                   |
|-------------|-------------------------------------------|
| `anonctl`   | GUI-приложение (Qt5 Widgets)              |
| `test_unit` | юнит-тесты доменного ядра (32 проверки)   |
| `test_e2e`  | end-to-end тесты, C++ и Python (108 проверок) |

**Qt Creator:** открыть `anonymizer.pro`, кит «Desktop Qt 5.15.2 MinGW 64-bit».
`test_unit` и `test_e2e` доступны как отдельные run-цели в селекторе.

**windeployqt** встроен в `gui.pro` как post-build шаг: копирует Qt DLL и
`platforms/qwindows.dll` рядом с `anonctl.exe`, иначе exe не стартует вне Creator.

**Готовая сборка:** CI на каждый push собирает автономный `anonctl.exe`
(Qt DLL + плагины + рантайм MinGW) и публикует его артефактом
`anonctl-windows-x64` — скачать можно во вкладке **Actions** соответствующего
запуска.

> Пути сборки — без кириллицы и пробелов (MinGW плохо их переваривает).

---

## Архитектура (Clean Architecture)

Зависимости направлены строго внутрь:
`presentation (gui) → application → domain ← infrastructure`.
Доменный слой не зависит ни от Qt, ни от tree-sitter, ни от nlohmann/json.

```
src/
  domain/             — сущности и порты (без внешних зависимостей)
    Types.h             Category, Hit, Leak (value-объекты)
    Placeholder.h       PlaceholderFactory — генерация/распознавание "ID_0001"
    Dictionary.h        агрегат словаря + порт IDictionaryStore
    IParser.h           порт парсера
    EditApplier.h       применение замен к тексту (справа налево)
    Policies.h          PreserveList, ScrubPatterns, LeakPatterns
  application/        — use-cases (оркестрация поверх портов)
    Anonymizer.{h,cpp}      прямая анонимизация (forward)
    Deanonymizer.{h,cpp}    восстановление: restoreTokens / restoreText
    LeakAuditor.{h,cpp}     аудит утечек
    AnonymizerService.h     фасад + composition root (ручной DI)
  infrastructure/     — реализации портов
    TreeSitterParser.{h,cpp}        IParser: языко-независимый драйвер tree-sitter
    LanguageProfile.h               абстракция языка (узлы, preserve, extern)
    CppLanguageProfile.{h,cpp}      профиль C/C++
    PythonLanguageProfile.{h,cpp}   профиль Python
    LanguageProfiles.{h,cpp}        реестр профилей (id/расширение → профиль)
    JsonFileDictionaryStore.{h,cpp} IDictionaryStore на JSON-файле
gui/                  — presentation (Qt5): mainwindow.{h,cpp}, main_gui.cpp
tests/  unit/ e2e/    — юнит- и end-to-end тесты
third_party/          — tree-sitter, tree-sitter-cpp, tree-sitter-python,
                        nlohmann/json (vendored)
```

### Порты и адаптеры

Интерфейсы введены **только там, где есть реальная альтернатива реализации**:

- **IParser** — `TreeSitterParser` (драйвер) + `LanguageProfile` на язык;
  альтернативы: libclang (точнее различает члены библиотечных типов) или
  лёгкий regex-парсер.
- **IDictionaryStore** — сейчас `JsonFileDictionaryStore`; альтернативы:
  in-memory для тестов, SQLite для больших словарей.

Use-cases и value-объекты — конкретные классы; спекулятивная абстракция не вводилась.

---

## GUI (`anonctl`)

Двухпанельное Qt5-окно над фасадом `AnonymizerService`:

- **Две панели:** исходный код ↔ анонимизированный/восстановленный результат
- **Тулбар:** Forward · Reverse · Reverse Text · Leak Scan · язык исходника (C++ / Python) · выбор режима строк (opaque / format)
- **Меню Dictionary:** New / Load / Save / Save As (JSON через `IDictionaryStore`)
- **Меню Clipboard:** анонимизировать (`Ctrl+Shift+A`) / деанонимизировать (`Ctrl+Shift+D`) буфер
- **Leak Report:** док-панель с таблицей утечек (Line / Pattern / Value)
- **Статусбар:** live-статистика словаря; в заголовке — имя словаря и маркер `*` несохранённых изменений
- **Batch:** File → Batch process (входная и выходная директории); по завершении
  словарь автосохраняется (в свой файл либо `anonymizer_dict.json` в каталоге
  вывода) — без него результат необратим. Защита от записи «на месте».
- **Язык исходника:** комбобокс на тулбаре — C++ (по умолчанию) / Python; при смене
  сервис пересоздаётся с нужным профилем, словарь сохраняется; выбор — в `QSettings`
- **Язык интерфейса:** меню Language — Русский (по умолчанию) / English, переключение
  «на лету» (Qt i18n, переводы встроены в ресурсы); выбор сохраняется в `QSettings`

---

## Использование (программно)

```cpp
#include "application/AnonymizerService.h"

application::AnonymizerService svc;   // composition root: парсер + use-cases (C++)
domain::Dictionary dict;

// Другой язык — передаём профиль из реестра (preserve-лист берётся из профиля):
application::AnonymizerService py(infrastructure::languageProfile("python"));

// Анонимизация (Opaque — непрозрачные плейсхолдеры; Format — с маскированием IP/MAC/…)
std::string anon = svc.anonymize(source, dict, application::StringMode::Opaque);

// Восстановление кода (точный round-trip) или произвольного текста (ответа LLM)
std::string code = svc.restoreCode(anon, dict);
std::string text = svc.restoreText(llmAnswer, dict);

// Аудит утечек
std::vector<domain::Leak> leaks = svc.audit(source);

// Сохранение / загрузка словаря
infrastructure::JsonFileDictionaryStore store;
store.save("dict.json", dict);
domain::Dictionary loaded = store.load("dict.json");
```

---

## Плейсхолдеры

`ID_*` имена · `STR_*` строки · `CMT_*` комментарии · `HDR_*` заголовки.
Скраб (format-режим): `IP_* · PATH_* · HOST_* · MAC_* · EMAIL_* · URL_*`.

---

## Тесты

```sh
mingw32-make check      # собирает и гоняет оба набора
```

- **test_unit** (32 проверки) — use-cases через фасад: round-trip, идемпотентность,
  preserve-list (в т.ч. внешний инжектируемый), extern-трекинг, leak-детекция,
  format-режим, JSON round-trip словаря.
- **test_e2e** (108 проверок) — реалистичный SCADA-исходник C++ ~335 строк через полный
  конвейер: анонимизация, восстановление, аудит утечек, межфайловая консистентность;
  плюс два блока Python: базовый (round-trip, устранение данных, preserve builtins,
  `#`-комментарии, изоляция профилей) и production (f-strings, async, декораторы,
  comprehensions, match — безопасность интерполяции в обоих режимах).

Ассерты без фреймворка (макросы `EXPECT_EQ` / `EXPECT_TRUE`).

---

## Безопасность

Словарь содержит **оригиналы** — это секрет. Код отдаём наружу, словарь и
leak-report держим локально. Перед отправкой прогоняем **Leak Scan**.

---

## Ограничения

- Компилируемость вывода не гарантируется (требование снято осознанно).
- «Свой символ vs библиотечный» — по curated preserve-list (профиль языка),
  не семантически. Точное различение членов библиотечных типов дал бы libclang
  (порт `IParser` для этого и введён).
- Скраб-регэкспы эвристичны (особенно `HOST`) — дотачиваются под конкретные данные.
- Языки — C/C++ и Python. Python: полный preserve-лист builtins/stdlib/typing,
  **f-string обрабатывается посегментно** (текст → строки, выражения `{…}` →
  имена; безопасно в обоих режимах). Импорты — обычные имена (модули → `ID_*`).
  Тело интерполяции f-string в opaque-режиме разбивается на сегменты `STR_*`;
  редкие случаи вроде вложенных кавычек в f-string ≤3.11 — на усмотрение грамматики.
  Форматы данных вне scope.
- `restoreText` имеет сложность O(N×M) на проход × размер словаря; при больших
  словарях стоит перейти на Aho-Corasick (отмечено как TODO).

---

## История

Проект прошёл путь: code review + багфиксы монолитного ядра → Qt5 GUI →
**рефакторинг в Clean Architecture** (монолит `anon_core` разбит на
domain/application/infrastructure, CLI удалён, точка входа — только GUI) →
локализация UI (ru/en) → **многоязычность** (профили языков; Python с
f-string-интерполяцией и полным preserve-листом).

Детальная история — в `git log`.

---

## Лицензия

[MIT](LICENSE). Сторонние компоненты в `third_party/` (tree-sitter,
tree-sitter-cpp, tree-sitter-python, nlohmann/json) — под собственными
MIT-лицензиями.
