#ifndef INTERFACEMANAGER_H
#define INTERFACEMANAGER_H

#include <QObject>
#include "packetinterface.h"
#include "netinterface.h"

class InterfaceManager : public QObject
{
    Q_OBJECT
public:
    explicit InterfaceManager(QObject *parent = 0);
    int numInterfaces() {return interfaces.count();}
    PacketInterface* getInterface(int i) {return interfaces[i];}
    NetInterface* getNewNetInterface() { return new NetInterface(interfaces.count()); }
    void addNewInterface( PacketInterface *iface );


signals:
    void interfaceAdded(PacketInterface *newInterface);

public slots:
    
private:
    QList<PacketInterface*> interfaces;
};

#endif // INTERFACEMANAGER_H
