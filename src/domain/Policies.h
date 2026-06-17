// domain/Policies.h — доменные политики: список сохраняемых имён и наборы паттернов.
// Вынесены в отдельные классы, чтобы их можно было конфигурировать/подменять
// без правки логики анонимизации.
#pragma once
#include "Placeholder.h"
#include <string>
#include <set>
#include <vector>
#include <regex>

namespace domain {

// Список имён, которые НЕ анонимизируются (библиотечные/общеупотребительные).
// Сейчас захардкожен, но инкапсуляция позволяет позже грузить из конфига.
class PreserveList {
public:
    PreserveList() : names_{
        "std","vector","string","size_t","quint16","nullptr","cout","endl","printf",
        "QString","QTcpSocket","qDebug","main","static_cast",
        "size","length","c_str","data","empty","clear","begin","end","at",
        "push_back","pop_back","front","back","insert","erase","find","substr","resize",
    } {}

    bool contains(const std::string& name) const {
        return names_.count(name) > 0;
    }

    void add(const std::string& name) { names_.insert(name); }

private:
    std::set<std::string> names_;
};

// Именованный паттерн: префикс плейсхолдера + регулярное выражение.
struct NamedPattern {
    std::string prefix;
    std::regex  regex;
};

// Паттерны для scrub-режима (затирание чувствительных подстрок в строках).
// Регэкспы компилируются один раз (static внутри).
class ScrubPatterns {
public:
    const std::vector<NamedPattern>& all() const {
        static const std::vector<NamedPattern> pats = {
            {prefix::Url,   std::regex(R"(https?://[^\s"']+)")},
            {prefix::Email, std::regex(R"([\w.+-]+@[\w-]+\.[\w.-]+)")},
            {prefix::Ip,    std::regex(R"((?:\d{1,3}\.){3}\d{1,3})")},
            {prefix::Mac,   std::regex(R"((?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})")},
            {prefix::Path,  std::regex(R"([A-Za-z]:\\[^\s"']+|(?:/[\w.-]+){2,})")},
            {prefix::Host,  std::regex(R"((?:[a-zA-Z0-9-]+\.)+[a-zA-Z]{2,})")},
        };
        return pats;
    }
};

// Паттерны для аудита утечек (полнота важнее точности).
class LeakPatterns {
public:
    const std::vector<NamedPattern>& all() const {
        static const std::vector<NamedPattern> pats = {
            {"IPv4",     std::regex(R"((?:\d{1,3}\.){3}\d{1,3})")},
            {"IPv6",     std::regex(R"(\bfe80::[0-9A-Fa-f:]+|\b(?:[0-9A-Fa-f]{1,4}:){4,7}[0-9A-Fa-f]{1,4}\b)")},
            {"MAC",      std::regex(R"((?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})")},
            {"EMAIL",    std::regex(R"([\w.+-]+@[\w-]+\.[\w.-]+)")},
            {"URL",      std::regex(R"(https?://[^\s"']+)")},
            {"PATH",     std::regex(R"([A-Za-z]:\\[^\s"']+|(?:/[\w.-]+){2,})")},
            {"HOST",     std::regex(R"((?:[a-zA-Z0-9-]+\.)+[a-zA-Z]{2,})")},
            {"DIGITS6+", std::regex(R"(\b\d{6,}\b)")},
        };
        return pats;
    }
};

} // namespace domain
