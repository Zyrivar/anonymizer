// domain/Types.h — базовые value-объекты доменного слоя.
// Слой domain не зависит ни от Qt, ни от tree-sitter, ни от nlohmann/json.
#pragma once
#include <string>
#include <cstdint>

namespace domain {

// Категория лексической сущности, которую анонимизатор различает в исходнике.
enum class Category {
    Name,      // идентификатор (имя переменной/типа/функции)
    String,    // строковый литерал
    Include,   // путь в #include "..."
    Comment    // комментарий (// или /* */)
};

// Диапазон байтов в исходнике [start, end) с привязанной категорией и текстом.
// Это «находка» (hit) — то, что парсер извлёк из дерева и что подлежит замене.
struct Hit {
    uint32_t  start = 0;
    uint32_t  end   = 0;
    Category  category = Category::Name;
    std::string text;

    uint32_t length() const { return end - start; }
};

// Потенциальная утечка чувствительных данных, найденная аудитором.
struct Leak {
    int         line = 0;       // 1-индексированный номер строки
    std::string pattern;        // имя сработавшего паттерна (IPv4, MAC, EMAIL, ...)
    std::string value;          // конкретное найденное значение

    bool operator==(const Leak& o) const {
        return line == o.line && pattern == o.pattern && value == o.value;
    }
};

} // namespace domain
