#ifndef INTERFACECONTROLDIALOG_H
#define INTERFACECONTROLDIALOG_H

#include <QDialog>
#include "interfacemanager.h"

namespace Ui {
class InterfaceControlDialog;
}

class InterfaceControlDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit InterfaceControlDialog(InterfaceManager& manager, QWidget *parent = 0 );
    ~InterfaceControlDialog();
    
public slots:
    void addInterfaceAction();
    void startAllAction();
    void stopAllAction();
    void startAction();
    void stopAction();
    void preferencesAction();
    void selectedRowChanged(int row);
    void managerAddedInterface(PacketInterface *newInterface);
    void interfaceStatusChanged(PacketInterface *iface, PacketInterface::Device_Status state);

private:
    Ui::InterfaceControlDialog *ui;
    InterfaceManager& theManager;
    void updateButtonsState();
};

#endif // INTERFACECONTROLDIALOG_H
