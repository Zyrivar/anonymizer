// mainwindow.h — Qt5 GUI для анонимизатора C++ (presentation-слой).
#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include <QDockWidget>
#include <QAction>
#include "application/AnonymizerService.h"
#include "infrastructure/JsonFileDictionaryStore.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    // Основные операции
    void doForward();
    void doReverse();
    void doReverseText();
    void doLeakScan();

    // Интеграция с буфером обмена (замена старого anonctl)
    void clipboardAnon();
    void clipboardDeanon();

    // Операции с файлами
    void openFile();
    void saveResult();
    void batchProcess();

    // Управление словарём
    void newDict();
    void loadDict();
    void saveDict();
    void saveDictAs();

private:
    void setupUi();
    void setupMenus();
    void setupToolBar();
    void setupDock();
    void updateDictStats();
    void setModified(bool m);
    bool ensureDictSaved();
    QString dictFilterString() const;

    // --- Виджеты ---
    QPlainTextEdit* m_srcEdit;      // левая панель: исходник / ввод
    QPlainTextEdit* m_resultEdit;   // правая панель: анонимизированный / вывод
    QComboBox*      m_modeCombo;    // opaque / format
    QLabel*         m_dictStatsLabel;
    QTableWidget*   m_leakTable;
    QDockWidget*    m_leakDock;

    // --- Состояние ---
    application::AnonymizerService        m_service;   // фасад use-cases
    infrastructure::JsonFileDictionaryStore m_store;   // хранилище словаря
    domain::Dictionary                    m_dict;      // доменный словарь
    QString         m_dictPath;
    bool            m_dictModified = false;
    QString         m_lastOpenDir;
};
