// application/Anonymizer.cpp — реализация use-case прямой анонимизации.
#include "Anonymizer.h"
#include "domain/EditApplier.h"
#include <regex>

namespace application {

using domain::Category;
using domain::Edit;
using domain::NameEntry;
using domain::PlaceholderFactory;
namespace pfx = domain::prefix;

void Anonymizer::primeCounters(const domain::Dictionary& dict) {
    // Имена: префикс ID
    for (const auto& [orig, entry] : dict.names()) {
        std::smatch m;
        static const std::regex re("[0-9]{4,}");
        if (std::regex_search(entry.placeholder, m, re))
            factory_.observe(pfx::Name, std::stoi(m.str()));
    }
    // Строки / комментарии / инклуды: извлекаем номер из плейсхолдера
    auto primeFrom = [&](const std::map<std::string, std::string>& sec,
                         const char* prefix) {
        static const std::regex re("[0-9]{4,}");
        for (const auto& [orig, ph] : sec) {
            std::smatch m;
            if (std::regex_search(ph, m, re))
                factory_.observe(prefix, std::stoi(m.str()));
        }
    };
    primeFrom(dict.strings(),  pfx::String);
    primeFrom(dict.comments(), pfx::Comment);
    primeFrom(dict.includes(), pfx::Include);

    // Scrub-счётчики: префикс зашит в начале значения ("IP_0001")
    for (const auto& [value, ph] : dict.scrub()) {
        auto u = ph.find('_');
        if (u == std::string::npos) continue;
        std::string prefix = ph.substr(0, u);
        try {
            factory_.observe(prefix, std::stoi(ph.substr(u + 1)));
        } catch (...) { /* некорректный формат — пропускаем */ }
    }
}

std::string Anonymizer::scrubString(const std::string& literal,
                                    domain::Dictionary& dict) {
    std::string s = literal;
    for (const auto& pat : scrubPatterns_.all()) {
        std::string res;
        size_t last = 0;
        for (auto it = std::sregex_iterator(s.begin(), s.end(), pat.regex),
                  e = std::sregex_iterator(); it != e; ++it) {
            auto m = *it;
            size_t st = m.position(), en = st + m.length();
            std::string val = m.str();
            res.append(s, last, st - last);

            auto found = dict.scrub().find(val);
            std::string ph;
            if (found != dict.scrub().end()) {
                ph = found->second;
            } else {
                ph = factory_.next(pat.prefix);
                dict.scrub()[val] = ph;
            }
            res += ph;
            last = en;
        }
        res.append(s, last, s.size() - last);
        s = res;
    }
    return s;
}

std::string Anonymizer::run(const std::string& source,
                            domain::Dictionary& dict,
                            StringMode mode) {
    factory_ = PlaceholderFactory{};  // свежие счётчики на каждый вызов
    primeCounters(dict);

    auto parsed = parser_.parse(source);

    std::vector<Edit> edits;
    edits.reserve(parsed.hits.size());

    for (const auto& h : parsed.hits) {
        std::string replacement;

        switch (h.category) {
        case Category::Name: {
            if (PlaceholderFactory::isPlaceholder(h.text)) continue;
            bool isExtern = parsed.externNames.count(h.text) > 0;
            auto it = dict.names().find(h.text);
            if (it == dict.names().end()) {
                NameEntry e{ factory_.next(pfx::Name), isExtern };
                dict.names()[h.text] = e;
                replacement = e.placeholder;
            } else {
                if (isExtern) it->second.isExtern = true;
                replacement = it->second.placeholder;
            }
            break;
        }
        case Category::String: {
            if (mode == StringMode::Format) {
                replacement = scrubString(h.text, dict);
            } else {
                auto it = dict.strings().find(h.text);
                if (it == dict.strings().end()) {
                    std::string ph = "\"" + factory_.next(pfx::String) + "\"";
                    dict.strings()[h.text] = ph;
                    replacement = ph;
                } else {
                    replacement = it->second;
                }
            }
            break;
        }
        case Category::Include: {
            auto it = dict.includes().find(h.text);
            if (it == dict.includes().end()) {
                std::string ph = "\"" + factory_.next(pfx::Include) + ".h\"";
                dict.includes()[h.text] = ph;
                replacement = ph;
            } else {
                replacement = it->second;
            }
            break;
        }
        case Category::Comment: {
            auto it = dict.comments().find(h.text);
            if (it == dict.comments().end()) {
                // Сохраняем стиль комментария исходного языка по его префиксу,
                // чтобы анонимизированный текст оставался валидным и при повторном
                // разборе снова распознавался как комментарий (нужно для round-trip).
                std::string body = factory_.next(pfx::Comment);
                std::string ph;
                if (h.text.rfind("#", 0) == 0)        ph = "# " + body;          // Python/шелл
                else if (h.text.rfind("//", 0) == 0)  ph = "// " + body;         // C++ строчный
                else                                  ph = "/* " + body + " */"; // C-блочный
                dict.comments()[h.text] = ph;
                replacement = ph;
            } else {
                replacement = it->second;
            }
            break;
        }
        }

        edits.push_back({h, replacement});
    }

    return domain::EditApplier::apply(source, edits);
}

} // namespace application
