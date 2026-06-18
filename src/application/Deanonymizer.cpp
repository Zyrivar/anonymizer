// application/Deanonymizer.cpp — реализация use-case деанонимизации.
#include "Deanonymizer.h"
#include "domain/EditApplier.h"
#include <map>
#include <regex>

namespace application {

using domain::Category;
using domain::Edit;

namespace {

// Заменить все вхождения подстроки (для restoreText).
void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

} // анонимное пространство имён

std::string Deanonymizer::restoreTokens(const std::string& source,
                                        const domain::Dictionary& dict) {
    // Обратные отображения: плейсхолдер → оригинал
    std::map<std::string, std::string> invNames, invStrings, invComments, invIncludes;
    for (const auto& [orig, e] : dict.names())    invNames[e.placeholder] = orig;
    for (const auto& [orig, v] : dict.strings())  invStrings[v]  = orig;
    for (const auto& [orig, v] : dict.comments()) invComments[v] = orig;
    for (const auto& [orig, v] : dict.includes()) invIncludes[v] = orig;

    auto parsed = parser_.parse(source);

    std::vector<Edit> edits;
    for (const auto& h : parsed.hits) {
        const std::map<std::string, std::string>* m =
              h.category == Category::Name    ? &invNames
            : h.category == Category::String  ? &invStrings
            : h.category == Category::StringFragment ? &invStrings
            : h.category == Category::Comment ? &invComments
            :                                   &invIncludes;
        auto it = m->find(h.text);
        if (it != m->end()) edits.push_back({h, it->second});
    }

    std::string out = domain::EditApplier::apply(source, edits);

    // Scrub-замены восстанавливаем простым текстовым проходом.
    for (const auto& [value, ph] : dict.scrub())
        replaceAll(out, ph, value);

    return out;
}

std::string Deanonymizer::restoreText(const std::string& text,
                                      const domain::Dictionary& dict) {
    std::string out = text;

    // Сначала восстанавливаем «обёрнутые» формы (строки, комментарии, инклуды),
    // т.к. их плейсхолдеры содержат кавычки/слэши. Bare-фрагменты f-string
    // (плейсхолдер без кавычек) сюда не входят — их безопаснее восстанавливать
    // через regex с границами слов (ниже), чтобы не задеть префиксы.
    // TODO: O(N*M) на проход — для больших словарей рассмотреть Aho-Corasick.
    for (const auto& [orig, v] : dict.strings())
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            replaceAll(out, v, orig);
    for (const auto& [orig, v] : dict.comments()) replaceAll(out, v, orig);
    for (const auto& [orig, v] : dict.includes()) replaceAll(out, v, orig);

    // Затем — голые плейсхолдеры за один проход через regex.
    std::map<std::string, std::string> bare;
    for (const auto& [orig, e] : dict.names())
        bare[e.placeholder] = orig;
    for (const auto& [orig, v] : dict.strings()) {
        // Обычная строка: v == "\"STR_0001\"" → голый STR_0001.
        // Фрагмент f-string: v == "STR_0001" (уже голый).
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            bare[v.substr(1, v.size() - 2)] = orig;
        else
            bare[v] = orig;
    }
    static const std::regex reCmt("CMT_[0-9]{4,}");
    static const std::regex reHdr("HDR_[0-9]{4,}");
    for (const auto& [orig, v] : dict.comments()) {
        std::smatch m;
        if (std::regex_search(v, m, reCmt)) bare[m.str()] = orig;
    }
    for (const auto& [orig, v] : dict.includes()) {
        std::smatch m;
        if (std::regex_search(v, m, reHdr)) bare[m.str()] = orig;
    }
    for (const auto& [value, ph] : dict.scrub())
        bare[ph] = value;

    static const std::regex token(
        R"(\b(ID|STR|CMT|HDR|IP|PATH|HOST|MAC|EMAIL|URL)_[0-9]{4,}\b)");
    std::string res;
    size_t last = 0;
    for (auto it = std::sregex_iterator(out.begin(), out.end(), token),
              e = std::sregex_iterator(); it != e; ++it) {
        auto m = *it;
        size_t s = m.position(), en = s + m.length();
        std::string t = m.str();
        res.append(out, last, s - last);
        auto f = bare.find(t);
        res += (f != bare.end() ? f->second : t);
        last = en;
    }
    res.append(out, last, out.size() - last);
    return res;
}

} // namespace application
