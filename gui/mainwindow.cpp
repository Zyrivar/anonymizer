// mainwindow.cpp — реализация Qt5 GUI для анонимизатора C++.
#include "mainwindow.h"
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFont>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QActionGroup>
#include <QSettings>
#include <QLibraryInfo>
#include <QEvent>
#include <fstream>
#include <sstream>


// ---------------------------------------------------------------------------
// Конструирование
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Язык интерфейса: сохранённый выбор или русский по умолчанию.
    QSettings settings;
    m_locale = settings.value("ui/language", "ru").toString();
    applyTranslators(m_locale);

    setupUi();
    setupMenus();
    setupToolBar();
    setupDock();
    retranslateUi();        // установка всех текстов под текущий язык
    updateDictStats();

    resize(1100, 680);
}

// ---------------------------------------------------------------------------
// Настройка интерфейса
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    // Двухпанельный редактор с моноширинным шрифтом
    m_srcEdit    = new QPlainTextEdit;
    m_resultEdit = new QPlainTextEdit;

    QFont mono("Consolas", 10);
    mono.setStyleHint(QFont::Monospace);
    m_srcEdit->setFont(mono);
    m_resultEdit->setFont(mono);
    m_resultEdit->setReadOnly(true);

    // Подписи над редакторами (текст — в retranslateUi)
    m_srcLabel    = new QLabel;
    m_resultLabel = new QLabel;
    QFont bold = m_srcLabel->font();
    bold.setBold(true);
    m_srcLabel->setFont(bold);
    m_resultLabel->setFont(bold);

    auto* leftPanel = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_srcLabel);
    leftLayout->addWidget(m_srcEdit);

    auto* rightPanel = new QWidget;
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_resultLabel);
    rightLayout->addWidget(m_resultEdit);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(splitter);

    // Строка состояния со статистикой словаря
    m_dictStatsLabel = new QLabel;
    statusBar()->addPermanentWidget(m_dictStatsLabel);
}

void MainWindow::setupMenus()
{
    // --- Меню File ---
    m_fileMenu = menuBar()->addMenu(QString());

    m_actOpen = new QAction(this);
    m_actOpen->setShortcut(QKeySequence::Open);
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::openFile);
    m_fileMenu->addAction(m_actOpen);

    m_actSaveResult = new QAction(this);
    m_actSaveResult->setShortcut(QKeySequence::Save);
    connect(m_actSaveResult, &QAction::triggered, this, &MainWindow::saveResult);
    m_fileMenu->addAction(m_actSaveResult);

    m_fileMenu->addSeparator();

    m_actBatch = new QAction(this);
    connect(m_actBatch, &QAction::triggered, this, &MainWindow::batchProcess);
    m_fileMenu->addAction(m_actBatch);

    m_fileMenu->addSeparator();

    m_actQuit = new QAction(this);
    m_actQuit->setShortcut(QKeySequence::Quit);
    connect(m_actQuit, &QAction::triggered, this, &QWidget::close);
    m_fileMenu->addAction(m_actQuit);

    // --- Меню Dictionary ---
    m_dictMenu = menuBar()->addMenu(QString());

    m_actNewDict = new QAction(this);
    connect(m_actNewDict, &QAction::triggered, this, &MainWindow::newDict);
    m_dictMenu->addAction(m_actNewDict);

    m_actLoadDict = new QAction(this);
    connect(m_actLoadDict, &QAction::triggered, this, &MainWindow::loadDict);
    m_dictMenu->addAction(m_actLoadDict);

    m_actSaveDict = new QAction(this);
    connect(m_actSaveDict, &QAction::triggered, this, &MainWindow::saveDict);
    m_dictMenu->addAction(m_actSaveDict);

    m_actSaveDictAs = new QAction(this);
    connect(m_actSaveDictAs, &QAction::triggered, this, &MainWindow::saveDictAs);
    m_dictMenu->addAction(m_actSaveDictAs);

    // --- Меню Clipboard ---
    m_clipMenu = menuBar()->addMenu(QString());

    m_actClipAnon = new QAction(this);
    m_actClipAnon->setShortcut(QKeySequence("Ctrl+Shift+A"));
    connect(m_actClipAnon, &QAction::triggered, this, &MainWindow::clipboardAnon);
    m_clipMenu->addAction(m_actClipAnon);

    m_actClipDeanon = new QAction(this);
    m_actClipDeanon->setShortcut(QKeySequence("Ctrl+Shift+D"));
    connect(m_actClipDeanon, &QAction::triggered, this, &MainWindow::clipboardDeanon);
    m_clipMenu->addAction(m_actClipDeanon);

    // --- Меню Tools ---
    m_toolsMenu = menuBar()->addMenu(QString());

    m_actLeakMenu = new QAction(this);
    connect(m_actLeakMenu, &QAction::triggered, this, &MainWindow::doLeakScan);
    m_toolsMenu->addAction(m_actLeakMenu);

    // --- Меню Language (выбор языка интерфейса) ---
    m_langMenu = menuBar()->addMenu(QString());

    auto* langGroup = new QActionGroup(this);   // взаимоисключающий выбор
    langGroup->setExclusive(true);

    // Эндонимы языков не переводятся (всегда на своём языке).
    m_actLangRu = new QAction(QStringLiteral("Русский"), this);
    m_actLangRu->setCheckable(true);
    langGroup->addAction(m_actLangRu);
    m_langMenu->addAction(m_actLangRu);
    connect(m_actLangRu, &QAction::triggered, this, [this] { switchLanguage("ru"); });

    m_actLangEn = new QAction(QStringLiteral("English"), this);
    m_actLangEn->setCheckable(true);
    langGroup->addAction(m_actLangEn);
    m_langMenu->addAction(m_actLangEn);
    connect(m_actLangEn, &QAction::triggered, this, [this] { switchLanguage("en"); });
}

void MainWindow::setupToolBar()
{
    m_toolBar = addToolBar(QString());
    m_toolBar->setMovable(false);

    m_actForward = new QAction(this);
    connect(m_actForward, &QAction::triggered, this, &MainWindow::doForward);
    m_toolBar->addAction(m_actForward);

    m_actReverse = new QAction(this);
    connect(m_actReverse, &QAction::triggered, this, &MainWindow::doReverse);
    m_toolBar->addAction(m_actReverse);

    m_actReverseText = new QAction(this);
    connect(m_actReverseText, &QAction::triggered, this, &MainWindow::doReverseText);
    m_toolBar->addAction(m_actReverseText);

    m_toolBar->addSeparator();

    m_actLeakTb = new QAction(this);
    connect(m_actLeakTb, &QAction::triggered, this, &MainWindow::doLeakScan);
    m_toolBar->addAction(m_actLeakTb);

    m_toolBar->addSeparator();

    // Выбор режима
    m_modeLabel = new QLabel;
    m_toolBar->addWidget(m_modeLabel);
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem(QString(), false);   // opaque (текст — в retranslateUi)
    m_modeCombo->addItem(QString(), true);    // format
    m_toolBar->addWidget(m_modeCombo);
}

void MainWindow::setupDock()
{
    m_leakTable = new QTableWidget(0, 3);
    m_leakTable->horizontalHeader()->setStretchLastSection(true);
    m_leakTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_leakTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_leakTable->verticalHeader()->setVisible(false);

    m_leakDock = new QDockWidget(this);
    m_leakDock->setWidget(m_leakTable);
    m_leakDock->setVisible(false);
    addDockWidget(Qt::BottomDockWidgetArea, m_leakDock);
}

// ---------------------------------------------------------------------------
// Локализация
// ---------------------------------------------------------------------------

void MainWindow::applyTranslators(const QString& code)
{
    qApp->removeTranslator(&m_appTranslator);
    qApp->removeTranslator(&m_qtTranslator);

    if (code == "ru") {
        // Переводы приложения встроены в ресурсы (:/i18n/anonymizer_ru.qm).
        if (m_appTranslator.load(QStringLiteral(":/i18n/anonymizer_ru")))
            qApp->installTranslator(&m_appTranslator);

        // Стандартные диалоги Qt (кнопки Save/Cancel, файловый диалог и т.д.).
        const QString qtDir = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
        if (m_qtTranslator.load(QStringLiteral("qtbase_ru"), qtDir) ||
            m_qtTranslator.load(QStringLiteral("qtbase_ru"),
                                qApp->applicationDirPath() + "/translations"))
            qApp->installTranslator(&m_qtTranslator);
    }
    // Для "en" переводчики сняты — остаются исходные английские строки.
}

void MainWindow::switchLanguage(const QString& code)
{
    if (code == m_locale) return;
    m_locale = code;
    QSettings().setValue("ui/language", code);
    applyTranslators(code);
    retranslateUi();   // немедленно обновляем тексты
}

void MainWindow::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QMainWindow::changeEvent(e);
}

void MainWindow::retranslateUi()
{
    // --- Центральная область ---
    m_srcLabel->setText(tr("Source"));
    m_resultLabel->setText(tr("Result"));
    m_srcEdit->setPlaceholderText(tr("Source C++ code (paste or open file)"));
    m_resultEdit->setPlaceholderText(tr("Anonymized / restored output"));

    // --- Меню File ---
    m_fileMenu->setTitle(tr("&File"));
    m_actOpen->setText(tr("&Open source..."));
    m_actSaveResult->setText(tr("&Save result..."));
    m_actBatch->setText(tr("&Batch process..."));
    m_actQuit->setText(tr("&Quit"));

    // --- Меню Dictionary ---
    m_dictMenu->setTitle(tr("&Dictionary"));
    m_actNewDict->setText(tr("&New dictionary"));
    m_actLoadDict->setText(tr("&Load..."));
    m_actSaveDict->setText(tr("&Save"));
    m_actSaveDictAs->setText(tr("Save &as..."));

    // --- Меню Clipboard ---
    m_clipMenu->setTitle(tr("&Clipboard"));
    m_actClipAnon->setText(tr("Anonymize clipboard"));
    m_actClipDeanon->setText(tr("Deanonymize clipboard"));

    // --- Меню Tools ---
    m_toolsMenu->setTitle(tr("&Tools"));
    m_actLeakMenu->setText(tr("Leak scan"));

    // --- Меню Language ---
    m_langMenu->setTitle(tr("&Language"));
    m_actLangRu->setChecked(m_locale == "ru");
    m_actLangEn->setChecked(m_locale == "en");

    // --- Тулбар ---
    m_toolBar->setWindowTitle(tr("Operations"));
    m_actForward->setText(tr("Forward"));
    m_actReverse->setText(tr("Reverse"));
    m_actReverseText->setText(tr("Reverse Text"));
    m_actLeakTb->setText(tr("Leak Scan"));
    m_modeLabel->setText(tr("  Mode: "));
    m_modeCombo->setItemText(0, tr("opaque"));
    m_modeCombo->setItemText(1, tr("format"));
    m_modeCombo->setToolTip(
        tr("opaque: whole strings → STR_*\n"
           "format: keep structure, scrub sensitive substrings only"));

    // --- Док утечек ---
    m_leakDock->setWindowTitle(tr("Leak Report"));
    m_leakTable->setHorizontalHeaderLabels({tr("Line"), tr("Pattern"), tr("Value")});

    // --- Заголовок окна и статистика (тексты на tr()) ---
    setModified(m_dictModified);
    updateDictStats();
}

// ---------------------------------------------------------------------------
// Основные операции
// ---------------------------------------------------------------------------

void MainWindow::doForward()
{
    const std::string src = m_srcEdit->toPlainText().toStdString();
    if (src.empty()) {
        statusBar()->showMessage(tr("Source is empty"), 3000);
        return;
    }

    bool fmt = m_modeCombo->currentData().toBool();
    auto mode = fmt ? application::StringMode::Format : application::StringMode::Opaque;
    std::string result = m_service.anonymize(src, m_dict, mode);
    m_resultEdit->setPlainText(QString::fromStdString(result));
    setModified(true);
    updateDictStats();
    statusBar()->showMessage(
        tr("Forward done (%1)").arg(fmt ? tr("format") : tr("opaque")), 3000);
}

void MainWindow::doReverse()
{
    const std::string src = m_srcEdit->toPlainText().toStdString();
    if (src.empty()) {
        statusBar()->showMessage(tr("Source is empty"), 3000);
        return;
    }
    std::string result = m_service.restoreCode(src, m_dict);
    m_resultEdit->setPlainText(QString::fromStdString(result));
    statusBar()->showMessage(tr("Reverse done"), 3000);
}

void MainWindow::doReverseText()
{
    const std::string src = m_srcEdit->toPlainText().toStdString();
    if (src.empty()) {
        statusBar()->showMessage(tr("Source is empty"), 3000);
        return;
    }
    std::string result = m_service.restoreText(src, m_dict);
    m_resultEdit->setPlainText(QString::fromStdString(result));
    statusBar()->showMessage(tr("Reverse-text done"), 3000);
}

void MainWindow::doLeakScan()
{
    const std::string src = m_srcEdit->toPlainText().toStdString();
    if (src.empty()) {
        statusBar()->showMessage(tr("Source is empty"), 3000);
        return;
    }

    auto leaks = m_service.audit(src);

    m_leakTable->setRowCount(0);
    m_leakTable->setRowCount(static_cast<int>(leaks.size()));

    for (int i = 0; i < static_cast<int>(leaks.size()); ++i) {
        m_leakTable->setItem(i, 0, new QTableWidgetItem(QString::number(leaks[i].line)));
        m_leakTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(leaks[i].pattern)));
        m_leakTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(leaks[i].value)));
    }

    m_leakDock->setVisible(true);
    m_leakDock->setWindowTitle(
        tr("Leak Report — %1 finding(s)").arg(leaks.size()));
    statusBar()->showMessage(
        tr("Leak scan: %1 finding(s)").arg(leaks.size()), 5000);
}

// ---------------------------------------------------------------------------
// Интеграция с буфером обмена
// ---------------------------------------------------------------------------

void MainWindow::clipboardAnon()
{
    QClipboard* cb = QApplication::clipboard();
    const std::string text = cb->text().toStdString();
    if (text.empty()) {
        statusBar()->showMessage(tr("Clipboard is empty"), 3000);
        return;
    }

    bool fmt = m_modeCombo->currentData().toBool();
    auto mode = fmt ? application::StringMode::Format : application::StringMode::Opaque;
    std::string result = m_service.anonymize(text, m_dict, mode);
    cb->setText(QString::fromStdString(result));
    setModified(true);
    updateDictStats();
    statusBar()->showMessage(tr("Clipboard anonymized"), 3000);
}

void MainWindow::clipboardDeanon()
{
    QClipboard* cb = QApplication::clipboard();
    const std::string text = cb->text().toStdString();
    if (text.empty()) {
        statusBar()->showMessage(tr("Clipboard is empty"), 3000);
        return;
    }

    std::string result = m_service.restoreText(text, m_dict);
    cb->setText(QString::fromStdString(result));
    statusBar()->showMessage(tr("Clipboard deanonymized"), 3000);
}

// ---------------------------------------------------------------------------
// Операции с файлами
// ---------------------------------------------------------------------------

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open C++ source"), m_lastOpenDir,
        tr("C++ files (*.cpp *.cc *.cxx *.h *.hpp *.hxx *.hh);;All files (*)"));
    if (path.isEmpty()) return;

    m_lastOpenDir = QFileInfo(path).absolutePath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot open file:\n%1").arg(path));
        return;
    }
    m_srcEdit->setPlainText(QString::fromUtf8(f.readAll()));
    statusBar()->showMessage(tr("Opened: %1").arg(QFileInfo(path).fileName()), 3000);
}

void MainWindow::saveResult()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save result"), m_lastOpenDir,
        tr("C++ files (*.cpp *.h);;Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot write file:\n%1").arg(path));
        return;
    }
    f.write(m_resultEdit->toPlainText().toUtf8());
    statusBar()->showMessage(tr("Saved: %1").arg(QFileInfo(path).fileName()), 3000);
}

void MainWindow::batchProcess()
{
    const QString inDir = QFileDialog::getExistingDirectory(
        this, tr("Select source directory"), m_lastOpenDir);
    if (inDir.isEmpty()) return;

    const QString outDir = QFileDialog::getExistingDirectory(
        this, tr("Select output directory"), inDir);
    if (outDir.isEmpty()) return;

    // Защита от записи «на месте»: если вывод совпадает с исходным каталогом
    // или лежит внутри него, исходники будут перезаписаны. Без сохранённого
    // словаря это необратимо — требуем явного подтверждения.
    const QString inCanon  = QDir(inDir).canonicalPath();
    const QString outCanon = QDir(outDir).canonicalPath();
    const bool sameTree =
        QString::compare(outCanon, inCanon, Qt::CaseInsensitive) == 0 ||
        outCanon.startsWith(inCanon + QLatin1Char('/'), Qt::CaseInsensitive);
    if (sameTree) {
        const auto btn = QMessageBox::warning(
            this, tr("Overwrite source files?"),
            tr("The output directory is inside the source directory.\n"
               "Source files will be overwritten in place — this cannot be undone\n"
               "unless the dictionary is saved.\n\nContinue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) return;
    }

    bool fmt = m_modeCombo->currentData().toBool();
    int count = 0;

    // Собираем файлы
    QDirIterator it(inDir, {"*.cpp","*.cc","*.cxx","*.c++","*.h","*.hpp","*.hxx","*.hh"},
                    QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        const QString absPath = it.filePath();
        const QString relPath = QDir(inDir).relativeFilePath(absPath);

        // Читаем исходник
        QFile fin(absPath);
        if (!fin.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        std::string src = QString::fromUtf8(fin.readAll()).toStdString();
        fin.close();

        // Анонимизируем
        auto mode = fmt ? application::StringMode::Format : application::StringMode::Opaque;
        std::string result = m_service.anonymize(src, m_dict, mode);

        // Пишем вывод (сохраняя относительную структуру каталогов)
        const QString outPath = QDir(outDir).filePath(relPath);
        QDir().mkpath(QFileInfo(outPath).absolutePath());
        QFile fout(outPath);
        if (!fout.open(QIODevice::WriteOnly | QIODevice::Text)) continue;
        fout.write(QString::fromStdString(result).toUtf8());
        ++count;
    }

    // Автосохранение словаря: без него анонимизированный результат необратим.
    // Если путь словаря уже задан — пишем туда; иначе кладём файл по умолчанию
    // в каталог вывода (рядом с анонимизированными файлами).
    QString dictMsg;
    try {
        if (m_dictPath.isEmpty())
            m_dictPath = QDir(outDir).filePath(QStringLiteral("anonymizer_dict.json"));
        m_store.save(m_dictPath.toStdString(), m_dict);
        setModified(false);
        dictMsg = tr("Dictionary saved: %1").arg(m_dictPath);
    } catch (const std::exception& e) {
        setModified(true);   // оставляем пометку несохранённого
        dictMsg = tr("Could not save dictionary:\n%1").arg(e.what());
    }
    updateDictStats();

    QMessageBox::information(this, tr("Batch complete"),
        tr("Processed %1 file(s)\n\n"
           "Names: %2\nStrings: %3\nScrub: %4")
            .arg(count)
            .arg(static_cast<int>(m_dict.names().size()))
            .arg(static_cast<int>(m_dict.strings().size()))
            .arg(static_cast<int>(m_dict.scrub().size()))
        + "\n\n" + dictMsg);
}

// ---------------------------------------------------------------------------
// Управление словарём
// ---------------------------------------------------------------------------

void MainWindow::newDict()
{
    if (!ensureDictSaved()) return;
    m_dict.clear();
    m_dictPath.clear();
    setModified(false);
    updateDictStats();
    statusBar()->showMessage(tr("New dictionary created"), 3000);
}

void MainWindow::loadDict()
{
    if (!ensureDictSaved()) return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load dictionary"), m_lastOpenDir, dictFilterString());
    if (path.isEmpty()) return;

    try {
        m_dict = m_store.load(path.toStdString());
        m_dictPath = path;
        setModified(false);
        updateDictStats();
        statusBar()->showMessage(
            tr("Dictionary loaded: %1").arg(QFileInfo(path).fileName()), 3000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to load dictionary:\n%1").arg(e.what()));
    }
}

void MainWindow::saveDict()
{
    if (m_dictPath.isEmpty()) {
        saveDictAs();
        return;
    }
    try {
        m_store.save(m_dictPath.toStdString(), m_dict);
        setModified(false);
        statusBar()->showMessage(
            tr("Dictionary saved: %1").arg(QFileInfo(m_dictPath).fileName()), 3000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot write dictionary:\n%1").arg(e.what()));
    }
}

void MainWindow::saveDictAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save dictionary as"), m_lastOpenDir, dictFilterString());
    if (path.isEmpty()) return;

    m_dictPath = path;
    saveDict();
}

// ---------------------------------------------------------------------------
// Вспомогательные функции
// ---------------------------------------------------------------------------

void MainWindow::updateDictStats()
{
    m_dictStatsLabel->setText(
        tr("Dict: names=%1  strings=%2  comments=%3  includes=%4  scrub=%5")
            .arg(static_cast<int>(m_dict.names().size()))
            .arg(static_cast<int>(m_dict.strings().size()))
            .arg(static_cast<int>(m_dict.comments().size()))
            .arg(static_cast<int>(m_dict.includes().size()))
            .arg(static_cast<int>(m_dict.scrub().size())));
}

void MainWindow::setModified(bool m)
{
    m_dictModified = m;
    const QString base = tr("C++ Anonymizer");
    if (m_dictPath.isEmpty())
        setWindowTitle(m ? base + " *" : base);
    else
        setWindowTitle(QFileInfo(m_dictPath).fileName()
                       + (m ? " * — " : " — ") + base);
}

bool MainWindow::ensureDictSaved()
{
    if (!m_dictModified) return true;
    auto btn = QMessageBox::question(
        this, tr("Unsaved dictionary"),
        tr("The current dictionary has unsaved changes.\n"
           "Save before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (btn == QMessageBox::Cancel) return false;
    if (btn == QMessageBox::Save)   saveDict();
    return true;
}

QString MainWindow::dictFilterString() const
{
    return tr("JSON files (*.json);;All files (*)");
}
