/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id: main.c,v 1.810 2012/09/23 16:19:22 tvrusso Exp $
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2013  The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

#include "netinterface.h"

NetInterface::NetInterface(int ifaceNum)
{
    connect(&tcpSocket, SIGNAL(connected()), this, SLOT(nowConnected()));
    connect(&tcpSocket, SIGNAL(disconnected()), this, SLOT(connectionClosedByServer()));
    connect(&tcpSocket, SIGNAL(readyRead()), this, SLOT(incomingData()));
    connect(&tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(error()));
    interfaceNumber = ifaceNum;
}

void NetInterface::start()
{
    if( deviceState == DEVICE_UP ) {
        return;
    }
    connectToServer();
}

void NetInterface::stop()
{
    closeConnection();
}

QString NetInterface::deviceName()
{
    return QString::number(interfaceNumber) + tr(": Net");
}

QString NetInterface::deviceDescription()
{
    QString text = tr("Device ") + QString::number(interfaceNumber) +
            tr(" Internet Server Connected to ") + hostName + tr( " and is ");

    if(deviceState == DEVICE_UP) text += tr("UP");
    else text += tr("DOWN");

    return text;
}

QString NetInterface::getHostName()
{
    return hostName;
}

void NetInterface::setHostName(QString newHostName)
{
    hostName = newHostName;
}

QString NetInterface::getPortString()
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
    interfaceChangedState(this, deviceState);
}

void NetInterface::closeConnection()
{
    if(deviceState==DEVICE_UP) deviceState = DEVICE_DOWN;
    tcpSocket.close();
    interfaceChangedState(this, deviceState);
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
        packetReceived(this, data);
    }
}
