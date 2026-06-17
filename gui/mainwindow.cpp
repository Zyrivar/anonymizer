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
#include <fstream>
#include <sstream>


// ---------------------------------------------------------------------------
// Конструирование
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenus();
    setupToolBar();
    setupDock();
    updateDictStats();

    setWindowTitle(tr("C++ Anonymizer"));
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

    m_srcEdit->setPlaceholderText(tr("Source C++ code (paste or open file)"));
    m_resultEdit->setPlaceholderText(tr("Anonymized / restored output"));
    m_resultEdit->setReadOnly(true);

    // Подписи над редакторами
    auto* leftLabel  = new QLabel(tr("Source"));
    auto* rightLabel = new QLabel(tr("Result"));
    QFont bold = leftLabel->font();
    bold.setBold(true);
    leftLabel->setFont(bold);
    rightLabel->setFont(bold);

    auto* leftPanel = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(leftLabel);
    leftLayout->addWidget(m_srcEdit);

    auto* rightPanel = new QWidget;
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(rightLabel);
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
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    fileMenu->addAction(tr("&Open source..."), this, &MainWindow::openFile,
                        QKeySequence::Open);
    fileMenu->addAction(tr("&Save result..."), this, &MainWindow::saveResult,
                        QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Batch process..."), this, &MainWindow::batchProcess);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), this, &QWidget::close, QKeySequence::Quit);

    // --- Меню Dictionary ---
    auto* dictMenu = menuBar()->addMenu(tr("&Dictionary"));

    dictMenu->addAction(tr("&New dictionary"),   this, &MainWindow::newDict);
    dictMenu->addAction(tr("&Load..."),          this, &MainWindow::loadDict);
    dictMenu->addAction(tr("&Save"),             this, &MainWindow::saveDict);
    dictMenu->addAction(tr("Save &as..."),       this, &MainWindow::saveDictAs);

    // --- Меню Clipboard ---
    auto* clipMenu = menuBar()->addMenu(tr("&Clipboard"));

    clipMenu->addAction(tr("Anonymize clipboard"),   this, &MainWindow::clipboardAnon,
                        QKeySequence(tr("Ctrl+Shift+A")));
    clipMenu->addAction(tr("Deanonymize clipboard"), this, &MainWindow::clipboardDeanon,
                        QKeySequence(tr("Ctrl+Shift+D")));

    // --- Меню Tools ---
    auto* toolsMenu = menuBar()->addMenu(tr("&Tools"));

    toolsMenu->addAction(tr("Leak scan"),  this, &MainWindow::doLeakScan);
}

void MainWindow::setupToolBar()
{
    auto* tb = addToolBar(tr("Operations"));
    tb->setMovable(false);

    tb->addAction(tr("Forward"),      this, &MainWindow::doForward);
    tb->addAction(tr("Reverse"),      this, &MainWindow::doReverse);
    tb->addAction(tr("Reverse Text"), this, &MainWindow::doReverseText);
    tb->addSeparator();
    tb->addAction(tr("Leak Scan"),    this, &MainWindow::doLeakScan);
    tb->addSeparator();

    // Выбор режима
    tb->addWidget(new QLabel(tr("  Mode: ")));
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem(tr("opaque"),  false);
    m_modeCombo->addItem(tr("format"),  true);
    m_modeCombo->setToolTip(
        tr("opaque: whole strings → STR_*\n"
           "format: keep structure, scrub sensitive substrings only"));
    tb->addWidget(m_modeCombo);
}

void MainWindow::setupDock()
{
    m_leakTable = new QTableWidget(0, 3);
    m_leakTable->setHorizontalHeaderLabels({tr("Line"), tr("Pattern"), tr("Value")});
    m_leakTable->horizontalHeader()->setStretchLastSection(true);
    m_leakTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_leakTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_leakTable->verticalHeader()->setVisible(false);

    m_leakDock = new QDockWidget(tr("Leak Report"), this);
    m_leakDock->setWidget(m_leakTable);
    m_leakDock->setVisible(false);
    addDockWidget(Qt::BottomDockWidgetArea, m_leakDock);
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
        tr("Forward done (%1)").arg(fmt ? "format" : "opaque"), 3000);
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

    setModified(true);
    updateDictStats();

    QMessageBox::information(this, tr("Batch complete"),
        tr("Processed %1 file(s)\n\n"
           "Names: %2\nStrings: %3\nScrub: %4")
            .arg(count)
            .arg(static_cast<int>(m_dict.names().size()))
            .arg(static_cast<int>(m_dict.strings().size()))
            .arg(static_cast<int>(m_dict.scrub().size())));
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
