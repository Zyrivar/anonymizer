// infrastructure/LanguageProfile.h — профиль языка программирования для парсера.
//
// Описывает, как узлы синтаксического дерева конкретной грамматики tree-sitter
// отображаются на категории анонимизации (имя / строка / комментарий / include),
// а также как собирать имена с внешним связыванием.
//
// Живёт в слое infrastructure, а НЕ в domain: профиль оперирует понятиями
// tree-sitter (грамматика, имена типов узлов), от которых доменный слой
// намеренно отвязан (см. инвариант в Types.h). Парсер (TreeSitterParser) —
// обобщённый драйвер; вся специфика языка вынесена сюда.
//
// Чтобы добавить язык: наследуют этот класс, реализуют методы под нужную
// грамматику и регистрируют экземпляр в LanguageProfiles.cpp.
#pragma once
#include "domain/Types.h"
#include "domain/Policies.h"
#include <tree_sitter/api.h>
#include <string>
#include <set>
#include <vector>

namespace infrastructure {

class LanguageProfile {
public:
    virtual ~LanguageProfile() = default;

    // --- Идентификация и выбор ---
    // Короткий стабильный id ("cpp", "python", ...) — для настроек и реестра.
    virtual std::string id() const = 0;
    // Человекочитаемое имя для UI ("C / C++", "Python").
    virtual std::string displayName() const = 0;
    // Расширения файлов без точки ("cpp","h",...) — для авто-выбора по файлу.
    virtual std::vector<std::string> fileExtensions() const = 0;

    // tree-sitter грамматика языка.
    virtual const TSLanguage* tsLanguage() const = 0;

    // Имена, не подлежащие анонимизации по умолчанию для этого языка
    // (builtins/stdlib/общеупотребительные). Может быть переопределён извне
    // через DI-шов AnonymizerService.
    virtual domain::PreserveList defaultPreserveList() const = 0;

    // --- Классификация узлов (по имени типа узла грамматики) ---
    // Узлы-комментарии → Category::Comment (вглубь не идём).
    virtual const std::set<std::string>& commentTypes() const = 0;
    // Узлы-идентификаторы → кандидаты в Category::Name (с учётом preserve-листа).
    virtual const std::set<std::string>& identifierTypes() const = 0;
    // Узлы-строки → Category::String/Include (уточняется classifyString).
    virtual const std::set<std::string>& stringTypes() const = 0;
    // Узлы, которые целиком пропускаются (не находка и не обходятся вглубь):
    // напр. символьные литералы и системные <...>-инклуды в C++.
    virtual const std::set<std::string>& skipTypes() const = 0;

    // Роль строкового узла, уточнённая по типу его родителя.
    enum class StringRole { String, Include, Skip };
    virtual StringRole classifyString(const std::string& parentType) const = 0;

    // Сбор имён с внешним связыванием (extern "C" и т.п.) — они помечаются в
    // словаре как extern. Понятие структурно зависит от языка; для языков без
    // него реализация по умолчанию ничего не добавляет.
    virtual void collectExternNames(TSNode /*root*/, const std::string& /*src*/,
                                    std::set<std::string>& /*out*/) const {}
};

} // namespace infrastructure
