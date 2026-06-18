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
#include <cstring>

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

// f-string в tree-sitter-python — узел `string` с дочерними `interpolation`
// (выражения {…}). Если они есть — заходим внутрь, иначе строка непрозрачна.
bool PythonLanguageProfile::descendIntoString(TSNode node,
                                              const std::string& /*src*/) const {
    for (uint32_t i = 0, c = ts_node_child_count(node); i < c; ++i)
        if (!std::strcmp(ts_node_type(ts_node_child(node, i)), "interpolation"))
            return true;
    return false;
}

const std::set<std::string>& PythonLanguageProfile::stringFragmentTypes() const {
    static const std::set<std::string> s{"string_content"};
    return s;
}

// Стартовый preserve-список Python: builtins, частые имена stdlib-модулей,
// typing-алиасы, общеупотребительные dunder'ы и self/cls. Для прототипа —
// разумный минимум; при необходимости расширяется/инжектируется через DI-шов.
domain::PreserveList PythonLanguageProfile::defaultPreserveList() const {
    domain::PreserveList p;
    static const char* kNames[] = {
        // self / cls / контекстные
        "self", "cls", "_",
        // builtins — функции и типы (полный набор CPython)
        "abs", "aiter", "anext", "all", "any", "ascii", "bin", "bool",
        "breakpoint", "bytearray", "bytes", "callable", "chr", "classmethod",
        "compile", "complex", "delattr", "dict", "dir", "divmod", "enumerate",
        "eval", "exec", "filter", "float", "format", "frozenset", "getattr",
        "globals", "hasattr", "hash", "help", "hex", "id", "input", "int",
        "isinstance", "issubclass", "iter", "len", "list", "locals", "map",
        "max", "memoryview", "min", "next", "object", "oct", "open", "ord",
        "pow", "print", "property", "range", "repr", "reversed", "round", "set",
        "setattr", "slice", "sorted", "staticmethod", "str", "sum", "super",
        "tuple", "type", "vars", "zip", "__import__",
        // builtin-константы
        "True", "False", "None", "NotImplemented", "Ellipsis", "__debug__",
        // исключения и предупреждения
        "BaseException", "BaseExceptionGroup", "Exception", "ExceptionGroup",
        "ArithmeticError", "AssertionError", "AttributeError", "BlockingIOError",
        "BrokenPipeError", "BufferError", "ConnectionError", "ConnectionResetError",
        "ConnectionAbortedError", "ConnectionRefusedError", "EOFError",
        "FileExistsError", "FileNotFoundError", "FloatingPointError", "GeneratorExit",
        "ImportError", "ModuleNotFoundError", "IndentationError", "IndexError",
        "InterruptedError", "IOError", "IsADirectoryError", "KeyboardInterrupt",
        "KeyError", "LookupError", "MemoryError", "NameError", "NotADirectoryError",
        "NotImplementedError", "OSError", "OverflowError", "PermissionError",
        "ProcessLookupError", "RecursionError", "ReferenceError", "RuntimeError",
        "StopAsyncIteration", "StopIteration", "SyntaxError", "SystemError",
        "SystemExit", "TabError", "TimeoutError", "TypeError", "UnboundLocalError",
        "UnicodeError", "UnicodeDecodeError", "UnicodeEncodeError", "ValueError",
        "ZeroDivisionError", "Warning", "DeprecationWarning", "FutureWarning",
        "RuntimeWarning", "UserWarning",
        // частые dunder'ы (методы и атрибуты)
        "__init__", "__new__", "__del__", "__name__", "__main__", "__str__",
        "__repr__", "__bytes__", "__format__", "__len__", "__call__", "__getattr__",
        "__setattr__", "__getattribute__", "__getitem__", "__setitem__",
        "__delitem__", "__contains__", "__enter__", "__exit__", "__aenter__",
        "__aexit__", "__iter__", "__next__", "__aiter__", "__anext__", "__await__",
        "__eq__", "__ne__", "__lt__", "__le__", "__gt__", "__ge__", "__hash__",
        "__bool__", "__add__", "__sub__", "__mul__", "__post_init__", "__slots__",
        "__doc__", "__dict__", "__class__", "__module__", "__file__", "__all__",
        "__version__", "__future__", "__annotations__", "__qualname__",
        // частые декораторы
        "dataclass", "field", "abstractmethod", "abstractproperty",
        "cached_property", "contextmanager", "wraps", "lru_cache", "cache",
        "singledispatch", "total_ordering", "override", "final",
        // частые stdlib-модули
        "os", "sys", "re", "json", "math", "time", "datetime", "random",
        "collections", "itertools", "functools", "typing", "pathlib", "logging",
        "subprocess", "threading", "multiprocessing", "asyncio", "socket",
        "struct", "argparse", "unittest", "pytest", "abc", "enum", "dataclasses",
        "io", "copy", "traceback", "warnings", "contextlib", "operator", "string",
        "uuid", "hashlib", "base64", "decimal", "fractions", "statistics",
        "shutil", "glob", "tempfile", "pickle", "csv", "sqlite3", "urllib",
        "http", "inspect", "importlib", "weakref", "queue", "signal", "atexit",
        // typing-алиасы
        "List", "Dict", "Optional", "Union", "Tuple", "Any", "Callable",
        "Iterator", "Iterable", "Sequence", "Mapping", "MutableMapping", "Set",
        "FrozenSet", "Type", "TypeVar", "Generic", "Protocol", "Literal", "Final",
        "ClassVar", "Annotated", "TypedDict", "NamedTuple", "NewType", "cast",
        "overload", "Awaitable", "Coroutine", "AsyncIterator", "AsyncIterable",
        "Deque", "Counter", "OrderedDict", "DefaultDict", "Hashable", "Sized",
    };
    for (const char* n : kNames) p.add(n);
    return p;
}

} // namespace infrastructure
