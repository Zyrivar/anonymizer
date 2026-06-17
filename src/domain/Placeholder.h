// domain/Placeholder.h — генерация и распознавание плейсхолдеров.
// Инкапсулирует формат "PREFIX_NNNN" и счётчики по префиксам.
#pragma once
#include <string>
#include <map>
#include <regex>

namespace domain {

// Префиксы плейсхолдеров для разных категорий и типов чувствительных данных.
namespace prefix {
    inline constexpr const char* Name    = "ID";
    inline constexpr const char* String  = "STR";
    inline constexpr const char* Comment = "CMT";
    inline constexpr const char* Include = "HDR";
    // Префиксы для scrub-замен (format-режим)
    inline constexpr const char* Url     = "URL";
    inline constexpr const char* Email   = "EMAIL";
    inline constexpr const char* Ip      = "IP";
    inline constexpr const char* Mac     = "MAC";
    inline constexpr const char* Path    = "PATH";
    inline constexpr const char* Host    = "HOST";
}

// Фабрика плейсхолдеров: хранит счётчики по префиксам и выдаёт следующий
// уникальный плейсхолдер вида "ID_0001". Также умеет распознавать плейсхолдеры,
// чтобы не анонимизировать их повторно.
class PlaceholderFactory {
public:
    // Следующий плейсхолдер для данного префикса: "ID" → "ID_0001", "ID_0002", ...
    std::string next(const std::string& pfx) {
        int n = ++counters_[pfx];
        return format(pfx, n);
    }

    // Зарегистрировать уже существующий номер (при загрузке словаря),
    // чтобы счётчик продолжился с правильного места.
    void observe(const std::string& pfx, int n) {
        int& c = counters_[pfx];
        if (n > c) c = n;
    }

    // Текущее значение счётчика для префикса (для восстановления состояния).
    int counter(const std::string& pfx) const {
        auto it = counters_.find(pfx);
        return it == counters_.end() ? 0 : it->second;
    }

    // Является ли строка плейсхолдером вида "PREFIX_NNNN"?
    static bool isPlaceholder(const std::string& s) {
        static const std::regex re(
            "^(ID|STR|CMT|HDR|IP|PATH|HOST|MAC|EMAIL|URL)_[0-9]{4,}$");
        return std::regex_match(s, re);
    }

    // Форматирование: ("ID", 1) → "ID_0001".
    static std::string format(const std::string& pfx, int n) {
        std::string num = std::to_string(n);
        while (num.size() < 4) num.insert(num.begin(), '0');
        return pfx + "_" + num;
    }

private:
    std::map<std::string, int> counters_;
};

} // namespace domain
