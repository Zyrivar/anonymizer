// application/Anonymizer.h — use-case прямой анонимизации (forward).
#pragma once
#include "domain/IParser.h"
#include "domain/Dictionary.h"
#include "domain/Placeholder.h"
#include "domain/Policies.h"
#include <string>

namespace application {

// Режим обработки строковых литералов.
enum class StringMode {
    Opaque,  // строка целиком → "STR_0001"
    Format   // структура сохраняется, затираются лишь чувствительные подстроки
};

// Use-case: анонимизирует исходный код, расширяя словарь на месте.
// Зависит только от порта IParser — не знает про tree-sitter.
class Anonymizer {
public:
    Anonymizer(domain::IParser& parser) : parser_(parser) {}

    // Анонимизировать source, дополняя dict. Возвращает анонимизированный текст.
    std::string run(const std::string& source,
                    domain::Dictionary& dict,
                    StringMode mode = StringMode::Opaque);

private:
    // Восстанавливает счётчики плейсхолдеров из существующего словаря,
    // чтобы повторные вызовы продолжали нумерацию без коллизий.
    void primeCounters(const domain::Dictionary& dict);

    // Затирает чувствительные подстроки в строковом литерале (format-режим).
    std::string scrubString(const std::string& literal, domain::Dictionary& dict);

    domain::IParser&            parser_;
    domain::PlaceholderFactory  factory_;
    domain::ScrubPatterns       scrubPatterns_;
};

} // namespace application
