// application/LeakAuditor.h — use-case аудита потенциальных утечек.
#pragma once
#include "domain/IParser.h"
#include "domain/Policies.h"
#include "domain/Types.h"
#include <string>
#include <vector>

namespace application {

// Use-case: сканирует строковые и комментарийные литералы исходника
// набором паттернов и возвращает всё, что выглядит чувствительным.
class LeakAuditor {
public:
    LeakAuditor(domain::IParser& parser) : parser_(parser) {}

    std::vector<domain::Leak> audit(const std::string& source);

private:
    domain::IParser&      parser_;
    domain::LeakPatterns  patterns_;
};

} // namespace application
