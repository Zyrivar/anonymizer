// infrastructure/PythonLanguageProfile.h — профиль языка Python
// (грамматика tree-sitter-python). Прототип многоязычной поддержки.
#pragma once
#include "LanguageProfile.h"

namespace infrastructure {

class PythonLanguageProfile : public LanguageProfile {
public:
    std::string id() const override { return "python"; }
    std::string displayName() const override { return "Python"; }
    std::vector<std::string> fileExtensions() const override;

    const TSLanguage* tsLanguage() const override;
    domain::PreserveList defaultPreserveList() const override;

    const std::set<std::string>& commentTypes() const override;
    const std::set<std::string>& identifierTypes() const override;
    const std::set<std::string>& stringTypes() const override;
    const std::set<std::string>& skipTypes() const override;

    StringRole classifyString(const std::string& parentType) const override;

    // f-string обрабатывается посегментно: текст → StringFragment, выражения
    // {…} → обычные имена. Иначе имена внутри {…} утекают / ломаются скрабом.
    bool descendIntoString(TSNode node, const std::string& src) const override;
    const std::set<std::string>& stringFragmentTypes() const override;

    // collectExternNames наследует пустую реализацию базы: в Python нет
    // понятия внешнего связывания (extern).
};

} // namespace infrastructure
