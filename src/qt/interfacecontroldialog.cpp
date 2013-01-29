#include "interfacecontroldialog.h"
#include "ui_interfacecontroldialog.h"
#include "netinterfacepropertiesdialog.h"

InterfaceControlDialog::InterfaceControlDialog(InterfaceManager& manager, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InterfaceControlDialog),
    theManager( manager )
{
    ui->setupUi(this);

    for( int i = 0; i < theManager.numInterfaces(); i++) {
        PacketInterface *iface = theManager.getInterface(i);
        ui->listWidget->addItem( iface->deviceDescription());
        connect( iface, SIGNAL(interfaceChangedState(PacketInterface*,PacketInterface::Device_Status)),
                 this, SLOT(interfaceStatusChanged(PacketInterface*,PacketInterface::Device_Status)));
    }
    connect(&theManager,SIGNAL(interfaceAdded(PacketInterface *)), this, SLOT(managerAddedInterface(PacketInterface*)));

    connect(ui->startAllButton,SIGNAL(clicked()), this, SLOT(startAllAction()));
    connect(ui->stopAllButton,SIGNAL(clicked()), this, SLOT(stopAllAction()));
    connect(ui->propertiesButton, SIGNAL(clicked()), this, SLOT(preferencesAction()));
    connect(ui->stopButton, SIGNAL(clicked()), this, SLOT(stopAction()));
    connect(ui->startButton, SIGNAL(clicked()), this, SLOT(startAction()));
    connect(ui->listWidget, SIGNAL(currentRowChanged(int)), this, SLOT(selectedRowChanged(int)));
}

InterfaceControlDialog::~InterfaceControlDialog()
{
    delete ui;
}

void InterfaceControlDialog::addInterfaceAction()
{
    NetInterfacePropertiesDialog *dialog = new NetInterfacePropertiesDialog(theManager,
                                                       theManager.getNewNetInterface(), true, this);
    dialog->show();
}

void InterfaceControlDialog::startAllAction()
{
    for( int i = 0; i < theManager.numInterfaces(); i++) {
        theManager.getInterface(i)->start();
    }
}

void InterfaceControlDialog::stopAllAction()
{
    for( int i = 0; i < theManager.numInterfaces(); i++) {
        theManager.getInterface(i)->stop();
    }
}

void InterfaceControlDialog::startAction()
{
    int row = ui->listWidget->currentRow();
    theManager.getInterface(row)->start();
}

void InterfaceControlDialog::stopAction()
{
    int row = ui->listWidget->currentRow();
    theManager.getInterface(row)->stop();
}

void InterfaceControlDialog::preferencesAction()
{
    int row = ui->listWidget->currentRow();
    NetInterfacePropertiesDialog *dialog = new NetInterfacePropertiesDialog(theManager,
                                                       (NetInterface*)theManager.getInterface(row), false, this);
    dialog->show();
}

void InterfaceControlDialog::selectedRowChanged(int row)
{
    updateButtonsState();
}

void InterfaceControlDialog::managerAddedInterface(PacketInterface *newInterface)
{
    ui->listWidget->addItem(newInterface->deviceDescription());
    connect( newInterface, SIGNAL(interfaceChangedState(PacketInterface*,PacketInterface::Device_Status)),
             this, SLOT(interfaceStatusChanged(PacketInterface*,PacketInterface::Device_Status)));
    updateButtonsState();
}

void InterfaceControlDialog::interfaceStatusChanged(PacketInterface *iface, PacketInterface::Device_Status state)
{
    PacketInterface *testIface;
    for( int i = 0; i < theManager.numInterfaces(); i++) {
        testIface = theManager.getInterface(i);
        if(testIface == iface) {
            ui->listWidget->item(i)->setText(iface->deviceDescription());
        }
    }
    ui->listWidget->update();
    updateButtonsState();
}

void InterfaceControlDialog::updateButtonsState()
{
    int row = ui->listWidget->currentRow();
    if( (row>-1) && (row<theManager.numInterfaces())) {
        ui->deleteButton->setEnabled(true);
        ui->propertiesButton->setEnabled(true);
        if(theManager.getInterface(row)->deviceStatus() == PacketInterface::DEVICE_UP) {
            ui->startButton->setEnabled(false);
            ui->stopButton->setEnabled(true);
        } else {
            ui->startButton->setEnabled(true);
            ui->stopButton->setEnabled(false);
        }
    }
    else {
        ui->deleteButton->setEnabled(false);
        ui->propertiesButton->setEnabled(false);
        ui->startButton->setEnabled(false);
        ui->stopButton->setEnabled(false);
    }
}
