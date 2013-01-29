#ifndef XASTIR_H
#define XASTIR_H

#include <QMainWindow>
#include <QtNetwork>
#include "packetinterface.h"
#include "interfacemanager.h"
#include "interfacecontroldialog.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void interfaceControlAction();

private slots:
    void newInterface(PacketInterface*);
    void newData(PacketInterface *, QString);
    //void closeConnection();
    //void statusChanged(PacketInterface::Device_Status newState);


protected:
    void changeEvent(QEvent *e);

private:
    Ui::MainWindow *ui;
    InterfaceControlDialog *interfaceControlDialog;

    QTcpSocket tcpSocket;
    InterfaceManager interfaceManager;
    QString packetDisplay;
    int total_lines;
};

#endif // XASTIR_H
