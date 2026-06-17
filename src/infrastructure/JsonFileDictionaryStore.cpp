// infrastructure/JsonFileDictionaryStore.cpp — сериализация словаря в JSON.
// Маппинг доменного Dictionary ↔ JSON-схема инкапсулирован здесь.
#include "JsonFileDictionaryStore.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace infrastructure {
namespace {

// JSON-схема (обратно совместима со старым форматом):
// {
//   "names":    { "<orig>": { "id": "ID_0001", "extern": false }, ... },
//   "strings":  { "<orig>": "\"STR_0001\"", ... },
//   "comments": { "<orig>": "// CMT_0001", ... },
//   "includes": { "<orig>": "\"HDR_0001.h\"", ... },
//   "scrub":    { "<value>": "IP_0001", ... }
// }

domain::Dictionary fromJson(const json& j) {
    domain::Dictionary d;

    if (j.contains("names"))
        for (auto& [orig, v] : j["names"].items())
            d.names()[orig] = {
                v.value("id", std::string{}),
                v.value("extern", false)
            };
    if (j.contains("strings"))
        for (auto& [orig, v] : j["strings"].items())
            d.strings()[orig] = v.get<std::string>();
    if (j.contains("comments"))
        for (auto& [orig, v] : j["comments"].items())
            d.comments()[orig] = v.get<std::string>();
    if (j.contains("includes"))
        for (auto& [orig, v] : j["includes"].items())
            d.includes()[orig] = v.get<std::string>();
    if (j.contains("scrub"))
        for (auto& [orig, v] : j["scrub"].items())
            d.scrub()[orig] = v.get<std::string>();

    return d;
}

json toJson(const domain::Dictionary& d) {
    json j;
    j["names"]    = json::object();
    j["strings"]  = json::object();
    j["comments"] = json::object();
    j["includes"] = json::object();
    j["scrub"]    = json::object();

    for (auto& [orig, e] : d.names())
        j["names"][orig] = { {"id", e.placeholder}, {"extern", e.isExtern} };
    for (auto& [orig, v] : d.strings())  j["strings"][orig]  = v;
    for (auto& [orig, v] : d.comments()) j["comments"][orig] = v;
    for (auto& [orig, v] : d.includes()) j["includes"][orig] = v;
    for (auto& [orig, v] : d.scrub())    j["scrub"][orig]    = v;

    return j;
}

} // анонимное пространство имён

domain::Dictionary JsonFileDictionaryStore::load(const std::string& location) {
    std::ifstream f(location);
    if (!f)
        throw std::runtime_error("JsonFileDictionaryStore: cannot open " + location);
    json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("JsonFileDictionaryStore: parse error: ") + e.what());
    }
    return fromJson(j);
}

void JsonFileDictionaryStore::save(const std::string& location,
                                   const domain::Dictionary& dict) {
    std::ofstream f(location);
    if (!f)
        throw std::runtime_error("JsonFileDictionaryStore: cannot write " + location);
    f << toJson(dict).dump(2);
}

} // namespace infrastructure
