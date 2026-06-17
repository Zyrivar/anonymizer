// domain/Dictionary.h — агрегат словаря отображений + порт его хранилища.
//
// Dictionary — доменный агрегат, инкапсулирующий обратимые отображения
// оригинал↔плейсхолдер для четырёх категорий плюс scrub-замены. Не зависит
// от формата сериализации (JSON прячется в инфраструктурном слое).
//
// IDictionaryStore — порт хранилища. Реальные альтернативы:
//   - JsonFileDictionaryStore (текущая: .json на диске)
//   - InMemoryDictionaryStore (для тестов / эфемерных сессий)
//   - возможный SQLite-бэкенд для больших словарей.
#pragma once
#include "Placeholder.h"
#include <string>
#include <map>
#include <memory>

namespace domain {

// Запись об имени: плейсхолдер + признак внешнего связывания.
struct NameEntry {
    std::string placeholder;   // "ID_0001"
    bool        isExtern = false;
};

// Доменный агрегат словаря. Хранит прямые отображения оригинал→замена.
// Обратные отображения строятся по запросу (для деанонимизации).
class Dictionary {
public:
    // --- Доступ к разделам (прямые отображения оригинал → замена) ---
    std::map<std::string, NameEntry>&    names()    { return names_; }
    std::map<std::string, std::string>&  strings()  { return strings_; }
    std::map<std::string, std::string>&  comments() { return comments_; }
    std::map<std::string, std::string>&  includes() { return includes_; }
    std::map<std::string, std::string>&  scrub()    { return scrub_; }

    const std::map<std::string, NameEntry>&   names()    const { return names_; }
    const std::map<std::string, std::string>& strings()  const { return strings_; }
    const std::map<std::string, std::string>& comments() const { return comments_; }
    const std::map<std::string, std::string>& includes() const { return includes_; }
    const std::map<std::string, std::string>& scrub()    const { return scrub_; }

    // Статистика (для отображения в UI).
    struct Stats { size_t names, strings, comments, includes, scrub; };
    Stats stats() const {
        return { names_.size(), strings_.size(), comments_.size(),
                 includes_.size(), scrub_.size() };
    }

    bool empty() const {
        return names_.empty() && strings_.empty() && comments_.empty()
            && includes_.empty() && scrub_.empty();
    }

    void clear() {
        names_.clear(); strings_.clear(); comments_.clear();
        includes_.clear(); scrub_.clear();
    }

private:
    std::map<std::string, NameEntry>   names_;
    std::map<std::string, std::string> strings_;
    std::map<std::string, std::string> comments_;
    std::map<std::string, std::string> includes_;
    std::map<std::string, std::string> scrub_;
};

// Порт хранилища словаря: загрузка и сохранение.
class IDictionaryStore {
public:
    virtual ~IDictionaryStore() = default;

    // Загрузить словарь из источника (путь/ключ зависит от реализации).
    // Бросает std::runtime_error при ошибке чтения/парсинга.
    virtual Dictionary load(const std::string& location) = 0;

    // Сохранить словарь по указанному адресу.
    // Бросает std::runtime_error при ошибке записи.
    virtual void save(const std::string& location, const Dictionary& dict) = 0;
};

} // namespace domain
