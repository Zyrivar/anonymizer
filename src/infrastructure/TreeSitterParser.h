// infrastructure/TreeSitterParser.h — обобщённый парсер исходников на tree-sitter.
#pragma once
#include "domain/IParser.h"
#include "domain/Policies.h"
#include <memory>

namespace infrastructure {

class LanguageProfile; // грамматика + правила классификации узлов конкретного языка

// Парсер на основе tree-sitter. Сам по себе языко-независимый драйвер: грамматика
// и правила отображения узлов на категории вынесены в LanguageProfile, поэтому
// один и тот же класс работает с любым поддержанным языком (раньше был жёстко
// привязан к C++). Реализация устойчива к синтаксическим ошибкам (фрагменты кода).
class TreeSitterParser : public domain::IParser {
public:
    // profile — язык (грамматика + классификация узлов), не может быть null.
    // preserve — имена, которые не анонимизируются (не попадают в Category::Name).
    explicit TreeSitterParser(std::shared_ptr<const LanguageProfile> profile,
                              domain::PreserveList preserve = {});

    domain::ParseOutput parse(const std::string& source) override;

private:
    std::shared_ptr<const LanguageProfile> profile_;
    domain::PreserveList                   preserve_;
};

} // namespace infrastructure
