// infrastructure/LanguageProfiles.cpp — реализация реестра профилей языков.
#include "LanguageProfiles.h"
#include "LanguageProfile.h"
#include "CppLanguageProfile.h"
#include "PythonLanguageProfile.h"
#include <algorithm>
#include <cctype>

namespace infrastructure {
namespace {

// Реестр доступных языков. Сейчас только C++. Чтобы добавить язык — создать
// профиль (наследник LanguageProfile) и добавить его экземпляр в этот список.
const std::vector<std::shared_ptr<const LanguageProfile>>& registry() {
    static const std::vector<std::shared_ptr<const LanguageProfile>> profiles = {
        std::make_shared<CppLanguageProfile>(),
        std::make_shared<PythonLanguageProfile>(),
    };
    return profiles;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // анонимное пространство имён

std::shared_ptr<const LanguageProfile> languageProfile(const std::string& id) {
    for (const auto& p : registry())
        if (p->id() == id) return p;
    return nullptr;
}

std::shared_ptr<const LanguageProfile>
languageProfileForExtension(const std::string& ext) {
    std::string e = toLower(ext);
    if (!e.empty() && e.front() == '.') e.erase(0, 1);
    for (const auto& p : registry())
        for (const auto& x : p->fileExtensions())
            if (x == e) return p;
    return nullptr;
}

std::vector<std::shared_ptr<const LanguageProfile>> allLanguageProfiles() {
    return registry();
}

} // namespace infrastructure
