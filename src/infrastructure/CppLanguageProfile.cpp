// infrastructure/CppLanguageProfile.cpp — реализация профиля C/C++.
// Имена типов узлов и логика extern-связывания перенесены сюда из
// TreeSitterParser.cpp (раньше парсер был жёстко привязан к C++).
#include "CppLanguageProfile.h"
#include <cstring>
#include <vector>
#include <utility>

// C API, экспортируемый из tree-sitter-cpp/src/parser.c (вендорится).
extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace infrastructure {
namespace {

std::string slice(const std::string& s, TSNode n) {
    uint32_t start = ts_node_start_byte(n);
    return s.substr(start, ts_node_end_byte(n) - start);
}

constexpr const char* kDeclarator = "declarator";
const uint32_t kDeclaratorLen = static_cast<uint32_t>(std::strlen(kDeclarator));

// Спускается по цепочке declarator'ов до идентификатора имени декларации.
TSNode declName(TSNode decl) {
    TSNode d = ts_node_child_by_field_name(decl, kDeclarator, kDeclaratorLen);
    while (!ts_node_is_null(d)) {
        const char* t = ts_node_type(d);
        if (!std::strcmp(t, "identifier") || !std::strcmp(t, "field_identifier"))
            return d;
        TSNode n = ts_node_child_by_field_name(d, kDeclarator, kDeclaratorLen);
        if (ts_node_is_null(n)) break;
        d = n;
    }
    return d;
}

} // анонимное пространство имён

std::vector<std::string> CppLanguageProfile::fileExtensions() const {
    return {"c", "cc", "cxx", "cpp", "h", "hh", "hxx", "hpp"};
}

const TSLanguage* CppLanguageProfile::tsLanguage() const {
    return tree_sitter_cpp();
}

const std::set<std::string>& CppLanguageProfile::commentTypes() const {
    static const std::set<std::string> s{"comment"};
    return s;
}

const std::set<std::string>& CppLanguageProfile::identifierTypes() const {
    static const std::set<std::string> s{
        "identifier", "type_identifier", "namespace_identifier", "field_identifier"};
    return s;
}

const std::set<std::string>& CppLanguageProfile::stringTypes() const {
    static const std::set<std::string> s{"string_literal", "raw_string_literal"};
    return s;
}

const std::set<std::string>& CppLanguageProfile::skipTypes() const {
    // Символьные литералы и системные <...>-инклуды анонимизировать не нужно.
    static const std::set<std::string> s{"char_literal", "system_lib_string"};
    return s;
}

LanguageProfile::StringRole
CppLanguageProfile::classifyString(const std::string& parentType) const {
    if (parentType == "linkage_specification") return StringRole::Skip;    // extern "C"
    if (parentType == "preproc_include")       return StringRole::Include; // #include "..."
    return StringRole::String;
}

// Итеративный DFS: собирает имена с внешним связыванием (extern).
void CppLanguageProfile::collectExternNames(TSNode root, const std::string& src,
                                            std::set<std::string>& names) const {
    std::vector<std::pair<TSNode, bool>> stack;
    stack.push_back({root, false});

    while (!stack.empty()) {
        auto [node, lk] = stack.back();
        stack.pop_back();

        const char* t = ts_node_type(node);
        if (!std::strcmp(t, "linkage_specification")) lk = true;
        if (!std::strcmp(t, "compound_statement")
            || !std::strcmp(t, "parameter_list")) lk = false;

        if (!std::strcmp(t, "declaration")) {
            bool ext = lk;
            for (uint32_t i = 0, c = ts_node_child_count(node); i < c; ++i) {
                TSNode ch = ts_node_child(node, i);
                if (!std::strcmp(ts_node_type(ch), "storage_class_specifier")
                    && slice(src, ch) == "extern")
                    ext = true;
            }
            if (ext) {
                TSNode nm = declName(node);
                if (!ts_node_is_null(nm)) {
                    const char* nt = ts_node_type(nm);
                    if (!std::strcmp(nt, "identifier")
                        || !std::strcmp(nt, "field_identifier"))
                        names.insert(slice(src, nm));
                }
            }
        }

        uint32_t n = ts_node_child_count(node);
        for (uint32_t i = n; i > 0; --i)
            stack.push_back({ts_node_child(node, i - 1), lk});
    }
}

} // namespace infrastructure
