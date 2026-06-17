// mainwindow.h — Qt5 GUI для анонимизатора C++ (presentation-слой).
#pragma once
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include <QDockWidget>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QTranslator>
#include "application/AnonymizerService.h"
#include "infrastructure/JsonFileDictionaryStore.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* e) override;   // ловит QEvent::LanguageChange

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
    void retranslateUi();                       // переустановка всех текстов UI
    void switchLanguage(const QString& code);   // "ru" / "en"
    void applyTranslators(const QString& code); // (раз)установка QTranslator
    void updateDictStats();
    void setModified(bool m);
    bool ensureDictSaved();
    QString dictFilterString() const;

    // --- Виджеты ---
    QPlainTextEdit* m_srcEdit;      // левая панель: исходник / ввод
    QPlainTextEdit* m_resultEdit;   // правая панель: анонимизированный / вывод
    QLabel*         m_srcLabel;
    QLabel*         m_resultLabel;
    QComboBox*      m_modeCombo;    // opaque / format
    QLabel*         m_modeLabel;
    QLabel*         m_dictStatsLabel;
    QTableWidget*   m_leakTable;
    QDockWidget*    m_leakDock;
    QToolBar*       m_toolBar;

    // --- Меню ---
    QMenu* m_fileMenu;
    QMenu* m_dictMenu;
    QMenu* m_clipMenu;
    QMenu* m_toolsMenu;
    QMenu* m_langMenu;

    // --- Действия (хранятся для retranslateUi) ---
    QAction* m_actOpen;       QAction* m_actSaveResult;
    QAction* m_actBatch;      QAction* m_actQuit;
    QAction* m_actNewDict;    QAction* m_actLoadDict;
    QAction* m_actSaveDict;   QAction* m_actSaveDictAs;
    QAction* m_actClipAnon;   QAction* m_actClipDeanon;
    QAction* m_actLeakMenu;
    QAction* m_actForward;    QAction* m_actReverse;
    QAction* m_actReverseText; QAction* m_actLeakTb;
    QAction* m_actLangRu;     QAction* m_actLangEn;

    // --- Локализация ---
    QString    m_locale;        // текущий код языка ("ru" / "en")
    QTranslator m_appTranslator; // переводы приложения (:/i18n/anonymizer_*.qm)
    QTranslator m_qtTranslator;  // переводы стандартных диалогов Qt (qtbase_*)

    // --- Состояние ---
    application::AnonymizerService        m_service;   // фасад use-cases
    infrastructure::JsonFileDictionaryStore m_store;   // хранилище словаря
    domain::Dictionary                    m_dict;      // доменный словарь
    QString         m_dictPath;
    bool            m_dictModified = false;
    QString         m_lastOpenDir;
};
