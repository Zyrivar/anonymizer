// application/AnonymizerService.h — фасад, связывающий use-cases воедино.
//
// Composition root уровня приложения: создаёт парсер и три use-case, предоставляет
// единую точку входа для presentation-слоя и тестов. Сам не содержит логики —
// только проводку зависимостей (Dependency Injection вручную).
#pragma once
#include "application/Anonymizer.h"
#include "application/Deanonymizer.h"
#include "application/LeakAuditor.h"
#include "domain/Policies.h"
#include "infrastructure/TreeSitterParser.h"
#include <memory>
#include <utility>

namespace application {

class AnonymizerService {
public:
    // Конструктор с внешним preserve-списком (DI-шов): список сохраняемых имён
    // больше не зашит в composition root — его можно задать из GUI, теста или
    // загрузить из конфига и передать сюда.
    explicit AnonymizerService(domain::PreserveList preserve)
        : parser_(std::make_unique<infrastructure::TreeSitterParser>(std::move(preserve)))
        , anonymizer_(*parser_)
        , deanonymizer_(*parser_)
        , auditor_(*parser_)
    {}

    // По умолчанию — встроенный preserve-список (библиотечные/общие имена).
    AnonymizerService() : AnonymizerService(domain::PreserveList{}) {}

    std::string anonymize(const std::string& src, domain::Dictionary& dict,
                          StringMode mode = StringMode::Opaque) {
        return anonymizer_.run(src, dict, mode);
    }

    std::string restoreCode(const std::string& src, const domain::Dictionary& dict) {
        return deanonymizer_.restoreTokens(src, dict);
    }

    std::string restoreText(const std::string& text, const domain::Dictionary& dict) {
        return deanonymizer_.restoreText(text, dict);
    }

    std::vector<domain::Leak> audit(const std::string& src) {
        return auditor_.audit(src);
    }

private:
    std::unique_ptr<domain::IParser> parser_;
    Anonymizer    anonymizer_;
    Deanonymizer  deanonymizer_;
    LeakAuditor   auditor_;
};

} // namespace application
