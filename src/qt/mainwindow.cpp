#include "ui_mainwindow.h"
#include "interfacecontroldialog.h"
#include "xastir.h"
#include <iostream>

using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    interfaceControlDialog = NULL;
    connect(&interfaceManager, SIGNAL(interfaceAdded(PacketInterface*)), this, SLOT(newInterface(PacketInterface*)));
 //   connect(&netInterface,SIGNAL(interfaceChangedState(PacketInterface::Device_Status)), this, SLOT(statusChanged(PacketInterface::Device_Status)));
 //   connect(&netInterface,SIGNAL(packetReceived(PacketInterface *, QString)), this, SLOT(newData(PacketInterface *,QString)));
    total_lines = 0;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::newInterface(PacketInterface *iface)
{
    connect(iface, SIGNAL(packetReceived(PacketInterface*,QString)), this, SLOT(newData(PacketInterface*,QString)));
}

void MainWindow::newData (PacketInterface *device, QString data)
{
    int max_lines = 15;
    QString tmp;


    packetDisplay.append(device->deviceName() + "-> " + data + "\n");

    if (total_lines >= max_lines) {
        int ii = packetDisplay.indexOf("\n");
        // Chop first line
        tmp = packetDisplay.right(packetDisplay.size() - (ii + 1));
        packetDisplay = tmp;
    }
    else {
        total_lines++;
    }
    ui->incomingPackets->setText(packetDisplay);
}

void MainWindow::interfaceControlAction()
{
    if( interfaceControlDialog == NULL) {
        interfaceControlDialog = new InterfaceControlDialog(interfaceManager, this);
    }
    interfaceControlDialog->show();
    interfaceControlDialog->raise();

}
