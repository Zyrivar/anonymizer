// domain/IParser.h — порт (интерфейс) парсера исходного кода.
//
// Реальная альтернатива реализаций оправдывает наличие интерфейса:
//   - TreeSitterParser  (текущая реализация на tree-sitter-cpp)
//   - libclang-парсер   (точнее в различении членов библиотечных типов)
//   - regex-парсер      (легковесный fallback без внешних зависимостей)
// Прикладной слой зависит только от этого порта, а не от tree-sitter.
#pragma once
#include "Types.h"
#include <string>
#include <vector>
#include <set>

namespace domain {

// Результат разбора исходника: находки (hits) и множество extern-имён.
struct ParseOutput {
    std::vector<Hit>      hits;        // все сущности, подлежащие обработке
    std::set<std::string> externNames; // имена с внешним связыванием (extern "C", extern)
};

// Порт парсера. Реализация обязана быть устойчивой к синтаксическим ошибкам
// (поддержка фрагментов кода), т.к. инструмент работает и с неполными сниппетами.
class IParser {
public:
    virtual ~IParser() = default;

    // Разобрать исходник и вернуть находки. Бросает std::runtime_error,
    // если разбор невозможен (например, ошибка выделения ресурсов парсера).
    virtual ParseOutput parse(const std::string& source) = 0;
};

} // namespace domain
