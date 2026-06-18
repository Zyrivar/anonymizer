// infrastructure/TreeSitterParser.cpp — обобщённый драйвер tree-sitter.
// C-API tree-sitter и RAII-обёртки инкапсулированы здесь; вся специфика языка
// (имена типов узлов, extern-связывание) — в LanguageProfile.
#include "TreeSitterParser.h"
#include "LanguageProfile.h"
#include <tree_sitter/api.h>
#include <stdexcept>
#include <vector>

namespace infrastructure {
namespace {

// --- RAII-обёртки для C-ресурсов tree-sitter (у них нет своих деструкторов) ---

class ParserGuard {
public:
    explicit ParserGuard(const TSLanguage* lang) : p_(ts_parser_new()) {
        if (p_) ts_parser_set_language(p_, lang);
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
    ParsedTree(const std::string& src, const TSLanguage* lang)
        : parser_(lang)
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

std::string slice(const std::string& s, TSNode n) {
    uint32_t start = ts_node_start_byte(n);
    return s.substr(start, ts_node_end_byte(n) - start);
}

// Итеративный DFS: собирает находки (hits), используя классификацию из профиля.
void collectHits(TSNode root, const std::string& src,
                 const LanguageProfile& prof,
                 const domain::PreserveList& preserve,
                 std::vector<domain::Hit>& out) {
    using domain::Category;
    std::vector<TSNode> stack;
    stack.push_back(root);

    while (!stack.empty()) {
        TSNode node = stack.back();
        stack.pop_back();

        const std::string t = ts_node_type(node);

        if (prof.commentTypes().count(t)) {
            out.push_back({ts_node_start_byte(node), ts_node_end_byte(node),
                           Category::Comment, slice(src, node)});
            continue; // не углубляемся
        }

        if (prof.skipTypes().count(t))
            continue; // пропускаем целиком (не находка, не обходим вглубь)

        if (prof.stringTypes().count(t)) {
            TSNode p = ts_node_parent(node);
            const std::string pt = ts_node_is_null(p) ? std::string()
                                                       : ts_node_type(p);
            switch (prof.classifyString(pt)) {
            case LanguageProfile::StringRole::Skip:
                break;
            case LanguageProfile::StringRole::Include:
                out.push_back({ts_node_start_byte(node), ts_node_end_byte(node),
                               Category::Include, slice(src, node)});
                break;
            case LanguageProfile::StringRole::String:
                out.push_back({ts_node_start_byte(node), ts_node_end_byte(node),
                               Category::String, slice(src, node)});
                break;
            }
            continue; // не углубляемся
        }

        if (prof.identifierTypes().count(t)) {
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

TreeSitterParser::TreeSitterParser(std::shared_ptr<const LanguageProfile> profile,
                                   domain::PreserveList preserve)
    : profile_(std::move(profile)), preserve_(std::move(preserve)) {
    if (!profile_)
        throw std::runtime_error("TreeSitterParser: null language profile");
}

domain::ParseOutput TreeSitterParser::parse(const std::string& source) {
    ParsedTree parsed(source, profile_->tsLanguage());
    domain::ParseOutput out;
    collectHits(parsed.root(), source, *profile_, preserve_, out.hits);
    profile_->collectExternNames(parsed.root(), source, out.externNames);
    return out;
}

} // namespace infrastructure
