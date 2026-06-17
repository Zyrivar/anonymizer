// test_e2e.cpp — End-to-end тест: реалистичный промышленный исходник C++ (~350 строк)
// через полный конвейер анонимизации. Проверяет round-trip, устранение утечек,
// затирание в режиме format, reverseText на прозе и консистентность словаря.
//
// Источник: смоделирован по реальной SCADA/АСУ ТП кодовой базе с
// IP-адресами, MAC-адресами, путями, URL, email, extern-декларациями,
// пространствами имён, классами, шаблонами, многострочными комментариями, инклудами и т.д.
//
// Сборка: cd tests && qmake && make && ./test_e2e

#include "application/AnonymizerService.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <set>
#include <regex>

using application::AnonymizerService;
using application::StringMode;
using domain::Dictionary;

// ── Реалистичный промышленный исходник C++ (~350 строк) ─────────────────
// Содержит все конструкции, которые анонимизатор должен обрабатывать:
// - пользовательские #include "..."
// - системные #include <...> (должны сохраняться)
// - однострочные и многострочные комментарии
// - строковые литералы с IP, MAC, путями, URL, email, хостами
// - extern-декларации
// - пространства имён, классы, перечисления, шаблоны
// - идентификаторы из preserve-list (std::vector, push_back и т.д.)
// - сырые строковые литералы

static const char* const SAMPLE_SOURCE = R"cpp(
// scada_gateway.cpp — SCADA gateway for turbogenerator monitoring
// Author: Ivanov A.V. <ivanov@factory.local>
// Created: 2024-03-15
// License: Proprietary, JSC MetallurgCombinat

#include "scada_gateway.h"
#include "modbus_client.h"
#include "data_logger.h"
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>

/*
 * Gateway configuration defaults.
 * These values are used when config.json is missing or incomplete.
 * Contact: support@scada-vendor.com for licensing questions.
 * Documentation: https://docs.scada-vendor.com/gateway/v3/setup
 */

extern int globalShutdownFlag;
extern const char* firmwareVersion;

namespace scada {
namespace gateway {

// Connection parameters for the Modbus TCP fieldbus
constexpr int DEFAULT_MODBUS_PORT = 502;
constexpr int RECONNECT_DELAY_MS  = 3000;
constexpr int MAX_RETRY_COUNT     = 5;

// Register map for KER-500 turbogenerator controller
enum class RegisterId : uint16_t {
    BEARING_TEMP_1   = 0x0100,
    BEARING_TEMP_2   = 0x0101,
    OIL_PRESSURE     = 0x0102,
    VIBRATION_X      = 0x0110,
    VIBRATION_Y      = 0x0111,
    ROTOR_SPEED      = 0x0120,
    STATOR_CURRENT   = 0x0130,
    EXCITER_VOLTAGE  = 0x0131,
    ACTIVE_POWER     = 0x0140,
    REACTIVE_POWER   = 0x0141,
    ALARM_WORD       = 0x0200,
};

// Alarm bit definitions for ALARM_WORD register
struct AlarmBits {
    static constexpr uint16_t OVER_TEMP       = 0x0001;
    static constexpr uint16_t LOW_OIL         = 0x0002;
    static constexpr uint16_t HIGH_VIBRATION  = 0x0004;
    static constexpr uint16_t OVERSPEED       = 0x0008;
    static constexpr uint16_t OVERCURRENT     = 0x0010;
    static constexpr uint16_t GROUND_FAULT    = 0x0020;
    static constexpr uint16_t COMM_FAILURE    = 0x0040;
};

// Single reading from a sensor
struct SensorReading {
    RegisterId   regId;
    double       value;
    uint64_t     timestamp;    // ms since epoch
    bool         valid;
    std::string  unit;         // "°C", "kPa", "mm/s", "RPM", "MW"
};

// Forward declaration
class DataLogger;

/**
 * @brief GatewayController manages connections to one or more PLCs
 *        and polls sensor data at configurable intervals.
 *
 * Network topology:
 *   PLC-1 (KER-500):  10.32.1.100:502
 *   PLC-2 (KER-500):  10.32.1.101:502
 *   HMI server:       10.32.1.5
 *   Historian:         10.32.2.50:5432
 *   Gateway MAC:       00:1A:2B:3C:4D:5E
 *
 * Config file: /opt/scada/etc/gateway.json
 * Log path:    /var/log/scada/gateway.log
 * PID file:    /var/run/scada/gateway.pid
 */
class GatewayController {
public:
    explicit GatewayController(const std::string& configPath)
        : m_configPath(configPath)
        , m_running(false)
        , m_pollIntervalMs(1000)
    {
        // Default PLC endpoints
        m_plcEndpoints.push_back({"10.32.1.100", DEFAULT_MODBUS_PORT, "TG-1"});
        m_plcEndpoints.push_back({"10.32.1.101", DEFAULT_MODBUS_PORT, "TG-2"});
    }

    ~GatewayController() {
        stop();
    }

    // Initialize: load config, open log, verify connectivity
    bool initialize() {
        std::cout << "Gateway initializing from " << m_configPath << std::endl;
        std::cout << "Firmware: " << firmwareVersion << std::endl;

        // Log the MAC address for audit trail
        std::string mac = "00:1A:2B:3C:4D:5E";
        std::cout << "Gateway MAC: " << mac << std::endl;

        // Try to reach the historian database
        std::string historianUrl = "postgresql://dbadmin@10.32.2.50:5432/scada_hist";
        std::cout << "Historian: " << historianUrl << std::endl;

        // Send startup notification email
        std::string notifyEmail = "operator@factory.local";
        std::cout << "Notify: " << notifyEmail << std::endl;

        // Check documentation URL
        std::string docsUrl = "https://docs.scada-vendor.com/gateway/v3/api";
        std::cout << "Docs: " << docsUrl << std::endl;

        for (size_t i = 0; i < m_plcEndpoints.size(); ++i) {
            auto& ep = m_plcEndpoints[i];
            std::cout << "PLC[" << i << "]: " << ep.host
                      << ":" << ep.port
                      << " (" << ep.label << ")" << std::endl;
        }

        m_logger = std::make_unique<DataLogger>("/var/log/scada/gateway.log");
        return true;
    }

    // Start the polling loop in a background thread
    void start() {
        if (m_running) return;
        m_running = true;
        m_pollThread = std::thread([this]() { pollLoop(); });
    }

    void stop() {
        m_running = false;
        if (m_pollThread.joinable()) {
            m_pollThread.join();
        }
    }

    // Get latest readings for a specific turbogenerator
    std::vector<SensorReading> getReadings(const std::string& tgLabel) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_latestData.find(tgLabel);
        if (it != m_latestData.end()) {
            return it->second;
        }
        return {};
    }

    // Check if any alarm is active
    bool hasActiveAlarm(const std::string& tgLabel) const {
        auto readings = getReadings(tgLabel);
        for (const auto& r : readings) {
            if (r.regId == RegisterId::ALARM_WORD && r.value != 0.0) {
                return true;
            }
        }
        return false;
    }

    // Decode alarm word into human-readable strings
    static std::vector<std::string> decodeAlarms(uint16_t alarmWord) {
        std::vector<std::string> alarms;
        if (alarmWord & AlarmBits::OVER_TEMP)      alarms.push_back("OVER_TEMP");
        if (alarmWord & AlarmBits::LOW_OIL)        alarms.push_back("LOW_OIL_PRESSURE");
        if (alarmWord & AlarmBits::HIGH_VIBRATION) alarms.push_back("HIGH_VIBRATION");
        if (alarmWord & AlarmBits::OVERSPEED)      alarms.push_back("OVERSPEED");
        if (alarmWord & AlarmBits::OVERCURRENT)    alarms.push_back("OVERCURRENT");
        if (alarmWord & AlarmBits::GROUND_FAULT)   alarms.push_back("GROUND_FAULT");
        if (alarmWord & AlarmBits::COMM_FAILURE)   alarms.push_back("COMM_FAILURE");
        return alarms;
    }

private:
    struct PlcEndpoint {
        std::string host;
        int         port;
        std::string label;
    };

    // Main polling loop — runs in background thread
    void pollLoop() {
        while (m_running) {
            for (auto& ep : m_plcEndpoints) {
                auto readings = pollSinglePlc(ep);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_latestData[ep.label] = readings;
                }

                // Check for critical alarms and log
                for (const auto& r : readings) {
                    if (r.regId == RegisterId::ALARM_WORD) {
                        uint16_t aw = static_cast<uint16_t>(r.value);
                        if (aw != 0) {
                            auto alarms = decodeAlarms(aw);
                            for (const auto& a : alarms) {
                                m_logger->logAlarm(ep.label, a, r.timestamp);
                            }
                        }
                    }
                }
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(m_pollIntervalMs));
        }
    }

    // Poll a single PLC via Modbus TCP
    std::vector<SensorReading> pollSinglePlc(const PlcEndpoint& ep) {
        std::vector<SensorReading> results;

        // Build the register read list
        struct RegDef {
            RegisterId id;
            std::string unit;
            double scale;
        };
        std::vector<RegDef> regs = {
            {RegisterId::BEARING_TEMP_1,  "°C",   0.1},
            {RegisterId::BEARING_TEMP_2,  "°C",   0.1},
            {RegisterId::OIL_PRESSURE,    "kPa",  1.0},
            {RegisterId::VIBRATION_X,     "mm/s", 0.01},
            {RegisterId::VIBRATION_Y,     "mm/s", 0.01},
            {RegisterId::ROTOR_SPEED,     "RPM",  1.0},
            {RegisterId::STATOR_CURRENT,  "A",    0.1},
            {RegisterId::EXCITER_VOLTAGE, "V",    0.1},
            {RegisterId::ACTIVE_POWER,    "MW",   0.01},
            {RegisterId::REACTIVE_POWER,  "MVAr", 0.01},
            {RegisterId::ALARM_WORD,      "",     1.0},
        };

        auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        for (const auto& reg : regs) {
            SensorReading reading;
            reading.regId    = reg.id;
            reading.value    = 0.0;  // Would read from Modbus here
            reading.timestamp = ts;
            reading.valid    = true;
            reading.unit     = reg.unit;
            results.push_back(reading);
        }

        return results;
    }

    std::string                                          m_configPath;
    bool                                                 m_running;
    int                                                  m_pollIntervalMs;
    std::vector<PlcEndpoint>                             m_plcEndpoints;
    std::thread                                          m_pollThread;
    mutable std::mutex                                   m_mutex;
    std::map<std::string, std::vector<SensorReading>>    m_latestData;
    std::unique_ptr<DataLogger>                          m_logger;
};

// Template utility: moving average filter for sensor data smoothing
template<typename T, size_t WindowSize>
class MovingAverage {
public:
    MovingAverage() : m_sum(0), m_count(0), m_index(0) {
        m_buffer.resize(WindowSize, T(0));
    }

    T update(T newValue) {
        m_sum -= m_buffer[m_index];
        m_buffer[m_index] = newValue;
        m_sum += newValue;
        m_index = (m_index + 1) % WindowSize;
        if (m_count < WindowSize) ++m_count;
        return m_sum / static_cast<T>(m_count);
    }

    T average() const {
        return m_count > 0 ? m_sum / static_cast<T>(m_count) : T(0);
    }

    void clear() {
        std::fill(m_buffer.begin(), m_buffer.end(), T(0));
        m_sum = T(0);
        m_count = 0;
        m_index = 0;
    }

private:
    std::vector<T> m_buffer;
    T              m_sum;
    size_t         m_count;
    size_t         m_index;
};

} // namespace gateway
} // namespace scada

// Entry point (for standalone testing)
int main(int argc, char* argv[]) {
    std::string configFile = "/opt/scada/etc/gateway.json";
    if (argc > 1) configFile = argv[1];

    scada::gateway::GatewayController gw(configFile);
    if (!gw.initialize()) {
        std::cerr << "Gateway init failed!" << std::endl;
        return 1;
    }

    gw.start();

    // Run for 60 seconds then shut down
    std::this_thread::sleep_for(std::chrono::seconds(60));
    gw.stop();

    // Print final readings
    auto readings = gw.getReadings("TG-1");
    for (const auto& r : readings) {
        std::cout << "Reg 0x" << std::hex
                  << static_cast<int>(r.regId) << std::dec
                  << ": " << r.value << " " << r.unit << std::endl;
    }

    return 0;
}
)cpp";

// ── Счётчики тестов ─────────────────────────────────────────────────────

static int g_run = 0, g_pass = 0;

static void check(bool cond, const char* file, int line, const char* expr) {
    ++g_run;
    if (cond) { ++g_pass; return; }
    fprintf(stderr, "FAIL [%s:%d] %s\n", file, line, expr);
}
#define CHECK(x) check((x), __FILE__, __LINE__, #x)

// ── Хелпер: подсчёт строк ───────────────────────────────────────────────

static int countLines(const std::string& s) {
    int n = 0;
    for (char c : s) if (c == '\n') ++n;
    return n;
}

// ── Хелпер: проверка, встречается ли любая из строк в тексте ────────────

static bool containsAny(const std::string& text,
                         const std::vector<std::string>& needles) {
    for (auto& n : needles)
        if (text.find(n) != std::string::npos) return true;
    return false;
}

// ── Тесты ───────────────────────────────────────────────────────────────

int main() {
    AnonymizerService svc;
    const std::string src(SAMPLE_SOURCE);
    fprintf(stderr, "E2E source: %d lines, %zu bytes\n",
            countLines(src), src.size());
    CHECK(countLines(src) >= 300);

    // ================================================================
    // ТЕСТ 1: Forward (режим opaque) — анонимизируем весь исходник
    // ================================================================
    Dictionary dict;
    std::string anon = svc.anonymize(src, dict);

    // 1a. Оригинальные чувствительные данные НЕ должны быть в выводе
    CHECK(anon.find("scada_gateway") == std::string::npos);
    CHECK(anon.find("modbus_client") == std::string::npos);
    CHECK(anon.find("data_logger") == std::string::npos);
    CHECK(anon.find("GatewayController") == std::string::npos);
    CHECK(anon.find("SensorReading") == std::string::npos);
    CHECK(anon.find("pollSinglePlc") == std::string::npos);
    CHECK(anon.find("MovingAverage") == std::string::npos);
    CHECK(anon.find("ivanov@factory.local") == std::string::npos);
    CHECK(anon.find("10.32.1.100") == std::string::npos);
    CHECK(anon.find("00:1A:2B:3C:4D:5E") == std::string::npos);
    CHECK(anon.find("/opt/scada") == std::string::npos);
    CHECK(anon.find("/var/log/scada") == std::string::npos);
    CHECK(anon.find("scada-vendor.com") == std::string::npos);
    CHECK(anon.find("factory.local") == std::string::npos);
    fprintf(stderr, "  [PASS] Forward: sensitive data eliminated\n");

    // 1b. Плейсхолдеры ДОЛЖНЫ присутствовать
    CHECK(anon.find("ID_") != std::string::npos);
    CHECK(anon.find("STR_") != std::string::npos);
    CHECK(anon.find("CMT_") != std::string::npos);
    CHECK(anon.find("HDR_") != std::string::npos);
    fprintf(stderr, "  [PASS] Forward: placeholders present\n");

    // 1c. Сохраняемые имена ДОЛЖНЫ уцелеть
    CHECK(anon.find("std") != std::string::npos);
    CHECK(anon.find("vector") != std::string::npos);
    CHECK(anon.find("string") != std::string::npos);
    CHECK(anon.find("push_back") != std::string::npos);
    CHECK(anon.find("cout") != std::string::npos);
    CHECK(anon.find("endl") != std::string::npos);
    CHECK(anon.find("size") != std::string::npos);
    CHECK(anon.find("find") != std::string::npos);
    CHECK(anon.find("begin") != std::string::npos);
    CHECK(anon.find("end") != std::string::npos);
    fprintf(stderr, "  [PASS] Forward: preserve-list intact\n");

    // 1d. Системные инклуды сохранены, пользовательские анонимизированы
    CHECK(anon.find("<vector>") != std::string::npos);
    CHECK(anon.find("<string>") != std::string::npos);
    CHECK(anon.find("<iostream>") != std::string::npos);
    CHECK(anon.find("<thread>") != std::string::npos);
    CHECK(anon.find("\"scada_gateway.h\"") == std::string::npos);
    CHECK(anon.find("\"modbus_client.h\"") == std::string::npos);
    fprintf(stderr, "  [PASS] Forward: includes handled correctly\n");

    // 1e. Словарь заполнен
    CHECK(dict.names().size() > 30);   // lots of identifiers
    CHECK(dict.strings().size() > 5);  // string literals
    CHECK(dict.comments().size() > 3); // comments
    CHECK(dict.includes().size() == 3); // 3 user includes
    fprintf(stderr, "  [PASS] Forward: dict populated (names=%u strings=%u comments=%u includes=%u)\n",
            (unsigned)dict.names().size(), (unsigned)dict.strings().size(),
            (unsigned)dict.comments().size(), (unsigned)dict.includes().size());

    // 1f. extern-декларации отслежены
    CHECK((dict.names().count("globalShutdownFlag") > 0));
    CHECK(dict.names()["globalShutdownFlag"].isExtern == true);
    CHECK((dict.names().count("firmwareVersion") > 0));
    CHECK(dict.names()["firmwareVersion"].isExtern == true);
    fprintf(stderr, "  [PASS] Forward: extern declarations tracked\n");

    // ================================================================
    // ТЕСТ 2: Round-trip — reverse(forward(x)) == x
    // ================================================================
    std::string restored = svc.restoreCode(anon, dict);
    CHECK(restored == src);
    fprintf(stderr, "  [PASS] Round-trip: reverse(forward(src)) == src  (%zu bytes)\n",
            src.size());

    // ================================================================
    // ТЕСТ 3: Идемпотентность forward для идентификаторов
    // ================================================================
    // Примечание: полная идемпотентность (forward(anon) == anon) НЕ выполняется, т.к.
    // комментарии-плейсхолдеры (// CMT_0001) и строки ("STR_0001") трактуются
    // как новые литералы при повторном проходе. Это by design.
    // Что гарантировано: плейсхолдеры идентификаторов (ID_*) распознаются
    // через isPlaceholder() и пропускаются, поэтому не анонимизируются дважды.
    Dictionary dict2 = dict;
    std::string anon2 = svc.anonymize(anon, dict2);

    // Ни один ID_NNNN не должен быть обёрнут в другой ID_ плейсхолдер
    std::regex doubleId("ID_[0-9]{4,}.*ID_[0-9]{4,}");
    // Считаем уникальные ID_ токены — должно быть одинаково
    std::regex idRe("\\bID_[0-9]{4,}\\b");
    auto countIds = [&](const std::string& s) {
        std::set<std::string> ids;
        for (auto it = std::sregex_iterator(s.begin(), s.end(), idRe),
                  e = std::sregex_iterator(); it != e; ++it)
            ids.insert(it->str());
        return ids;
    };
    auto ids1 = countIds(anon);
    auto ids2 = countIds(anon2);
    CHECK(ids1 == ids2);
    fprintf(stderr, "  [PASS] Idempotency: identifier placeholders stable (%zu IDs)\n",
            ids1.size());

    // ================================================================
    // ТЕСТ 4: Режим format — IP/MAC/URL/email затёрты, строки целиком — нет
    // ================================================================
    Dictionary dictFmt;
    std::string anonFmt = svc.anonymize(src, dictFmt, StringMode::Format);

    // В режиме format чувствительные подстроки в строках заменяются на scrub-плейсхолдеры
    CHECK(anonFmt.find("10.32.1.100") == std::string::npos);
    CHECK(anonFmt.find("10.32.1.101") == std::string::npos);
    CHECK(anonFmt.find("10.32.2.50") == std::string::npos);
    CHECK(anonFmt.find("00:1A:2B:3C:4D:5E") == std::string::npos);
    CHECK(anonFmt.find("ivanov@factory.local") == std::string::npos);
    CHECK(anonFmt.find("operator@factory.local") == std::string::npos);
    CHECK(dictFmt.scrub().size() > 5);  // несколько затёртых значений
    fprintf(stderr, "  [PASS] Format mode: sensitive substrings scrubbed (scrub=%u)\n",
            (unsigned)dictFmt.scrub().size());

    // Round-trip режима format через reverseTokens
    std::string restoredFmt = svc.restoreCode(anonFmt, dictFmt);
    CHECK(restoredFmt == src);
    fprintf(stderr, "  [PASS] Format mode round-trip: OK\n");

    // ================================================================
    // ТЕСТ 5: Сканирование утечек на оригинале
    // ================================================================
    auto leaks = svc.audit(src);
    CHECK(!leaks.empty());

    // Должен обнаружить конкретные чувствительные значения
    bool foundIp = false, foundMac = false, foundEmail = false, foundUrl = false, foundPath = false;
    for (auto& l : leaks) {
        if (l.pattern == "IPv4"  && l.value.find("10.32") != std::string::npos) foundIp = true;
        if (l.pattern == "MAC"   && l.value == "00:1A:2B:3C:4D:5E") foundMac = true;
        if (l.pattern == "EMAIL" && l.value.find("@factory.local") != std::string::npos) foundEmail = true;
        if (l.pattern == "URL"   && l.value.find("https://") != std::string::npos) foundUrl = true;
        if (l.pattern == "PATH"  && l.value.find("/opt/scada") != std::string::npos) foundPath = true;
    }
    CHECK(foundIp);
    CHECK(foundMac);
    CHECK(foundEmail);
    CHECK(foundUrl);
    CHECK(foundPath);
    fprintf(stderr, "  [PASS] Leak scan: %zu findings (IP=%d MAC=%d EMAIL=%d URL=%d PATH=%d)\n",
            leaks.size(), foundIp, foundMac, foundEmail, foundUrl, foundPath);

    // ================================================================
    // ТЕСТ 6: Сканирование утечек на анонимизированном выводе — НИЧЕГО не должно быть
    // ================================================================
    auto leaksAnon = svc.audit(anon);
    bool hasRealLeak = false;
    for (auto& l : leaksAnon) {
        // Плейсхолдеры (IP_0001 и т.п.) допустимы — они ожидаемы
        // Но реальных IP, MAC, email быть не должно
        if (l.pattern == "IPv4" && l.value.find("10.32") != std::string::npos) hasRealLeak = true;
        if (l.pattern == "MAC"  && l.value == "00:1A:2B:3C:4D:5E") hasRealLeak = true;
        if (l.pattern == "EMAIL" && l.value.find("@factory") != std::string::npos) hasRealLeak = true;
    }
    CHECK(!hasRealLeak);
    fprintf(stderr, "  [PASS] Leak scan on anonymized output: no real leaks\n");

    // ================================================================
    // ТЕСТ 7: reverseText на прозе с плейсхолдерами
    // ================================================================
    // Имитируем ответ LLM, использующий наши плейсхолдеры
    std::string gwId = dict.names()["GatewayController"].placeholder;
    std::string srId = dict.names()["SensorReading"].placeholder;
    std::string maId = dict.names()["MovingAverage"].placeholder;

    std::string prose =
        "I suggest refactoring " + gwId + " to use dependency injection. "
        "The " + srId + " struct could benefit from a builder pattern. "
        "Also, " + maId + " should use a circular buffer internally.";

    std::string proseDeanon = svc.restoreText(prose, dict);
    CHECK(proseDeanon.find("GatewayController") != std::string::npos);
    CHECK(proseDeanon.find("SensorReading") != std::string::npos);
    CHECK(proseDeanon.find("MovingAverage") != std::string::npos);
    CHECK(proseDeanon.find(gwId) == std::string::npos);
    CHECK(proseDeanon.find(srId) == std::string::npos);
    CHECK(proseDeanon.find(maId) == std::string::npos);
    fprintf(stderr, "  [PASS] reverseText: placeholders restored in prose\n");

    // ================================================================
    // ТЕСТ 8: Консистентность словаря — один символ → один ID
    // ================================================================
    // Повторно анонимизируем ТОТ ЖЕ исходник с ТЕМ ЖЕ словарём — вывод идентичен
    Dictionary dictCopy = dict;
    std::string anon3 = svc.anonymize(src, dictCopy);
    CHECK(anon3 == anon);
    fprintf(stderr, "  [PASS] Dict consistency: re-anonymize with same dict → same output\n");

    // ================================================================
    // ТЕСТ 9: Межфайловая консистентность — ID символов общие между файлами
    // ================================================================
    // Имитируем второй файл, ссылающийся на те же имена
    std::string src2 = R"cpp(
#include "scada_gateway.h"
#include <iostream>

// Тестовый файл, использующий общие символы
void testGateway() {
    scada::gateway::GatewayController gw("/etc/test.json");
    gw.initialize();
    auto readings = gw.getReadings("TG-1");
    for (const auto& r : readings) {
        std::cout << r.value << " " << r.unit << std::endl;
    }
}
)cpp";

    Dictionary dictMulti = dict;  // начинаем с существующего словаря
    std::string anon4 = svc.anonymize(src2, dictMulti);

    // GatewayController должен получить ТОТ ЖЕ ID, что и в первом файле
    CHECK(anon4.find(gwId) != std::string::npos);
    // Вывод второго файла тоже должен проходить round-trip
    std::string restored2 = svc.restoreCode(anon4, dictMulti);
    CHECK(restored2 == src2);
    fprintf(stderr, "  [PASS] Multi-file: shared symbol IDs, independent round-trip\n");

    // ================================================================
    // ТЕСТ 10: Большое число идентификаторов — нагрузка на словарь
    // ================================================================
    CHECK(dict.names().size() > 30);
    // Проверяем, что все имена имеют валидный формат ID
    std::regex idPattern("^ID_[0-9]{4,}$");
    bool allValid = true;
    for (auto& [name, val] : dict.names()) {
        (void)name;
        std::string id = val.placeholder;
        if (!std::regex_match(id, idPattern)) {
            fprintf(stderr, "  BAD ID: '%s' for name '%s'\n", id.c_str(), name.c_str());
            allValid = false;
        }
    }
    CHECK(allValid);
    fprintf(stderr, "  [PASS] All %u name IDs match ID_NNNN format\n",
            (unsigned)dict.names().size());

    // ================================================================
    // Итог
    // ================================================================
    fprintf(stderr, "\n=== E2E RESULTS: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
