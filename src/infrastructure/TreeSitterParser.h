// infrastructure/TreeSitterParser.h — реализация IParser на tree-sitter-cpp.
#pragma once
#include "domain/IParser.h"
#include "domain/Policies.h"

namespace infrastructure {

// Парсер исходников C++ на основе вендоренного tree-sitter-cpp.
// Вся работа с C-API tree-sitter и RAII-управление ресурсами скрыты в .cpp.
class TreeSitterParser : public domain::IParser {
public:
    // Принимает список сохраняемых имён: идентификаторы из него не попадают
    // в находки категории Name (их не нужно анонимизировать).
    explicit TreeSitterParser(domain::PreserveList preserve = {});

    domain::ParseOutput parse(const std::string& source) override;

private:
    domain::PreserveList preserve_;
};

} // namespace infrastructure
