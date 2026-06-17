// application/LeakAuditor.cpp — реализация use-case аудита утечек.
#include "LeakAuditor.h"
#include "domain/Placeholder.h"
#include <regex>

namespace application {

using domain::Category;
using domain::Leak;
using domain::PlaceholderFactory;

std::vector<Leak> LeakAuditor::audit(const std::string& source) {
    std::vector<Leak> out;
    auto parsed = parser_.parse(source);

    for (const auto& h : parsed.hits) {
        if (h.category != Category::String && h.category != Category::Comment)
            continue;

        for (const auto& pat : patterns_.all()) {
            for (auto it = std::sregex_iterator(h.text.begin(), h.text.end(), pat.regex),
                      e = std::sregex_iterator(); it != e; ++it) {
                std::string val = it->str();
                if (PlaceholderFactory::isPlaceholder(val)) continue;

                // Номер строки = число '\n' до начала находки.
                int line = 1;
                for (uint32_t i = 0; i < h.start && i < source.size(); ++i)
                    if (source[i] == '\n') ++line;

                out.push_back({ line, pat.prefix, val });
            }
        }
    }
    return out;
}

} // namespace application
