#ifndef XASTIR_H
#define XASTIR_H

#include <QMainWindow>
#include <QtNetwork>
#include "netinterface.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void connectToServer();
    void newData(QString);
    void closeConnection();
    void statusChanged(PacketInterface::Device_Status newState);

protected:
    void changeEvent(QEvent *e);

private:
    Ui::MainWindow *ui;

    QTcpSocket tcpSocket;
    NetInterface netInterface;
    QString packetDisplay;
    int total_lines;
};

#endif // XASTIR_H
