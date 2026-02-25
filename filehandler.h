#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <QObject>
#include <QQueue>

class FileHandler : public QObject
{
    Q_OBJECT

    int xorFile(const QString &filename, QString &note);
    QString getUniqueFileName(const QString &filename);
    void oneTimeStart();
    void timerModeStart();

public:
    explicit FileHandler(QByteArray &xorValue,
                         QString &outputDir,
                         bool deleteInput,
                         bool rewriteRepeated,
                         QObject *parent = nullptr);
    ~FileHandler() = default;

public slots:
    void start();
    void stop() {
        m_stopRequested.store(true);
        emit finished();
    }
    void handleFiles(const QStringList &files);

signals:
    void fileStarted(const QString &file, int status, QString error = "");
    void fileFinished(const QString &file, int status, QString error);
    void allFinished();

    void progressUpdated(int percent);
    void fileError(const QString &file, const QString &msg);
    void finished();
    void requestFiles();


private:

    QByteArray m_xorValue;
    QString m_outputDir;
    bool m_deleteInput;
    bool m_rewriteRepeated;
    std::atomic<bool> m_stopRequested{false};
};

#endif // FILEHANDLER_H
