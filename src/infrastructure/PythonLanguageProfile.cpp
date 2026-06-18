// infrastructure/PythonLanguageProfile.cpp — реализация профиля Python.
//
// Прототип. Узлы Python-грамматики проще C++:
//   - comment    → Category::Comment ('#'-строки)
//   - identifier → Category::Name    (один тип, без type_/field_/namespace_)
//   - string     → Category::String  (узел охватывает кавычки и префикс f/r/b)
// Нет символьных литералов и системных <...>-инклудов (skipTypes пуст), нет
// extern-связывания. Имена модулей в `import x` — это identifier и
// анонимизируются как обычные имена (консистентно между файлами).
#include "PythonLanguageProfile.h"

// C API, экспортируемый из tree-sitter-python/src/parser.c (вендорится).
extern "C" const TSLanguage* tree_sitter_python(void);

namespace infrastructure {

std::vector<std::string> PythonLanguageProfile::fileExtensions() const {
    return {"py", "pyi", "pyw"};
}

const TSLanguage* PythonLanguageProfile::tsLanguage() const {
    return tree_sitter_python();
}

const std::set<std::string>& PythonLanguageProfile::commentTypes() const {
    static const std::set<std::string> s{"comment"};
    return s;
}

const std::set<std::string>& PythonLanguageProfile::identifierTypes() const {
    static const std::set<std::string> s{"identifier"};
    return s;
}

const std::set<std::string>& PythonLanguageProfile::stringTypes() const {
    static const std::set<std::string> s{"string"};
    return s;
}

const std::set<std::string>& PythonLanguageProfile::skipTypes() const {
    static const std::set<std::string> s{}; // в Python пропускать нечего
    return s;
}

LanguageProfile::StringRole
PythonLanguageProfile::classifyString(const std::string& /*parentType*/) const {
    return StringRole::String; // в Python нет #include и extern "C"
}

// Стартовый preserve-список Python: builtins, частые имена stdlib-модулей,
// typing-алиасы, общеупотребительные dunder'ы и self/cls. Для прототипа —
// разумный минимум; при необходимости расширяется/инжектируется через DI-шов.
domain::PreserveList PythonLanguageProfile::defaultPreserveList() const {
    domain::PreserveList p;
    static const char* kNames[] = {
        // self / cls
        "self", "cls",
        // builtins (функции и типы)
        "print", "len", "range", "str", "int", "float", "bool", "bytes",
        "bytearray", "list", "dict", "set", "frozenset", "tuple", "type",
        "object", "super", "isinstance", "issubclass", "hasattr", "getattr",
        "setattr", "delattr", "enumerate", "zip", "map", "filter", "sorted",
        "reversed", "sum", "min", "max", "abs", "round", "open", "input",
        "format", "repr", "id", "hash", "iter", "next", "any", "all", "complex",
        "divmod", "pow", "ord", "chr", "hex", "oct", "bin", "vars", "dir",
        "globals", "locals", "callable", "staticmethod", "classmethod",
        "property", "slice", "memoryview", "NotImplemented", "Ellipsis",
        // частые исключения
        "Exception", "BaseException", "ValueError", "TypeError", "KeyError",
        "IndexError", "RuntimeError", "StopIteration", "AttributeError",
        "NotImplementedError", "FileNotFoundError", "OSError", "IOError",
        "ImportError", "ZeroDivisionError", "ArithmeticError", "KeyboardInterrupt",
        "SystemExit", "GeneratorExit", "Warning", "AssertionError",
        // частые dunder'ы
        "__init__", "__name__", "__main__", "__str__", "__repr__", "__len__",
        "__call__", "__enter__", "__exit__", "__iter__", "__next__", "__eq__",
        "__hash__", "__doc__", "__dict__", "__class__", "__module__", "__file__",
        // частые stdlib-модули
        "os", "sys", "re", "json", "math", "time", "datetime", "random",
        "collections", "itertools", "functools", "typing", "pathlib", "logging",
        "subprocess", "threading", "asyncio", "argparse", "unittest", "abc",
        "enum", "dataclasses", "io", "copy", "traceback", "warnings",
        // typing-алиасы
        "List", "Dict", "Optional", "Union", "Tuple", "Any", "Callable",
        "Iterator", "Iterable", "Sequence", "Mapping", "Set", "Type",
    };
    for (const char* n : kNames) p.add(n);
    return p;
}

} // namespace infrastructure
