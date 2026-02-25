#include "filehandler.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include <QCoreApplication>

FileHandler::FileHandler(QByteArray &xorValue,
                         QString &outputDir,
                         bool deleteInput,
                         bool rewriteRepeated,
                         QObject *parent)
    : QObject(parent)
    , m_xorValue(xorValue)
    , m_outputDir(outputDir)
    , m_deleteInput(deleteInput)
    , m_rewriteRepeated(rewriteRepeated) {}

void FileHandler::start() {
    emit requestFiles();
}

void FileHandler::handleFiles(const QStringList &files) {
    for (int i = 0; i < files.count(); ++i) {
        if (QThread::currentThread()->isInterruptionRequested() || m_stopRequested)
            break;
        QString file = files[i];
        emit fileStarted(file, 1);
        QFileInfo fileInfo(file);
        if (!fileInfo.exists()) {
            emit fileFinished(file, 2, "Файл не существует");
            continue;
        }
        if (!fileInfo.isReadable()) {
            emit fileFinished(file, 2, "Нет доступа на чтение");
            continue;
        }
        QString note;
        int error = xorFile(file, note);

        emit fileFinished(file, error, note);
    }
    QThread::msleep(1000);
    emit finished();
}

int FileHandler::xorFile(const QString &filename, QString &note) {
    QFileInfo fileInfo(filename);
    QFile source(filename);
    if (!source.open(QIODevice::ReadOnly)) {
        note = "Файл не доступен для чтения";
        return 2;
    }
    QString newFilename = m_outputDir + "/" + fileInfo.fileName();
    if (!m_rewriteRepeated) {
        newFilename = getUniqueFileName(newFilename);
    }
    QFile dest(newFilename);
    if (!dest.open(QIODevice::WriteOnly)) {
        note = "Не удалось создать файл для записи";
        return 2;
    }
    QByteArray data;
    while (!source.atEnd()) {
        data = source.read(8);
        for (int i = 0; i < m_xorValue.size() && i < data.size(); ++i) {
            data[i] ^= m_xorValue[i];
        }
        dest.write(data);
    }

    if (m_deleteInput) {
        source.close();
        source.remove();
    }
    note = newFilename;

    return 0;
}

QString FileHandler::getUniqueFileName(const QString &filename) {
    if (!QFile::exists(filename))
        return filename;

    QFileInfo fi(filename);
    QString baseName = fi.completeBaseName();
    QString ext = fi.suffix();
    QString path = fi.path();

    int counter = 1;
    QString newFileName;

    do {
        QString newName = QString("%1_%2.%3").arg(baseName).arg(counter).arg(ext);
        newFileName = path + "/" + newName;
        ++counter;
    } while (QFile::exists(newFileName));
    return newFileName;
}
