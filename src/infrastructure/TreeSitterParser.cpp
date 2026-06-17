// infrastructure/TreeSitterParser.cpp — реализация парсера на tree-sitter-cpp.
// Весь C-API tree-sitter, RAII-обёртки и обход дерева инкапсулированы здесь.
#include "TreeSitterParser.h"
#include <tree_sitter/api.h>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <utility>

// C API, экспортируемый из tree-sitter-cpp/src/parser.c (вендорится).
extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace infrastructure {
namespace {

// --- RAII-обёртки для C-ресурсов tree-sitter (у них нет своих деструкторов) ---

class ParserGuard {
public:
    ParserGuard() : p_(ts_parser_new()) {
        if (p_) ts_parser_set_language(p_, tree_sitter_cpp());
    }
    ~ParserGuard() { if (p_) ts_parser_delete(p_); }
    ParserGuard(const ParserGuard&) = delete;
    ParserGuard& operator=(const ParserGuard&) = delete;
    TSParser* get() const { return p_; }
private:
    TSParser* p_;
};

class TreeGuard {
public:
    explicit TreeGuard(TSTree* t) : t_(t) {}
    ~TreeGuard() { if (t_) ts_tree_delete(t_); }
    TreeGuard(const TreeGuard&) = delete;
    TreeGuard& operator=(const TreeGuard&) = delete;
    TSTree* get() const { return t_; }
private:
    TSTree* t_;
};

// Защищённый разбор: парсер + дерево + корень, с проверкой ошибок.
class ParsedTree {
public:
    explicit ParsedTree(const std::string& src)
        : parser_()
        , tree_(parser_.get()
                ? ts_parser_parse_string(parser_.get(), nullptr,
                                         src.c_str(),
                                         static_cast<uint32_t>(src.size()))
                : nullptr)
    {
        if (!parser_.get())
            throw std::runtime_error("TreeSitterParser: ts_parser_new() failed");
        if (!tree_.get())
            throw std::runtime_error("TreeSitterParser: parsing returned null");
        root_ = ts_tree_root_node(tree_.get());
    }
    TSNode root() const { return root_; }
private:
    ParserGuard parser_;
    TreeGuard   tree_;
    TSNode      root_{};
};

// --- Вспомогательные функции обхода ---

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

// Итеративный DFS: собирает extern-имена (внешнее связывание).
void collectExtern(TSNode root, const std::string& src,
                   std::set<std::string>& names) {
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

// Итеративный DFS: собирает находки (hits).
void collectHits(TSNode root, const std::string& src,
                 const domain::PreserveList& preserve,
                 std::vector<domain::Hit>& out) {
    using domain::Category;
    std::vector<TSNode> stack;
    stack.push_back(root);

    while (!stack.empty()) {
        TSNode node = stack.back();
        stack.pop_back();

        const char* t = ts_node_type(node);

        if (!std::strcmp(t, "comment")) {
            out.push_back({ts_node_start_byte(node), ts_node_end_byte(node),
                           Category::Comment, slice(src, node)});
            continue; // не углубляемся
        }
        if (!std::strcmp(t, "char_literal")
            || !std::strcmp(t, "system_lib_string"))
            continue; // системные <...> и символьные литералы пропускаем

        if (!std::strcmp(t, "string_literal")
            || !std::strcmp(t, "raw_string_literal")) {
            TSNode p = ts_node_parent(node);
            const char* pt = ts_node_is_null(p) ? "" : ts_node_type(p);
            if (!std::strcmp(pt, "linkage_specification")) continue;
            Category cat = !std::strcmp(pt, "preproc_include")
                           ? Category::Include : Category::String;
            out.push_back({ts_node_start_byte(node), ts_node_end_byte(node),
                           cat, slice(src, node)});
            continue; // не углубляемся
        }

        if (!std::strcmp(t, "identifier") || !std::strcmp(t, "type_identifier")
            || !std::strcmp(t, "namespace_identifier")
            || !std::strcmp(t, "field_identifier")) {
            std::string x = slice(src, node);
            if (!preserve.contains(x))
                out.push_back({ts_node_start_byte(node), ts_node_end_byte(node),
                               Category::Name, x});
        }

        uint32_t n = ts_node_child_count(node);
        for (uint32_t i = n; i > 0; --i)
            stack.push_back(ts_node_child(node, i - 1));
    }
}

} // анонимное пространство имён

TreeSitterParser::TreeSitterParser(domain::PreserveList preserve)
    : preserve_(std::move(preserve)) {}

domain::ParseOutput TreeSitterParser::parse(const std::string& source) {
    ParsedTree parsed(source);
    domain::ParseOutput out;
    collectHits(parsed.root(), source, preserve_, out.hits);
    collectExtern(parsed.root(), source, out.externNames);
    return out;
}

} // namespace infrastructure
