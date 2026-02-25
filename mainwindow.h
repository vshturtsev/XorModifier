#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "filehandler.h"

#include <QMainWindow>
#include <QDir>
#include <QThread>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT


    void setupInterface();
    bool validateXorValue(const QByteArray &byteArray);
    bool validateDir(const QString &path, QString &errorMsg);
    QString getFileStateText(int status);
    void startHandler();
    void startHandlerByTimer();

public:
    enum class ProgramState {
        WaitStart,          //ожидает запуска
        Processing,         //выполняется обработка
        ProcessCompleted,   //обработка завершена, ожидает запуска
        WaitTimer,          //обработка завершена, ожидает таймер
        Failed
    };

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    ProgramState getProgramState() {return m_state;};
    QString getProgramStateText(ProgramState state);


private slots:
    void onAddFilesClicked();
    void onOutputPathButtonClicked();
    void onStartClicked();
    void onStopClicked();
    void onFilesRequested();
    void onFileStateChanged(const QString &file, int status, QString error = "");
    void onHandlingFinished();
    void onDeleteSelectedClicked();

signals:

    void filesSended(const QStringList &files);

private:
    Ui::MainWindow *ui;
    QString m_currentDir = QDir::rootPath(); //QDir::rootPath();
    FileHandler *m_handler;
    QThread *m_thread;
    QTimer *m_timer;
    bool m_deleteInput;
    bool m_rewriteRepeated;
    QByteArray m_xorValue;
    QString m_outputDir;
    bool m_timerMode;
    ProgramState m_state{ProgramState::WaitStart};

};
#endif // MAINWINDOW_H
