// infrastructure/JsonFileDictionaryStore.h — реализация IDictionaryStore на JSON-файле.
#pragma once
#include "domain/Dictionary.h"

namespace infrastructure {

// Хранит словарь как JSON-файл на диске. Формат JSON (схема разделов
// names/strings/comments/includes/scrub) инкапсулирован в .cpp.
class JsonFileDictionaryStore : public domain::IDictionaryStore {
public:
    // location — путь к .json файлу.
    domain::Dictionary load(const std::string& location) override;
    void save(const std::string& location, const domain::Dictionary& dict) override;
};

} // namespace infrastructure
