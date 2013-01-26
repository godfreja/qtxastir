#ifndef NETINTERFACE_H
#define NETINTERFACE_H

#include "packetinterface.h"
#include <QtNetwork>

class NetInterface : public PacketInterface
{
    Q_OBJECT

public:
    NetInterface();
    void start(void);
    void stop(void);
    QString getHostName(void);
    void setHostName(QString);
    QString setPortString(void);
    void setPortString(QString);
    QString getCallSign(void);
    void setCallsign(QString);
    QString getPasscode(void);
    void setPasscode(QString newPasscode);
    QString getFilter(void);
    void setFilter(QString newFilter);


protected:
    QTcpSocket tcpSocket;
    QString hostName;
    QString portString;
    QString callsign;  // Should this be in the base class??
    QString passcode;
    QString filter;

private slots:
    void connectToServer();
    void connectionClosedByServer();
    void error();
    void nowConnected();
    void closeConnection();
    void incomingData();

};

#endif // NETINTERFACE_H
