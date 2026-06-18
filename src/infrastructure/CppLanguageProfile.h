// infrastructure/CppLanguageProfile.h — профиль языка C/C++ (грамматика
// tree-sitter-cpp). Инкапсулирует все специфичные для C++ имена типов узлов
// и структурный сбор extern-имён.
#pragma once
#include "LanguageProfile.h"

namespace infrastructure {

class CppLanguageProfile : public LanguageProfile {
public:
    std::string id() const override { return "cpp"; }
    std::string displayName() const override { return "C / C++"; }
    std::vector<std::string> fileExtensions() const override;

    const TSLanguage* tsLanguage() const override;
    domain::PreserveList defaultPreserveList() const override { return {}; }

    const std::set<std::string>& commentTypes() const override;
    const std::set<std::string>& identifierTypes() const override;
    const std::set<std::string>& stringTypes() const override;
    const std::set<std::string>& skipTypes() const override;

    StringRole classifyString(const std::string& parentType) const override;

    void collectExternNames(TSNode root, const std::string& src,
                            std::set<std::string>& out) const override;
};

} // namespace infrastructure
