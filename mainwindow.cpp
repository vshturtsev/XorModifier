#include "mainwindow.h"
#include "filehandler.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    setupInterface();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setupInterface() {
    m_state = ProgramState::WaitStart;
    ui->runningStateLabel->setText(getProgramStateText(m_state));
    ui->xorValue->setText("FFFFFFFFFFFFFFFF");
    ui->handlingFilesWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->handlingFilesWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    connect(ui->addFilesButton, &QPushButton::clicked, this, &MainWindow::onAddFilesClicked);
    connect(ui->outputPathButton,
            &QPushButton::clicked,
            this,
            &MainWindow::onOutputPathButtonClicked);
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(ui->deleteSelected, &QPushButton::clicked, this, &MainWindow::onDeleteSelectedClicked);

    auto *timer = ui->startModeTimer;
    connect(ui->startModeOption, &QComboBox::currentTextChanged, timer, [timer](const QString &text) {
        if (text == "Разовый")
            timer->setDisabled(true);
        else
            timer->setEnabled(true);
    });

}

void MainWindow::onAddFilesClicked() {
    QString filter = ui->fileMaskOption->text();
    QStringList files = QFileDialog::getOpenFileNames(this,
                                                      "Выберите файл или папку",
                                                      m_currentDir,
                                                      filter);
    if (!files.empty()) {
        QListWidget *fileList = ui->pendingFilesWidget;
        for (int i = 0; i < files.count(); ++i) {
            if (fileList->findItems(files[i], Qt::MatchExactly).isEmpty())
                ui->pendingFilesWidget->addItem(files[i]);
        }
        QFileInfo fi(files.at(files.count() - 1));
        m_currentDir = fi.path();
    }
}

void MainWindow::onOutputPathButtonClicked() {
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    "Выберите папку...",
                                                    QDir::currentPath(),
                                                    QFileDialog::ShowDirsOnly);
    ui->outputPath->setText(dir);
}

void MainWindow::onStartClicked() {
    //set settings
    QString errorMsg;
    bool ok = true;
    m_deleteInput = (ui->deleteInputCheckBox->isChecked()) ? true : false;
    m_rewriteRepeated = (ui->repeatedNameOption->currentText() == "Перезаписывать") ? true : false;
    m_xorValue = QByteArray::fromHex(ui->xorValue->text().toLatin1());
    if (m_xorValue.size() != 8) {
        QMessageBox::critical(this,
                              "Ошибка",
                              "Некорректное hex значение для XOR: " + ui->xorValue->text()
                                  + "\nИспользуйте 0-9 и A-F, должны быть введены все 16 символов, "
                                    "по 2 символа на байт.");
        return;
    }
    m_outputDir = ui->outputPath->text();
    ok = validateDir(m_outputDir, errorMsg);
    if (!ok) {
        QMessageBox::critical(this, "Ошибка", errorMsg);
        return;
    }

    m_timerMode = false;
    if (ui->startModeOption->currentText() == "Разовый") {
        if (ui->pendingFilesWidget->count() < 1) {
            QMessageBox::critical(this, "Ошибка", "Нет файлов для разового запуска");
            return;
        }
    } else if (ui->startModeOption->currentText() == "По таймеру (мин)") {
        m_timerMode = true;
    }

    //visual
    ui->startButton->setDisabled(true);
    ui->stopButton->setEnabled(true);

    if (m_timerMode) {
        startHandlerByTimer();
    } else {
        startHandler();
    }
}

void MainWindow::startHandlerByTimer() {
    startHandler();
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(ui->startModeTimer->value() * 60 * 1000);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::startHandlerByTimer);
}

void MainWindow::startHandler() {
    m_state = ProgramState::Processing;
    ui->runningStateLabel->setText(getProgramStateText(m_state));

    m_thread = new QThread(this);
    m_handler = new FileHandler(m_xorValue, m_outputDir, m_deleteInput, m_rewriteRepeated);
    m_handler->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_handler, &FileHandler::start);
    connect(m_handler, &FileHandler::finished, m_thread, &QThread::quit);
    connect(m_handler, &FileHandler::finished, m_handler, &FileHandler::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
    connect(m_thread, &QThread::finished, this, &MainWindow::onHandlingFinished);

    connect(m_handler, &FileHandler::requestFiles, this, &MainWindow::onFilesRequested);
    connect(this, &MainWindow::filesSended, m_handler, &FileHandler::handleFiles);
    connect(m_handler, &FileHandler::fileStarted, this, &MainWindow::onFileStateChanged);
    connect(m_handler, &FileHandler::fileFinished, this, &MainWindow::onFileStateChanged);

    m_thread->start();
}

void MainWindow::onStopClicked() {
    if (m_timer)
        m_timer->stop();
    if (m_handler)
        m_handler->stop();
    m_state = ProgramState::WaitStart;
    ui->runningStateLabel->setText(getProgramStateText(m_state));
    ui->startButton->setEnabled(true);
    ui->stopButton->setDisabled(true);
}

void MainWindow::onHandlingFinished() {
    if (m_timerMode) {
        m_state = ProgramState::WaitTimer;
        ui->runningStateLabel->setText(getProgramStateText(m_state));
    } else {
        ui->startButton->setEnabled(true);
        ui->stopButton->setDisabled(true);
        m_state = ProgramState::ProcessCompleted;
        ui->runningStateLabel->setText(getProgramStateText(m_state));
    }
}

bool MainWindow::validateXorValue(const QByteArray &byteArray) {
    size_t size = byteArray.size();
    if (size != 8)
        return false;
    else
        return true;
}

bool MainWindow::validateDir(const QString &path, QString &errorMsg) {
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isDir()) {
        errorMsg = "Указанная директория для сохранения файлов не существует.";
        return false;
    }
    if (!fi.isWritable()) {
        errorMsg = "Нет прав для записи в директорию для сохранения.";
        return false;
    }
    return true;
}

void MainWindow::onFilesRequested() {
    QStringList files;
    QTableWidget *handlingFiles = ui->handlingFilesWidget;
    for (int i = ui->pendingFilesWidget->count() - 1; i >= 0; --i) {
        auto *item = ui->pendingFilesWidget->item(i);
        QString file = item->text();
        files.append(file);
        handlingFiles->insertRow(0);
        handlingFiles->setItem(0, 0, new QTableWidgetItem(file));
        handlingFiles->setItem(0, 1, new QTableWidgetItem("Отправлен"));
        handlingFiles->setItem(0, 2, new QTableWidgetItem(""));
    }
    ui->pendingFilesWidget->clear();
    emit filesSended(files);
}

void MainWindow::onFileStateChanged(const QString &file, int status, QString error) {
    QTableWidget *handlingFiles = ui->handlingFilesWidget;
    for (int i = 0; i < handlingFiles->rowCount(); ++i) {
        if (handlingFiles->item(i, 0)->text() == file) {
            QString state = getFileStateText(status);
            handlingFiles->item(i, 1)->setText(state);
            handlingFiles->item(i, 2)->setText(error);
            break;
        }
    }
}

QString MainWindow::getFileStateText(int status) {
    switch (status) {
    case 0:
        return "Обработан";
    case 1:
        return "В работе";
    case 2:
        return "Ошибка";
    default:
        return "Не известно";
    }
}

QString MainWindow::getProgramStateText(MainWindow::ProgramState state) {
    switch (state) {
    case MainWindow::ProgramState::WaitStart:
        return "Ожидает запуска";
    case MainWindow::ProgramState::Processing:
        return "Выполняется обработка";
    case MainWindow::ProgramState::ProcessCompleted:
        return "Обработка завершена. Ожидает запуска...";
    case MainWindow::ProgramState::WaitTimer:
        return "Обработка завершена. Ждет следующего запуска по таймеру";
    case MainWindow::ProgramState::Failed:
        return "Ошибка";
    default:
        return "Неизвестное состояние";
    }
}

void MainWindow::onDeleteSelectedClicked() {
    auto items = ui->pendingFilesWidget->selectedItems();
    for (int i = 0; i < items.count(); ++i) {
        delete items[i];
    }
}
