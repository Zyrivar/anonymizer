// application/Deanonymizer.h — use-case обратного восстановления.
#pragma once
#include "domain/IParser.h"
#include "domain/Dictionary.h"
#include <string>

namespace application {

// Use-case деанонимизации. Две стратегии:
//   - restoreTokens: точное потокенное восстановление файла кода
//                    (reverse(forward(x)) == x), требует парсинга.
//   - restoreText:   восстановление произвольного текста (проза + код),
//                    например ответа помощника из буфера обмена.
class Deanonymizer {
public:
    Deanonymizer(domain::IParser& parser) : parser_(parser) {}

    // Точное восстановление анонимизированного исходника по токенам.
    std::string restoreTokens(const std::string& source,
                              const domain::Dictionary& dict);

    // Восстановление произвольного текста с плейсхолдерами.
    // Неизвестные плейсхолдеры остаются нетронутыми.
    std::string restoreText(const std::string& text,
                            const domain::Dictionary& dict);

private:
    domain::IParser& parser_;
};

} // namespace application
