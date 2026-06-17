// test_core.cpp — юнит-тесты доменного ядра (Clean Architecture).
// Тестируют use-cases через фасад AnonymizerService. На assert, без фреймворка.
#include "application/AnonymizerService.h"
#include "infrastructure/JsonFileDictionaryStore.h"
#include <cstdio>
#include <string>

using application::AnonymizerService;
using application::StringMode;
using domain::Dictionary;

static int tests_run = 0;
static int tests_passed = 0;

#define EXPECT_EQ(a, b)                                       \
    do {                                                      \
        ++tests_run;                                          \
        if ((a) == (b)) { ++tests_passed; }                   \
        else {                                                \
            fprintf(stderr, "FAIL [%s:%d]: '%s' != '%s'\n",   \
                    __FILE__, __LINE__,                        \
                    std::string(a).c_str(),                    \
                    std::string(b).c_str());                   \
        }                                                     \
    } while(0)

#define EXPECT_TRUE(x)                                        \
    do {                                                      \
        ++tests_run;                                          \
        if ((x)) { ++tests_passed; }                          \
        else {                                                \
            fprintf(stderr, "FAIL [%s:%d]: %s\n",             \
                    __FILE__, __LINE__, #x);                   \
        }                                                     \
    } while(0)

// ─── Round-trip: restoreCode(anonymize(x)) == x ─────────────────────

void test_roundtrip_simple() {
    AnonymizerService svc;
    std::string src = R"(
int myVar = 42;
void myFunc() { myVar++; }
)";
    Dictionary d;
    std::string anon = svc.anonymize(src, d);
    std::string restored = svc.restoreCode(anon, d);
    EXPECT_EQ(restored, src);
}

void test_roundtrip_with_strings() {
    AnonymizerService svc;
    std::string src = R"(
const char* msg = "hello world";
// какой-то комментарий
int x = 0;
)";
    Dictionary d;
    std::string anon = svc.anonymize(src, d);
    std::string restored = svc.restoreCode(anon, d);
    EXPECT_EQ(restored, src);
}

void test_roundtrip_with_includes() {
    AnonymizerService svc;
    std::string src = R"(
#include "my_header.h"
#include <vector>
int foo() { return 0; }
)";
    Dictionary d;
    std::string anon = svc.anonymize(src, d);
    std::string restored = svc.restoreCode(anon, d);
    EXPECT_EQ(restored, src);
}

void test_roundtrip_multiline_comment() {
    AnonymizerService svc;
    std::string src = R"(
/* многострочный
   комментарий
   из трёх строк */
int bar = 1;
)";
    Dictionary d;
    std::string anon = svc.anonymize(src, d);
    std::string restored = svc.restoreCode(anon, d);
    EXPECT_EQ(restored, src);
}

// ─── Консистентность: один символ → один ID между вызовами ──────────

void test_same_symbol_same_id() {
    AnonymizerService svc;
    std::string src1 = "int myVar = 1;\n";
    std::string src2 = "int myVar = 2;\n";
    Dictionary d;
    std::string a1 = svc.anonymize(src1, d);
    std::string a2 = svc.anonymize(src2, d);
    EXPECT_TRUE(a1.find("ID_") != std::string::npos);
    std::string id = d.names()["myVar"].placeholder;
    EXPECT_TRUE(a1.find(id) != std::string::npos);
    EXPECT_TRUE(a2.find(id) != std::string::npos);
}

// ─── Сохраняемые имена не анонимизируются ───────────────────────────

void test_preserve_std_names() {
    AnonymizerService svc;
    std::string src = R"(
#include <vector>
int main() {
    std::vector<int> v;
    v.push_back(42);
    return 0;
}
)";
    Dictionary d;
    std::string anon = svc.anonymize(src, d);
    EXPECT_TRUE(anon.find("std") != std::string::npos);
    EXPECT_TRUE(anon.find("vector") != std::string::npos);
    EXPECT_TRUE(anon.find("main") != std::string::npos);
    EXPECT_TRUE(anon.find("push_back") != std::string::npos);
}

// ─── anonymize производит плейсхолдеры ──────────────────────────────

void test_forward_produces_placeholders() {
    AnonymizerService svc;
    std::string src = "int secretCounter = 0;\n";
    Dictionary d;
    std::string anon = svc.anonymize(src, d);
    EXPECT_TRUE(anon.find("secretCounter") == std::string::npos);
    EXPECT_TRUE(anon.find("ID_") != std::string::npos);
}

// ─── Пустой ввод ────────────────────────────────────────────────────

void test_empty_input() {
    AnonymizerService svc;
    Dictionary d;
    std::string anon = svc.anonymize("", d);
    EXPECT_EQ(anon, std::string(""));
    std::string rev = svc.restoreCode("", d);
    EXPECT_EQ(rev, std::string(""));
}

// ─── restoreText восстанавливает плейсхолдеры в прозе ───────────────

void test_reverse_text_in_prose() {
    AnonymizerService svc;
    std::string src = "int myFunc() { return 0; }\n";
    Dictionary d;
    std::string anon = svc.anonymize(src, d);
    std::string id = d.names()["myFunc"].placeholder;
    std::string prose = "You should refactor " + id + " to accept a parameter.";
    std::string restored = svc.restoreText(prose, d);
    EXPECT_TRUE(restored.find("myFunc") != std::string::npos);
    EXPECT_TRUE(restored.find(id) == std::string::npos);
}

// ─── Режим format: строки затираются, а не заменяются целиком ───────

void test_format_mode_scrubs_ips() {
    AnonymizerService svc;
    std::string src = R"(const char* addr = "192.168.1.100";)" "\n";
    Dictionary d;
    std::string anon = svc.anonymize(src, d, StringMode::Format);
    EXPECT_TRUE(anon.find("192.168.1.100") == std::string::npos);
    EXPECT_TRUE(anon.find("IP_") != std::string::npos);
}

// ─── Сканер утечек находит IP в строках ─────────────────────────────

void test_leak_scan_detects_ip() {
    AnonymizerService svc;
    std::string src = R"(const char* s = "connect to 10.0.0.5";)" "\n";
    auto leaks = svc.audit(src);
    EXPECT_TRUE(!leaks.empty());
    bool found_ip = false;
    for (auto& l : leaks)
        if (l.pattern == "IPv4" && l.value == "10.0.0.5") found_ip = true;
    EXPECT_TRUE(found_ip);
}

void test_leak_scan_no_false_positive_on_placeholder() {
    AnonymizerService svc;
    std::string src = R"(const char* s = "IP_0001";)" "\n";
    auto leaks = svc.audit(src);
    bool found_placeholder = false;
    for (auto& l : leaks)
        if (l.value == "IP_0001") found_placeholder = true;
    EXPECT_TRUE(!found_placeholder);
}

// ─── Идемпотентность: anonymize(anonymize(x)) использует те же ID ───

void test_forward_idempotent() {
    AnonymizerService svc;
    std::string src = "int myVal = 1;\n";
    Dictionary d;
    std::string a1 = svc.anonymize(src, d);
    std::string a2 = svc.anonymize(a1, d);
    EXPECT_EQ(a1, a2);
}

// ─── Структура словаря ──────────────────────────────────────────────

void test_dict_empty_initially() {
    Dictionary d;
    EXPECT_TRUE(d.empty());
    AnonymizerService svc;
    svc.anonymize("int x = 0;\n", d);
    EXPECT_TRUE(!d.empty());
}

// ─── extern-декларации отслеживаются ────────────────────────────────

void test_extern_tracked() {
    AnonymizerService svc;
    std::string src = R"(
extern int globalFlag;
void myFunc() { globalFlag = 1; }
)";
    Dictionary d;
    svc.anonymize(src, d);
    EXPECT_TRUE(d.names().count("globalFlag") > 0);
    EXPECT_TRUE(d.names()["globalFlag"].isExtern);
}

// ─── JSON round-trip словаря ────────────────────────────────────────

void test_dict_json_roundtrip() {
    AnonymizerService svc;
    std::string src = R"(
int mySecret = 42;
const char* host = "192.168.0.1";
// комментарий
)";
    Dictionary d;
    svc.anonymize(src, d);

    infrastructure::JsonFileDictionaryStore store;
    // Относительный путь рядом с тестом — кроссплатформенно (на Windows /tmp нет).
    const std::string path = "test_dict_rt.json";
    store.save(path, d);
    Dictionary loaded = store.load(path);

    std::string anon = svc.anonymize(src, d);
    std::string r1 = svc.restoreCode(anon, d);
    std::string r2 = svc.restoreCode(anon, loaded);
    EXPECT_EQ(r1, r2);
    EXPECT_EQ(r1, src);
}

// ─── DI-шов: внешний preserve-список уважается ──────────────────────

void test_custom_preserve_list() {
    const std::string src = "int ScadaTag = 0;\n";

    // Контроль: без кастомного списка имя анонимизируется.
    {
        AnonymizerService svc;
        Dictionary d;
        std::string anon = svc.anonymize(src, d);
        EXPECT_TRUE(anon.find("ScadaTag") == std::string::npos);
    }

    // С инжектированным preserve-списком имя сохраняется.
    {
        domain::PreserveList preserve;   // дефолтные имена + наше
        preserve.add("ScadaTag");
        AnonymizerService svc(preserve);
        Dictionary d;
        std::string anon = svc.anonymize(src, d);
        EXPECT_TRUE(anon.find("ScadaTag") != std::string::npos);
        EXPECT_TRUE(d.names().count("ScadaTag") == 0);
    }
}

// ─── main ───────────────────────────────────────────────────────────

int main() {
    test_roundtrip_simple();
    test_roundtrip_with_strings();
    test_roundtrip_with_includes();
    test_roundtrip_multiline_comment();
    test_same_symbol_same_id();
    test_preserve_std_names();
    test_forward_produces_placeholders();
    test_empty_input();
    test_reverse_text_in_prose();
    test_format_mode_scrubs_ips();
    test_leak_scan_detects_ip();
    test_leak_scan_no_false_positive_on_placeholder();
    test_forward_idempotent();
    test_dict_empty_initially();
    test_extern_tracked();
    test_dict_json_roundtrip();
    test_custom_preserve_list();

    fprintf(stderr, "\n=== core tests: %d/%d passed ===\n",
            tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
