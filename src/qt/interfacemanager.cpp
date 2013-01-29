#include "interfacemanager.h"

InterfaceManager::InterfaceManager(QObject *parent) :
    QObject(parent)
{
}

void InterfaceManager::addNewInterface(PacketInterface *iface)
{
    interfaces.append(iface);
    interfaceAdded(iface);
}
