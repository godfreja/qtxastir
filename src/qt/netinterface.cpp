#include "netinterface.h"

NetInterface::NetInterface()
{
    connect(&tcpSocket, SIGNAL(connected()), this, SLOT(nowConnected()));
    connect(&tcpSocket, SIGNAL(disconnected()), this, SLOT(connectionClosedByServer()));
    connect(&tcpSocket, SIGNAL(readyRead()), this, SLOT(incomingData()));
    connect(&tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(error()));
}

void NetInterface::start()
{
    connectToServer();
}

void NetInterface::stop()
{
    closeConnection();
}


QString NetInterface::getHostName()
{
    return hostName;
}

void NetInterface::setHostName(QString newHostName)
{
    hostName = newHostName;
}

QString NetInterface::setPortString()
{
    return portString;
}

void NetInterface::setPortString(QString newPortString)
{
    portString = newPortString;
}

QString NetInterface::getCallSign()
{
    return callsign;
}

void NetInterface::setCallsign(QString newCallsign)
{
    callsign = newCallsign;
    // XXX Need to update on server if we are connected
}

QString NetInterface::getPasscode()
{
    return passcode;
}

void NetInterface::setPasscode(QString newPasscode)
{
    passcode = newPasscode;
    // XXX Need to update on server if we are connected
}

QString NetInterface::getFilter()
{
    return filter;
}

void NetInterface::setFilter(QString newFilter)
{
    filter = newFilter;
    // XXX Need to update on server if we are connected
}

void NetInterface::connectToServer()
{
    int port = portString.toInt();
    tcpSocket.connectToHost(hostName, port);

}

void NetInterface::connectionClosedByServer()
{
    deviceState = DEVICE_DOWN;
    closeConnection();
}

void NetInterface::error()
{
    deviceState = DEVICE_ERROR;
    closeConnection();
}

void NetInterface::nowConnected()
{
    // Send authentication and filter info to server.
    // Note that the callsign is case-sensitive in javaAprsServer
    QString callsign = this->callsign.toUpper();
    QString loginStr;
    if (filter.size() > 0) {
        loginStr = "user " + callsign + " pass " + passcode + " vers XASTIR-QT 0.1 filter " + filter + "\r\n";
    }
    else {
        loginStr = "user " + callsign + " pass " + passcode + " vers XASTIR-QT 0.1 \r\n";
    }
    tcpSocket.write( loginStr.toAscii().data() );

    // Update interface status
    deviceState = DEVICE_UP;
    interfaceChangedState(deviceState);
}

void NetInterface::closeConnection()
{
    if(deviceState==DEVICE_UP) deviceState = DEVICE_DOWN;
    tcpSocket.close();
    interfaceChangedState(deviceState);
}

void NetInterface::incomingData ()
{
    QTextStream in(&tcpSocket);
    QString data;

    forever {
        if (!tcpSocket.canReadLine()) {
            break;
        }
        data = in.readLine() + "\n";
        packetReceived(data);
    }
}
