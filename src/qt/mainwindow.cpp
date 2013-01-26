#include "ui_mainwindow.h"
#include "xastir.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->disconnectServerButton->setEnabled(false);

    connect(ui->connectServerButton, SIGNAL(clicked()), this, SLOT(connectToServer()));
    connect(ui->disconnectServerButton, SIGNAL(clicked()), this, SLOT(closeConnection()));
    connect(&netInterface,SIGNAL(interfaceChangedState(PacketInterface::Device_Status)), this, SLOT(statusChanged(PacketInterface::Device_Status)));
    connect(&netInterface,SIGNAL(packetReceived(QString)), this, SLOT(newData(QString)));
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

void MainWindow::statusChanged(PacketInterface::Device_Status newState)
{
    if( newState == PacketInterface::DEVICE_UP)
    {
        ui->disconnectServerButton->setEnabled(true);
        ui->connectServerButton->setEnabled(false);
    }
    else
    {
        ui->disconnectServerButton->setEnabled(false);
        ui->connectServerButton->setEnabled(true);
    }
}

void MainWindow::connectToServer ()
{
    QString host = ui->InternetHost->text();
    QString portStr = ui->InternetPort->text();
    netInterface.setHostName(ui->InternetHost->text());
    netInterface.setPortString(ui->InternetPort->text());
    netInterface.setCallsign(ui->Callsign->text().toUpper());
    netInterface.setPasscode(ui->Passcode->text());
    netInterface.setFilter(ui->Filter->text());
    netInterface.start();

#if 0
    // Send a posit to the server
    if (callsign.startsWith("WE7U")) {
        tcpSocket.write( "WE7U>APX201:=/6<A5/VVOx   SCVSAR\r\n" );
    }
#endif
}

void MainWindow::newData (QString data)
{
    int max_lines = 15;
    QString tmp;


    packetDisplay.append(data + "\n");

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

void MainWindow::closeConnection ()
{
    netInterface.stop();
}

