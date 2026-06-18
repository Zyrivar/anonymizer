// infrastructure/LanguageProfiles.h — реестр доступных профилей языков.
// Единая точка, через которую composition root и GUI получают профиль, не зная
// о конкретных классах-наследниках LanguageProfile.
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace infrastructure {

class LanguageProfile;

// Профиль по стабильному id ("cpp"); nullptr, если язык не зарегистрирован.
std::shared_ptr<const LanguageProfile> languageProfile(const std::string& id);

// Профиль по расширению файла без точки или с точкой ("cpp", ".h" → C++);
// nullptr, если расширение не сопоставлено ни одному языку.
std::shared_ptr<const LanguageProfile>
languageProfileForExtension(const std::string& ext);

// Все зарегистрированные профили (для построения меню выбора языка в GUI).
std::vector<std::shared_ptr<const LanguageProfile>> allLanguageProfiles();

} // namespace infrastructure
